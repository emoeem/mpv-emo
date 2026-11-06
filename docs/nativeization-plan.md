# EmoPlay 原生化计划（mpv-Emo → EmoPlay 能力映射）

> 背景：用户已有深度定制的 mpv 配置仓库 [emoeem/mpv](https://github.com/emoeem/mpv)（mpv-Emo），
> 大量能力以 Lua/JS 脚本 + 着色器 + VapourSynth 预设实现。EmoPlay 的方向是把脚本受限的能力
> 升级为程序原生功能，其余进入 P8 插件体系。本文给出逐项映射与优先级，作为 P5–P9 排期依据。

## 0. 方法与约束

- 参考仓库以浅克隆方式只读检阅，固定快照 `70d6eb4`（2026-10-31，README 重塑版）。检阅范围：
  `README.md`、`mpv.conf`、`input.conf`（379 条 `#menu:` 菜单注解）、`profiles.conf`、
  `platform.conf`、`manager.json`、`deploy.py`、`scripts/`（80 项）、`script-opts/`（70 项）、
  `script-modules/`、`shaders/`（34 个子目录）、`vs/`（15 个 .vpy）。
- mpv-Emo 整体为 **LGPL-3.0**（第三方组件保留各自许可证：uosc GPL-3.0、Anime4K MIT/LGPL、
  hdr-toys MIT 等）。本计划**只借鉴设计与行为语义，不复制任何代码、着色器、字体、ICC、模型等资源**；
  涉及第三方来源的资产由 P8 以其原始许可证重新获取（见 §4.3）。
- 文中 `scripts/…`、`script-opts/…` 等路径均指 mpv-Emo 仓库内文件。
- 落点六类：**P5 画质引擎 / P6 媒体库 / P7 音乐模式 / P8 插件仓库 / P9 平台集成 / 保留为脚本**
  （保留为脚本 = 经 P4 兼容层继续加载 mpv 脚本，或成为 P8 插件）。
  约定：设置持久化、键位、窗口行为等「播放器设置中心」类归 P9；历史/书签/文件管理类归 P6。
- 遵守 `docs/architecture.md` 既有决策：播放状态不缓存（决策 7）、QML 命令式同步。

## 1. 处置策略总览

| 处置 | 判据 | 代表组件 |
|---|---|---|
| **原生化** | 需要深度访问引擎/窗口/文件系统，脚本侧 hack 多或性能受限 | content-adaptive、HDR 双引擎、EQ、dynamic-crop、file-browser、history/断点续播 |
| **插件化（P8）** | 能力本身是第三方服务/资产，边界清晰 | 弹幕源、字幕源、whisper、上游着色器包、manager 更新协议 |
| **保留为脚本** | mpv 脚本已足够好，与引擎耦合低 | evafast、simple-mpv-webui、trakt-scrobble、sleep-timer 等长尾 |

## 2. 映射总表

优先级：高 = P5 起步批次或 README 头条特性；中 = 次批次；低 = 长尾/机会性。
「依赖」列中 `属性`/`命令` 指 libmpv client API 能力，`P5.x` 等指本表行号组。

### 2.1 画质与渲染（P5 画质引擎）

| 脚本/组件 | 现状（做什么） | EmoPlay 落点 | 优先级 | 依赖 |
|---|---|---|---|---|
| **adaptive-quality**（三件套：`scripts/adaptive-quality.lua` + `scripts/content-adaptive.lua` + `script-opts/adaptive_quality.conf`） | 内容自适应画质引擎：按 GPU 档位（auto/low/balanced/medium/high，Windows/D3D11 检测适配器）× 内容类型（动画/真人，按压制组词库判定）× 分辨率（SD/HD/4K）× 帧率，在文件加载时自动挂载超分着色器组；掉帧自动降档（重型→轻量→仅色度）；HDR 源与直播源避让；用户手动改 `glsl-shaders` 时自动让位、下个文件恢复；uosc 菜单四档（关闭/自动/性能/画质）并持久化 | **P5**：原生「自适应画质引擎」——C++ 侧按 `track-list`/`video-params`/`estimated-vf-fps`/掉帧属性决策，通过 glsl-shaders 列表命令挂载；档位与让位状态持久化到应用设置 | 高 | 着色器注册表（§2.1 第 4 行）；性能采样属性（`frame-drop*`、`estimated-vf-fps`、`video-params`）；设置持久化（P9） |
| **live-adaptive-quality.lua**（live 变体） | Linux 直播自适应：不猜带宽，读 demuxer 缓存 runway/underrun 信号 + resolver 档位表，限频升降档（`low_cache_seconds=5`、`max_switches_per_file=8` 等，见 `script-opts/live_adaptive_quality.conf`）；与 `online-media.lua` 的质量阶梯联动 | **P5**（联动 P9 网络播放）：原生直播画质控制器——观察缓存属性、调用播放器「切流质量」接口 | 中 | demuxer 缓存属性（`demuxer-cache-state`）；在线媒体解析与质量阶梯（P9，`online-media` 等价物）；本表 adaptive-quality 行 |
| **hdr-pipeline + hdr-toys 双引擎**（`scripts/hdr-pipeline.lua` + `shaders/hdr-toys/`） | HDR 处理链双模式：`native`（gpu-next 原生直通 + libplacebo tone-mapping）与 `toys`（hdr-toys 着色器链按片源选用 clip→pq_inv/hlg_inv→astra→bottosson→bt1886）`ALT+H` 互斥切换；进 toys 快照 `tone-mapping`/`gamut-mapping-mode`/`target-*`/`icc-*` 并守护属性不被其他写入方改回；非 BT.2020 自动回退原生链并提示；上游 `astra.glsl` 有本地补丁（GLSL 保留字 `packed`），manager 以 `check`+`baseline b43e457` 管理 | **P5**：原生「HDR 引擎选择器」——模式切换、属性快照/守护、片源探测（`video-params/primaries+transfer`）内置；hdr-toys 着色器本体作为 P8 上游 git 资产引入，不内嵌 | 高 | glsl-shaders 列表操作命令；渲染后端参数读写（`tone-mapping`、`target-*`、`icc-*`）；P8 上游资产行（hdr-toys） |
| **shaders/ 分类法（厂商×技法）** | `shaders/` 下 34 个子目录按两维组织：**厂商**（`AMD/` CAS/FSR、`NV/` NVScaler/NVSharpen、`QCOM/`、Intel CMAA2 归入 `AA/`）×**技法家族**（AI 超分：`Anime4K/`（Restore/Upscale/Deblur/Denoise/Thin/AIO）、`Ani/`+`Ani4k/` ArtCNN、`AnimeJaNai/`、`CuNNy/` 含 `dp4a/` 量化、`FSRCNNX/`、`ravu/`、`nnedi3/`、`SSim/`；抗锯齿 `AA/`；去色带 `Deband/`；色度 `DPID/`、`CfL/`（KrigBilateral）；锐化 `USM/`、`WarpSharp/`、`guided/`、`Adaptive_sharpen/`；插值 `EDI/`；降噪 `nlmeans/`；去交错 `deint/`；色彩 `color/`、`eq/`、`LUT/`；几何变换 `Canvas/`、`mirror/`；氛围 `ambient/`；HDR `hdr-toys/`；VR `mpv360.glsl`；杂项 `other/` 等）。手动热键 `CTRL+0~9`/`ALT+1~8` 与 profile 整列表替换共存 | **P5**：着色器注册表——每条着色器一项元数据（名称、来源厂商/家族、技法类别、开销档位、适用内容类型、来源出处），供自适应引擎决策与 UI 菜单生成；文件本体不入仓库，走 P8 资产分发 | 高 | P8 资产分发（§4.3）；mpv.conf profile 等价物（P5 画质预设 UI） |
| **vs/ VapourSynth 预设 + `rife.lua` / `ai-interpolation.lua`** | `vs/` 15 个 .vpy：补帧 `MEMC_RIFE_NV/STD/DML`、`MEMC_SVP_PRO`、`MEMC_MVT_LQ`、`MEMC_DRBA_NV`；超分 `SR_ACNET_STD`、`SR_ARTCNN_NV`；降噪 `NR_BM3D_NV`、`NR_CCD_STD`；去交错 `ETC_DEINT_EX`；混合 AI `MIX_UAI_DML/MIGX/NV_TRT`、`MIX_UVR_MAD`。脚本侧按 `script-opts/rife.conf`、`ai_interpolation.conf` 决策；`adaptive_quality.conf` 定义安全回退（核显、DV/HLG/HDR Vivid、ISO/VFR、高帧率、倍速、实时过载时不启用）；README 明确 AI 补帧「仅记录意图」当运行时未装 | **P5**：外部处理器适配器——VapourSynth 进程托管 + 预设库（RIFE/SVP/ArtCNN/BM3D 语义）+ 准入检查器（HDR/VFR/高帧率/性能档位）；无运行时降级为原生档 | 中 | VapourSynth + vsmlrt + TensorRT/ncnn 后端与模型文件（外部依赖，`deploy.py` 四态中的 `degraded` 项）；子进程管理与生命周期（P9） |
| **dynamic-crop.lua** | lavfi `cropdetect` 连续检测黑边并插入 crop 滤镜，缓存可信值加速变化；只对视频流工作；要求 `-copy` 硬解变体；起播尺寸未就绪自动重试 | **P5**：原生去黑边——`vf cropdetect` 注入 + 结果解析 + `vf crop` 应用，C++ 直读滤镜日志回调 | 高 | libmpv vf 滤镜链命令与 log-message 回调；hwdec 策略（P5） |
| **mpv360 + `shaders/mpv360.glsl`** | 360° 全景：equirectangular / 双鱼眼 / dual half-equirect / half-equirect / cylindrical / EAC 投影重映射，键鼠自由视角，VR 激活期间才注册键位；进入 append `mpv360.glsl`、退出 remove；`script-opts/mpv360.conf` 配参数；manager `check` 模式且配套 glsl 不随上游替换 | **P5**：原生 VR 视图——投影选择、视角状态机、输入捕获用 Qt 实现，投影着色器走 P8 资产；键位仅 VR 模式生效的原则保留 | 中 | glsl-shaders append/remove；鼠标/键盘事件重定向（QML 层）；P8 上游资产（mpv360） |

### 2.2 媒体库与文件（P6 媒体库）

| 脚本/组件 | 现状（做什么） | EmoPlay 落点 | 优先级 | 依赖 |
|---|---|---|---|---|
| **file-browser + addons** | `scripts/file-browser/`（上游 CogentRedTester，`replace` 模式）：OSD 文件浏览器、目录缓存、多选、拨号收藏；addons 生态两层：仓库内 `script-modules/file-browser-addons/`（favourites/filter-paths/iso-loader/sort/url-decode/winsort）与上游 `scripts/file-browser/addons/`（apache-browser/ftp-browser/m3u-browser/powershell/root 等） | **P6**：原生 QML 文件浏览器（目录树、收藏、多选）；addon 协议定义为 P8 插件接口之一（浏览源插件：FTP/WebDAV/ISO 挂载等） | 高 | Qt Quick ListView + 文件系统模型；P8 插件浏览源接口（§4） |
| **playlistmanager.lua** | 播放列表浏览/排序/移动/保存/随机/网络标题解析（`script-opts/playlistmanager.conf`）；EmoPlay P3 已有 observe playlist + 侧栏基础 | **P6**：播放列表管理增强——排序、磁盘保存（P3 收尾项）、网络标题解析 | 中 | P3 播放列表（已完成基座）；文件持久化 |
| **history-bookmark.lua** | 记录播放历史并恢复进度（sorayuki 版精简，`script-opts/history_bookmark.conf`）；与 `simplehistory.lua`、`simplebookmark.lua` 分工：历史+进度书签 | **P6**：并入 HistoryStore 演进——P3 已记录 path/title/duration/position；补齐书签（文件级/进度级）实体 | 中 | P3 HistoryStore（已完成基座）；注意架构决策 7：不缓存播放状态，全部走信号+按需读取 |
| **recentmenu.lua** | 最近播放菜单（`~~/recent.json`，uosc/select/命令面板三处适配）；EmoPlay P3 idle 界面已有最近播放入口 | **P6**：最近播放视图（P3 雏形补全为完整列表页，含隐身模式开关） | 中 | P3 HistoryStore |
| **auto-save-state.lua** | 周期性 `write-watch-later-config`（默认 60s、99% 上限、暂停感知延迟），替代退出时才保存 | **P6**：断点续播策略并入 HistoryStore——定时 position 落盘已有 5s 刷新，补齐「写盘节流+接近片尾不保存」语义 | 中 | P3 HistoryStore；架构决策 7 |
| **chapterskip.lua** | 静音跳转（检测下一静音位）、按标题/时间正则跳过章节、片头片尾标记跳过；`script-opts/chapterskip.conf` | **P6**：章节/片段跳过引擎——章节列表 UI + 静音检测跳转 + 跳过规则（正则/标记），与 skip-segments 的片头片尾标记合并设计 | 中 | 章节属性（`chapter-list`、`chapter-metadata`）；`demuxer-cache-state`（静音检测） |
| **bluray-titles-menu.lua** | 蓝光/UHD 原盘标题列表菜单；只读取 edition/title 枚举，不接管 ISO 加载与原盘菜单会话 | **P6**：原盘浏览——标题列表 UI + 切换；ISO 加载走 file-browser iso-loader 插件语义 | 低 | disc 枚举属性（`edition-list`、disc:// 协议） |
| **edition-list.lua** | 多 edition（版本）文件加载时提示并可交互切换（scroll-list 实现） | **P6**：轨道/版本抽屉增加 edition 分区 | 低 | `edition-list`/`current-edition` 属性 |
| **delete-current-file.lua** | 删除当前播放文件（Linux 需 trash-cli，回收站语义）并移出播放列表，需确认 | **P6**：文件管理——删除到系统回收站（QML/C++ 原生 trash API） | 低 | Qt 文件引擎回收站接口；播放列表操作（P3） |
| **blacklist-extensions.lua** | 按扩展名黑白名单过滤目录加载（oneshot 模式），可移除无扩展文件 | **P6**：媒体库/文件夹扫描的过滤规则（设置中心可配） | 低 | 文件系统扫描（P6 媒体库） |
| **online-media.lua**（补充行，被 live 变体依赖） | 在线媒体解析承接：yt-dlp/自建 resolver、质量阶梯列表、venv 运行时（`runtime-manifest.json` 按 manifest 校验）；`deploy.py` 对其报四态 | **P9**（网络播放子项）：原生 URL 解析 + 质量阶梯 API，供 live-adaptive-quality 与清晰度菜单调用 | 中 | yt-dlp 集成；进程托管；本表 live 变体行 |

### 2.3 音频（P7 音乐模式）

| 脚本/组件 | 现状（做什么） | EmoPlay 落点 | 优先级 | 依赖 |
|---|---|---|---|---|
| **music-mode.lua** | 音乐播放模式：按文件类型/模式（loop 等，`script-opts/music_mode.conf`）切换行为，`gapless-audio=weak` 配合无缝切歌，联动窗口/OSD 行为 | **P7**：音乐模式场景（UI 布局切换、专辑浏览、歌词位）总开关，行为语义以 mpv.md P7 验收为准 | 高 | 音频文件探测（`track-list`/mime）；P7 歌词/EQ |
| **equalizer-gui.lua** | 可视化 10 段 EQ：图形化推子、14 种预设、开关胶囊、配置持久化到 `script-opts/equalizer-gui.json` 并启动自动加载；与比特流直通互斥 | **P7**：原生均衡器——af 滤镜链写入 + QML 推子（遵循架构篇命令式同步约定）+ 预设/自定义持久化 | 高 | libmpv `af` 命令（lavfi equalizer/firequalizer）；JSON 设置持久化；与 audio-passthrough 互锁 |
| **audio-passthrough.lua** | 比特流直通开关（off 默认、切换可重载当前文件、缓冲参数），`script-opts/audio_passthrough.conf` | **P7**：音频输出设置——直通模式切换 + 与 EQ 互斥逻辑 | 中 | libmpv `audio-output*`/`ad` 重载；设备枚举 |
| **drcbox.lua** | `dynaudnorm` 动态范围压缩 + OSD 可视化反馈（振幅图），`CTRL+F2` 循环档位 | **P7**：响度/动态范围处理档位（含 OSD 反馈曲线） | 中 | libmpv `af` 滤镜；OSD overlay 或 QML 图形 |

### 2.4 弹幕、字幕与服务（P8 插件仓库）

| 脚本/组件 | 现状（做什么） | EmoPlay 落点 | 优先级 | 依赖 |
|---|---|---|---|---|
| **uosc_danmaku 全网弹幕**（上游 Tony15246，`check`+`baseline d8c1cd8`，本地有重构如 `resolver_affinity.lua`） | 弹幕搜索/自动加载/剧集自动关联，B 站与爱腾优源（AES/Base64/MD5 签名模块），overlay 高刷渲染，延迟/保存/源管理/样式菜单（`script-opts/uosc_danmaku.conf`） | **P8**：弹幕源插件（每源一个插件，走 §4 清单）；**原生只做弹幕渲染层**（QML overlay 或 osd-overlay，高刷、防碰撞、样式），源解析不进核心 | 高 | 弹幕渲染层（QML 扩展点）；P8 插件 UI 契约与 HTTP 能力；各站 API 属插件自身责任 |
| **在线字幕双源：`sub-assrt.lua` + `sub-wyzie.lua`** | ASSRT（射手网）搜索下载（dyphire/MIT）；Wyzie：文件名→标题清洗→TMDb 取 ID→Wyzie 搜索→下载挂载，需免费 API key（未配置只提示不发请求）；uosc 另内置 OpenSubtitles 下载 | **P8**：字幕源插件（ASSRT/Wyzie/OpenSubtitles 各一个）；核心只暴露「当前媒体元数据」与「sub-add」接口 | 高 | 插件宿主 HTTP 能力；`sub-add` 命令（P3 已有）；TMDb 查询（P6 媒体库元数据可复用） |
| **sub-fastwhisper.lua（whisper）** | 本地 `whisper-cli` 生成 srt 字幕（热键触发，`script-opts/sub_fastwhisper.conf`） | **P8**：语音识别引擎插件（whisper.cpp/ faster-whisper 二进制+模型由插件声明依赖）；核心提供进度回调与字幕挂载 | 中 | 子进程托管与进度事件；模型分发策略（用户自备或插件清单声明） |
| **manager.lua + manager.json** | 第三方更新器：来源清单由 `manager.json` 定义（name/url/branch/source/destination/mode/baseline 十条目，见 §4.1）；异步 git 子进程 + bare mirror（`.manager/<name>.git`）+ `.manager/state.json` 状态；`check` 只比较目录树绝不覆盖，`replace` 确认变化后暂存→备份→整体替换且遇本地修改保护性跳过；嵌套 `.git` 移入备份；mpv 退出中止子进程；uosc/OSD 显示进度 | **P8**：插件仓库管理的**直接原型**——字段语义与双模式协议原样吸收进 P8 插件清单与更新器（见 §4），git 子进程换成 Qt 并发 | 高 | Qt 并发子进程；本地状态持久化；§4 全文 |

### 2.5 平台与系统集成（P9 平台集成）

| 脚本/组件 | 现状（做什么） | EmoPlay 落点 | 优先级 | 依赖 |
|---|---|---|---|---|
| **deploy.py 依赖自检** | 纯标准库（Python 3.9+）三平台自检：mpv/hwdec（后端列表与**运行时**拆开，后者恒 `unverified`）、渲染上下文、音频（按平台探测 PipeWire/WASAPI/coreaudio）、剪贴板、GPU、RIFE/vsmlrt、字体四字重；四态 `available/degraded/unavailable/unverified`；`--check/--prepare/--fonts-dir/--skip-fonts/--json`；退出码 0/2；`--prepare` 补建 `cache/`、`files/`、`cache/shaders_cache/`、`cache/icc_cache/`、`files/screen/` | **P9**：首启体检与运行时准备——四态模型与「列表里有≠跑得通」原则原样吸收；C++/Qt 实现（QProcess 探测 + JSON 报告 + 运行时目录补建），打包内嵌 | 中 | QProcess/QStandardPaths；缺失组件的指引链接 |
| **keybinding-manager.lua** | 键位管理：菜单浏览全部绑定、详情、按键捕获、冲突检测，写回 `script-opts/yaozhi-keybindings.json` | **P9**：设置中心键位编辑器——输入映射表 + 冲突检测 + 捕获录入（QML 原生，不经 input.conf 回写） | 中 | QML 键事件捕获；配置模型（P4 MpvConfigParser 可用于导入既有 input.conf） |
| **external-player.lua** | 外部播放器移交：识别慢启动代理（Ainan/CatPaw）顺序启动、loopback HTTP 白名单校验、`script-opts/external-player.conf` | **P9**：「用外部程序打开」——协议/程序注册 + URL 校验 | 低 | QDesktopServices/进程启动 |
| **persist_properties.lua** | 指定属性（volume、sub-scale 等）跨文件/会话持久化到 JSON（d87 版改造） | **P9**：应用设置持久化的一部分——可持久化属性白名单，随设置中心统一落盘 | 低 | 设置存储（与 EQ/画质档位同一套） |
| **pip.lua** | 画中画：缩小置顶小窗（autofit/geometry 可配，还原档位三态） | **P9**：窗口模式——小窗置顶（Qt 窗口标志 + geometry），系统级 PiP 待平台能力评估 | 低 | Qt 窗口标志；平台窗口管理差异 |
| **auto-fullscreen.lua** | 按媒体类型（视频/图片）自动全屏切换（默认关，`script-opts/auto_fullscreen.conf`） | **P9**：窗口行为设置项（视频自动全屏开关） | 低 | 内容类型探测；窗口状态控制 |
| **platform.conf**（补充行） | 按 mpv `platform` 属性为 Linux/Windows/macOS 注入 `gpu-context`/`ao`/`hwdec`，三平台共用一份配置 | **P9**：三平台渲染/音频策略常量表内置 C++，替代用户侧 conf 分流 | 低 | 平台探测（QSysInfo/编译期）；P1 收尾的 CI 矩阵验证 |

### 2.6 保留为脚本（P4 兼容层继续承载 / P8 插件）

| 脚本/组件 | 现状（做什么） | EmoPlay 落点 | 优先级 | 依赖 |
|---|---|---|---|---|
| **evafast.lua** | 长按右键快进 + 点按跳转混合（bilibili 式），`input-ar-delay` 控制阈值 | **保留为脚本**；若原生吸收则归 P9 输入系统（长按手势） | 低 | 输入系统长按语义 |
| **长尾脚本群**：simple-mpv-webui（浏览器控制）、trakt-scrobble（Trakt 同步）、sponsorblock_minimal、sleep-timer、memo、undoredo、slicing_copy、mpv-cropscreen、mpv-animated、startup-format-logos、system-monitor、thumbfast、stats.lua（中文增强）、sub_export、autosubsync、sub-select、chapter-make-read、episode-preferences、mute-on-specific-subtitle-words.js、strm-loader、autoload 等 | 单点功能清晰、与引擎耦合低，上游活跃 | **保留为脚本**：P4 兼容层扫描/加载 `scripts/`；其中适合第三方的（webui/trakt/sponsorblock）登记进 P8 插件清单示例（`check` 模式） | 低 | P4 兼容层（扫描/导入，解析器已就绪）；P8 清单（§4） |

## 3. 明确不映射的项（含理由）

- **uosc（定制版 5.13.0）**：EmoPlay 原生 QML UI 就是它的替代品，不迁移其配置；其信息分工
  （顶栏=标题/章节/引擎胶囊、时间轴=唯一完整进度源、底栏=纯操作）作为 UI 设计参考保留。
- **MFGA 字体、ICC 文件、`fonts/`**：授权未定或个人资产，不进入 EmoPlay 仓库或发行包；
  P9 只保留「用户字体目录探测 + 回退警告」的体检项（deploy.py 语义）。
- **`shaders/`、`vs/` 的文件本体**：资源不复制，语义进入注册表，文件由 P8 按来源重新获取（§4.3）。

## 4. P8 插件仓库格式目标

### 4.1 清单字段：兼容 manager.json 语义

EmoPlay 插件清单（JSON）**兼容 mpv-Emo `manager.json` 的字段语义**，以便用户既有来源可以
低迁移成本导入：

| 字段 | manager.json 语义（实测） | EmoPlay 语义 |
|---|---|---|
| `name` | 来源唯一名（如 `uosc`、`anime4k`） | 插件 ID，小写连字符，全局唯一 |
| `url` | 上游 git 仓库 URL | 插件来源仓库；官方内置项可省略 |
| `branch` | 跟踪分支 | 同左，默认 `main` |
| `source` | 上游仓库内的子目录（`.` 表示整仓） | 同左：插件内容在仓库内的路径 |
| `destination` | 安装目标（`scripts/uosc`、`shaders/Anime4K/glsl`、单文件 `scripts/mpv360.lua`） | EmoPlay 插件目录约定：`plugins/<id>/`；脚本/着色器型插件映射到兼容层目录 |
| `mode` | `check`：只获取上游并比较提交与目录树，报告有无更新，**绝不覆盖本地**；`replace`：确认上游目录树确有变化后，暂存→备份→整体替换；无变化时不产生备份 | 原样保留两种模式；`replace` 额外要求：目标未被本地修改（mpv-Emo 的保护性跳过语义），否则拒绝并提示改为 `check` |
| `baseline` | 定制版本基于的上游版本/提交（如 uosc `5.13.0`、hdr-toys `b43e457`）；上游前进时人工三方合并后再更新 baseline | 同左：记录「本地差异基于哪个上游基线」，更新前强制人工确认 |

EmoPlay 扩展字段（可选，不破坏兼容）：`version`、`license`、`homepage`、`platforms`、
`entry`（插件入口/类型：qml-service、lua-script、shader-pack、asset）、`minAppVersion`。
更新器行为吸收：bare mirror + 状态文件、退出中止子进程、进度事件上报 UI。

### 4.2 「本地定制永不覆盖」原则

manager.lua 的核心约束在 EmoPlay 中作为硬性规则：

1. 默认 `check`；`replace` 必须显式声明且逐项确认。
2. 目标目录存在本地修改时，`replace` 保护性跳过（先提示，绝不静默覆盖）。
3. 备份先行：任何替换先暂存并备份到插件数据目录；旧嵌套 `.git` 移入备份不参与复制。
4. 定制基线（`baseline`）前移必须经过人工合并，更新器永不自动改写本地差异。

### 4.3 插件来源分类

| 来源类 | 说明 | 例（对应本计划） |
|---|---|---|
| **官方内置** | EmoPlay 仓库自带、随发行包分发，无需下载 | 弹幕渲染层、EQ、文件浏览器、HistoryStore——即本文各「原生化」落点 |
| **上游 git** | 第三方仓库按 §4.1 协议获取与更新 | hdr-toys、Anime4K、file-browser、mpv360、equalizer-gui 上游、uosc_danmaku 源插件、ASSRT/Wyzie 字幕源、whisper 引擎包 |
| **本仓库资产** | mpv-Emo 中用户的自研成果（设计/配置语义），EmoPlay 以原生实现吸收后**不再以资产形式分发** | content-adaptive 决策表、hdr-pipeline 守护逻辑、deploy.py 四态模型、vs/ 预设语义 |

> 依 LGPL-3.0 与「只借鉴设计」原则：mpv-Emo 的任何文件本体不复制进 EmoPlay；上表「本仓库资产」
> 指重写为原生代码的**行为语义**。分发链上的第三方组件（GPL/MIT 着色器、脚本）一律通过
> 「上游 git」类按其原始许可证获取，EmoPlay 只提供获取与更新机制。

## 5. 排期落点（衔接 docs/roadmap.md）

- **P5 首批次（高）**：着色器注册表 → content-adaptive → HDR 双引擎 → dynamic-crop。
- **P6/P7 首批次（高）**：file-browser 原生化、history/断点续播收尾（auto-save-state 语义）、
  music-mode + equalizer-gui（互锁 audio-passthrough）。
- **P8 启动件**：manager.json 兼容清单 + check/replace 更新器 → 弹幕源与字幕源插件示范。
- **P9**：deploy.py 四态体检、键位编辑器、platform 策略内置。
- 每个映射项落实现时，按 conventional commits 一个功能一个 commit，CI（Qt 6.4.2 / libmpv 0.37）
  绿后才推进下一项。
