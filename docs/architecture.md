# EmoPlay 架构设计

> 对应总体规划见 `mpv.md`（P0–P10）。本文档记录已实施和已定的架构决策，随阶段推进持续更新。

## 分层总览

```
┌─────────────────────────────────────────────────────────┐
│ UI 层 (QML)                                             │
│   Main.qml — 播放器窗口、控制栏、快捷键、文件对话框        │
│             音轨/字幕抽屉、播放列表抽屉、最近播放          │
├─────────────────────────────────────────────────────────┤
│ 播放器服务层 (C++ / Qt)                                  │
│   MpvItem   — QQuickItem 视频渲染控件（状态镜像给 QML）   │
│   MpvEngine — libmpv 客户端句柄封装（命令/属性/事件桥）    │
│   MpvConfigParser — mpv.conf / input.conf 纯逻辑解析      │
│   HistoryStore — 播放历史（JSON 持久化，≤50 条）          │
├─────────────────────────────────────────────────────────┤
│ 引擎适配层 (libmpv render API)                           │
│   mpv_render_context + QQuickFramebufferObject (OpenGL)  │
└─────────────────────────────────────────────────────────┘
```

## 模块与职责

| 模块 | 文件 | 职责 |
|---|---|---|
| 引擎封装 | `src/mpv/MpvEngine.{h,cpp}` | 拥有 `mpv_handle`；初始化（vo=libmpv, hwdec=auto-safe, keep-open；用户选项应用后**重申** vo/osc/terminal）；`command/setProperty/getProperty/loadFile`、`toMediaPath`（file://→本地路径）；`keyPress`（mpv `keypress` 命令，P4 按键转发）与 `mpvKeyName`（Qt 键→mpv 键名）；wakeup 回调→GUI 线程事件泵；观察属性（time-pos/duration/pause/volume/mute/media-title/track-list/playlist/idle-active；P5 增 video-params(NODE)/container-fps(DOUBLE)/frame-drop-count(NODE int64)/current-vo/video-format(STRING)）→Qt 信号 |
| 渲染控件 | `src/mpv/MpvItem.{h,cpp}` | `QQuickFramebufferObject` 子类；render 线程持有 `mpv_render_context`；把引擎状态（含 trackList/playlist，P5 增 videoWidth/videoHeight/fps/droppedFrames/hdrActive 镜像与 2s 掉帧率采样 QTimer）镜像为 Q_PROPERTY；提供 play/pause/seek、音轨/字幕选择（set aid/sid）、`sub-add` 外挂字幕、loadfile append-play / playlist-next/prev / playlist-play-index / playlist-clear 等命令方法 |
| 配置解析 | `src/config/MpvConfigParser.{h,cpp}` | 解析 `mpv.conf`（无行内注释、profile 段）与 `input.conf`（引号分词、空白后 # 才是注释）；纯逻辑、可单元测试；P4 兼容层的数据基础 |
| 播放历史 | `src/history/HistoryStore.{h,cpp}` | QObject 服务；engine `fileLoaded` 时记录 path/title/duration，5 秒 QTimer 从 `time-pos` 刷新当前条目 position；按 path 去重置顶、上限 50 条；`AppDataLocation/history.json`（QSaveFile 原子写入），QML 经 `entries` 属性读取 |
| 画质决策 | `src/quality/QualityProfileEngine.{h,cpp}` | 纯决策表（无 mpv/GUI/IO，全部单测）：MediaInfo（宽高/fps/animated 提示/HDR 提示/可用着色器）+ 用户偏好（contentMode/tier/tone-mapping/scale/deband）→ mpv 选项映射 + 着色器列表；全部选项名/值对照 mpv v0.37.0 文档与源码核实；Anime4K 组名/文件链对照上游 v4.0.1 GLSL_Instructions |
| 画质控制 | `src/quality/QualityController.{h,cpp}` | 把决策表结果应用到运行中的引擎（标量选项走 setProperty、着色器列表走 `change-list glsl-shaders clr/append` 命令）；启动时收编/挂载持久化着色器清单；tier=off 且从未启用时不写引擎（不碰导入配置）；掉帧率超阈值把**当前生效档位**降为 low（会话级，持久化用户档位不动，下个文件恢复）；照 HistoryStore 模式：信号驱动 + 按需读属性 |
| 配置扫描 | `src/compat/MpvProfileScanner.{h,cpp}` | 定位用户 mpv 配置目录（对齐 mpv osdep/path-unix.c 语义：Linux/macOS 设了 `$XDG_CONFIG_HOME` 时**只用** `$XDG_CONFIG_HOME/mpv`，否则 `~/.config/mpv`，旧 `~/.mpv` 仅在主候选不存在时兜底；Windows `%APPDATA%/mpv`；base-dir 可注入，测试不碰真实 HOME）；`scan()` 产出 `ImportSummary`：逐文件条目数（复用 MpvConfigParser）、脚本/着色器/字体计数、字节数 |
| 导入规则 | `src/compat/MpvImportRules.{h,cpp}` | 把解析出的 mpv.conf 条目变成 `initialize()` 选项映射：强制 vo=libmpv/osc=no/terminal=no；剥离 include、profile、scripts、input-conf、config-dir、写入路径与转储类选项；写入路径重定向到 EmoPlay AppData；其余选项原样通过；全部动作留审计行 |
| 键名映射 | `src/compat/MpvKeyName.{h,cpp}` | Qt 按键事件→mpv 键名（可打印文本优先、命名键表、ctrl/alt/meta/shift 组合）；不可映射返回空串。支撑 P4 input.conf 绑定在嵌入 UI 内生效 |
| 兼容控制 | `src/compat/CompatController.{h,cpp}` | 读 settings 决策→重扫→组装 `initialize()` 选项映射（full/selective 走导入规则、导入 input.conf 时指向用户原文件、scripts 按平台路径分隔符显式列表、fonts-dir/config-dir 指向用户目录）；未导入 input.conf 的一切情形（未决策/skip/未勾选/standalone）都指向 EmoPlay 自有空 input.conf 并经 `errorOccurred` 上报创建失败；向导三选一动作持久化决策；`settings()` 暴露共享 AppSettings 实例 |
| 应用设置 | `src/settings/AppSettings.{h,cpp}` | `AppDataLocation/settings.json`（QSaveFile 原子写入）：导入决策（full/selective/standalone/skip）、选择性文件清单、上次 ImportSummary、dontAskAgain、决策时间戳；P5 quality 节（contentMode/qualityTier/toneMappingMode/scaleOverride/debandMode/manualShaderList）；损坏文件回退默认值 |

## 关键决策与理由

1. **Qt6/QML + libmpv 动态链接**（P0 选型结论，详见 `tech-selection.md`）。
2. **视频嵌入用 QQuickFramebufferObject + mpv render API**：这是 mpv 官方示例（mpv-examples/libmpv/qml）验证过的模式；render context 在 render 线程的 GL context 上创建，`MPV_RENDER_PARAM_FLIP_Y=1` 配合 Qt 的 FBO 镜像合成。相比 `QQuickRenderControl`/自绘 render node，FBO 方案实现面最小、可回滚。
3. **单一 MpvEngine 实例**：`MpvEngine::instance()` 静态可达，`MpvItem` 据此取 handle。播放状态只有一份权威来源（mpv 本体）；UI 写操作只下发请求，读状态一律来自观察属性事件回显，避免双向绑定打架。
4. **线程模型**：libmpv client API 线程安全；事件通过 `mpv_set_wakeup_callback` → `QMetaObject::invokeMethod(queued)` 泵到 GUI 线程；render context 严格在 Qt Quick render 线程创建/使用/销毁（`mpv_render_context_create/free`）。
5. **销毁顺序契约**：`main.cpp` 中 `MpvEngine` 声明在 `QQmlApplicationEngine` 之前，栈回收时 QML 对象（含 render context）先于 mpv core 析构。
6. **配置解析是纯函数库**：无 mpv、无 GUI 依赖，直接单测。P4 兼容层在此之上做扫描/导入/回写。
7. **播放历史是被动服务**：只订阅 `MpvEngine` 信号（`fileLoaded` / `idleActiveChanged`）并按需 `getProperty`（path / media-title / duration / time-pos），不缓存播放状态；存储路径可注入以便测试（`QStandardPaths::setTestModeEnabled` 下 headless 可测），每次变更即 QSaveFile 原子落盘。

## QML 状态同步模式（重要约定）

QML 中 Slider 等控件的值用 **命令式同步**（`Connections { function onXxxChanged() }`）而不是声明式绑定：用户拖动会对 `value` 做命令式写，声明式绑定会被一次性销毁导致后续失联。新增进度/音量类控件时遵循同一模式。

## 画质引擎（P5 v1）

把 mpv-Emo adaptive-quality 的语义做成原生决策表（简化 v1，见
`docs/nativeization-plan.md` §2.1）。分层与 P4 相同：纯逻辑决策 +
薄控制器 + QML 面板。

### 决策表（QualityProfileEngine）

输入 `MediaInfo{width,height,fps,vfFormat,isHdr,availableShaders}` 与
用户偏好 `contentMode/qualityTier/toneMappingMode/scaleOverride/debandMode`：

| 条件 | 输出 |
|---|---|
| tier=off | scale=lanczos（mpv 默认）、deband=no、tone-mapping=auto、target-colorspace-hint=no、着色器清空 |
| tier=high + anime（auto 模式按 animated 提示判定） | scale=ewa_lanczossharp + 按分辨率挂 Anime4K 组：SD(<900p)→A+A、HD(≥900p)→A、UHD(≥1440p)→C（Fast 变体链，文件名对照上游 v4.0.1；只挂 `availableShaders` 中实际存在的文件） |
| tier=high + live | scale=ewa_lanczossharp + 可选 SSimSuperRes 组 |
| tier=medium / low | 只保留 scale（medium=ewa_lanczossharp，low=lanczos），不挂组；deband 开 |
| fps≥50 | 保留 scale，去掉 shader 组（HFR 避让） |
| HDR 源 | 不挂 shader；tone-mapping 按显式用户选择，否则 tier 映射（high/medium→bt.2390、low→mobius）+ target-colorspace-hint=yes（v0.37 下为 gpu-next 专属、实际 no-op，仍设置以对齐未来管线） |

### mpv 0.37 核实结论（写入代码注释）

- `--scale` 默认 `lanczos`；`ewa_lanczossharp` 为合法选项（options.rst）。
- `tone-mapping` 的 `spline`/`bt.2446a`/`st2094-40` 是 **gpu-next 专属**；
  `vo=libmpv` 走经典 gpu 渲染器（vo_libmpv.c `render_backends[]` 只有
  `render_backend_gpu`/`render_backend_sw`），故 UI 只提供
  auto/bt.2390/mobius/hable。
- `glsl-shaders` 是 path-list 选项；运行时变更走 `change-list glsl-shaders
  append/clr` 命令（input.rst change-list；player/command.c）。
- `MpvEngine::setProperty` 的字符串路径必须传 `char*` 的**地址**（client.c
  `mpv_set_property_string` 即 `mpv_set_property(..., &data)`）——评审发现
  的历史 bug，字符串属性会直接段错误，已修复并加回归测试。

### 应用路径与让位

- 控制器按 HistoryStore 模式工作：`fileLoaded`/掉帧信号驱动 + 按需读属性；
  着色器最终列表 = 手动清单（面板勾选，绝对路径，持久化）∪ 引擎选择。
- 首次启动把 P4 导入配置已挂载的 `glsl-shaders` 收编进手动清单（面板成为
  唯一事实来源）；其后任何应用都是 clr+append。
- **inert 规则**：tier=off 且本次会话从未启用过引擎/着色器时，不向引擎写
  任何东西——导入的渲染选项原样生效。从启用切回 off 时按决策表重置到 mpv
  默认值（已知简化：不恢复用户导入的旧值，adaptive-quality 的快照语义留
  给后续批次）。
- 着色器目录按导入决策选择：full/selective 导入时用用户配置目录的
  `shaders/`（只读扫描），否则用 AppData `shaders/`。

### 掉帧自动降档

MpvItem 内 2s QTimer 采样镜像的 `frame-drop-count`，把增长率（帧/秒）经
QML 桥发给 QualityController；≥2 帧/秒且当前档位为 medium/high 时把
**当前生效档位**降为 low 并置 `autoDowngraded`（面板内提示条，不弹窗）。
持久化的用户档位不动，下个文件加载时恢复并重新计测。

### 持久化（settings.json `quality` 节）

`contentMode`（auto）、`qualityTier`（off）、`toneMappingMode`（auto）、
`scaleOverride`（""）、`debandMode`（auto）、`manualShaderList`（[]）。
与 P4 导入决策同文档共存，QSaveFile 原子写。

### 已知限制（P5 v1 未含项）

- RIFE/VS 补帧、ICC 自动加载、着色器商店（P8 上游资产分发）不在 v1。
- 无 GPU 档位检测（Windows D3D11 适配器探测等）与压制组词库（用固定关键
  字表启发）。
- tier=off 的「恢复默认」重置的是 mpv 默认值而非用户导入值（见上）。
- `change-list glsl-shaders clr` 会连带清掉脚本运行期挂的着色器（引擎与
  面板是列表的唯一管理者；第三方脚本让位语义留待 P8）。

## 配置兼容层（P4）

把用户既有的 mpv 配置（mpv.conf / input.conf / profiles.conf / platform.conf /
script-opts / scripts / shaders / fonts）接入 EmoPlay。两条用户原则：

1. **绝不自动覆盖用户原配置**：任何模式都不写用户的 mpv 目录。导入只把 mpv 的读路径
   指向用户文件（`config-dir`、`input-conf`、`fonts-dir`、显式脚本文件列表），写入路径
   全部重定向到 EmoPlay 自己的 AppData。
2. **嵌入约束不可被用户配置覆盖**：`vo=libmpv`、`osc=no`、`terminal=no` 强制生效。

### 启动序（main.cpp）

```
CompatController::startupOptions()      # 读 settings.json → 重扫 → 组装选项映射
MpvEngine::initialize(options)          # 内置默认 → 用户选项 → 重申 vo/osc/terminal
QML 首启向导（检测到配置且未决策时）      # 选择持久化，导入的引擎选项下次启动生效
```

`initialize()` 里在用户选项**之后**重申三个强制值属于纵深防御：即使导入规则有 bug，
嵌入也不会被 mpv.conf 破坏。

### 导入规则（MpvImportRules）

| 类别 | 键 | 处置 |
|---|---|---|
| 强制 | `vo` / `osc` / `terminal` | 替换为 libmpv / no / no，留审计行 |
| 剥离 | `include` | 任意文件加载会绕过审计，直接丢弃 |
| 剥离 | `profile` | P4 不应用 profile（含 profiles.conf 场景段），报告名称 |
| 剥离 | `scripts` / `input-conf` / `config-dir` | 由 CompatController 统一提供 |
| 剥离 | `watch-later-dir` / `screenshot-directory` / `gpu-shader-cache-dir` / `icc-cache-dir` / `demuxer-cache-dir` | 用户值丢弃，重定向到 `AppData/{watch_later,screenshots,shader_cache,icc_cache,demuxer_cache}` |
| 剥离 | `screenshot-template`（含 `~~` 时） | `~~` 展开会写进用户目录；纯文件名模板保留 |
| 剥离 | `dump-stats` / `log-file` | 避免向用户指定路径写文件 |
| 通过 | `hwdec` / `volume` / `keep-open` / `slang` 等 | 原样导入，后出现的重复项覆盖（同 mpv 语义） |
| 通过 | 值含 `~~` 的读路径（`glsl-shaders`、`icc-profile` 等） | 保留，由 `config-dir` 解析到用户原位置，不复制文件 |

### profiles.conf 与 mpv.conf 内嵌 profile 的关系

两者是**同一机制**：mpv 的 `[section]` 语法，profiles.conf 只是 profile 定义的
include 式拆分（mpv-Emo 用它放场景模式，mpv.conf 放默认启用的 `profile=` 引用）。
解析上二者共用 `MpvConfigParser::parseMpvConf`。P4 行为：profile 段内的选项不应用
（libmpv 无法通过 `mpv_set_option_string` 定义 profile），段名与 `profile=` 引用被
剥离并计入审计；`[网络流]`/`[直播]` 这类 `profile-cond` 自动 profile 同样不生效。

### input.conf 与按键转发

`input-conf` 选项直接指向用户原 input.conf（不复制）。嵌入 UI 下 mpv 收不到键盘
事件，转发路径：QML 中未被 EmoPlay 自身 Shortcut/控件消费的按键 →
`MpvEngine::mpvKeyName()`（Qt 键→mpv 键名）→ `MpvEngine::keyPress()`（mpv
`keypress` 命令）→ mpv 输入层。挂载点：视频区焦点持有项（Main.qml `compatKeys`）、
底部控制栏、两个抽屉。libmpv 默认键位是关闭的，转发只触发用户绑定与脚本注册的
绑定；已知的未覆盖面：焦点在 Slider 上时方向键由 Slider 自行消费（属 EmoPlay 交互），
不会转发。

### 脚本与 script-opts / shaders / fonts

- `scripts` 选项 = **路径分隔符**连接的绝对路径显式列表——Unix `:`、Windows `;`
  （mpv 的 `--scripts` 是 OPT_PATHLIST，见 options.rst `--scripts=file1.lua:file2.lua:...`，
  不是普通列表选项的 `,`）。含分隔符字符的脚本路径不受支持，记录为限制。
- `load-scripts` 在 `MpvEngine::initialize` 内置默认中强制 `no`：mpv 默认 yes，
  会在 libmpv 下自动加载 config 目录 `scripts/` 子目录的全部脚本
  （player/scripting.c `mp_load_scripts`）；脚本只经显式 `scripts` 列表进入
  （`load-scripts=no` 只关目录自动加载，不关显式列表）。
- `config-dir` 指向用户 mpv 目录（只读用途）：脚本内 `~~` 路径、`mp.find_config_file`
  的 script-opts 查找、mpv.conf 里 `~~/shaders/...` 这类值都解析到用户原位置。
- `fonts-dir` 指向用户 `fonts/` 目录。
- 已知边界：第三方脚本自身往用户目录写数据（如 manager.lua 的 `.manager/`）是用户
  原配置的既有行为，不经 EmoPlay 写路径重定向，由用户决策是否导入该脚本。

### 决策持久化与向导

`AppDataLocation/settings.json`（AppSettings，QSaveFile 原子写）记录
`importDecision`（full / selective / standalone / skip）、选择性文件清单、上次
`ImportSummary` 快照、`dontAskAgain`。首启检测到配置且未决策时弹
`ImportWizard.qml`：完整导入 / 选择性导入（按文件复选）/ 独立配置，另有
「不再询问」+「Not now」。

**input-conf 恒定受控**：mpv 在 `mpv_initialize` 内无条件加载 input.conf
（player/main.c `mp_input_load_config`），input-conf 未设或打不开时回退到 config
目录的 input.conf（input/input.c），且不受 libmpv `config=no` 门控。因此只要用户
未显式导入 input.conf（未决策 / skip / 选择性导入未勾选），`input-conf` 一律指向
EmoPlay 自有的空 input.conf（AppData 下按需创建，创建失败经 `errorOccurred`
上报——否则 mpv 会静默回退加载用户文件）。重跑导入入口：
`compatController.requestWizard()`（设置页后续阶段接）。

### 已知限制

- profile 定义与 `profile=` 引用不应用（见上）；画质 profile 的原生等价物在 P5。
- 用户 mpv.conf 里比 libmpv 0.37 新的选项名会被 `mpv_set_option_string` 静默拒绝
  （按选项逐条忽略，不影响其余导入）。
- GUI 键位编辑器后续阶段做（P4 只加载并使 input.conf 生效）。

## 已知限制 / 待改进（记入 roadmap）

- 渲染后端目前固定走 OpenGL render API；Vulkan/D3D/Metal 直通（P5+）。渲染管线是经典 gpu（vo=libmpv 限制），gpu-next 专属能力（spline/bt.2446a tone-mapping 等）不可用。
- 音量 UI 语义：mpv volume 可 >100，滑条钳制 0–100，快捷键可到 130（待统一）。
- 播放列表只存在于 mpv 会话内，未持久化到磁盘（roadmap P3 原文含「持久化」，本批次未包含，待后续批次）；历史记录按上次 position 点击回放，暂不支持断点续播选项。
- 配置兼容层（P4）已实现扫描/导入/使 input.conf 与脚本生效；GUI 键位编辑器与设置页入口待后续批次。
- 画质引擎（P5 v1）已落地决策表/应用路径/面板/持久化；未含项与简化见「画质引擎（P5 v1）」一节。
