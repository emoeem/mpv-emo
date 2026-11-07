# 测试报告与手工测试清单（P10）

> 第一部分是**自动化测试覆盖总览**（随批次演进；每次 CI 的实际通过数以
> `docs/execution-log.md` 对应批次的「CI 结论」为准）。第二部分是**实机手工
> 测试清单模板**：发布前复制 §2 按平台 × GPU × 功能矩阵逐格填写。自动化跑在
> CI（Ubuntu 24.04 / Qt 6.4.2 / libmpv 0.37 / GCC 13，offscreen/Xvfb），三平台
> 实体机验证必须走本清单。

## 1. 自动化测试覆盖总览

CI 入口：`.github/workflows/ci.yml`（构建 → `ctest` → Xvfb/llvmpipe QML 启动
冒烟）。套件全部在 `tests/`，与 `tests/CMakeLists.txt` 一一对应：

| # | 套件 | 覆盖对象 | 类型 | 作用 |
|---|---|---|---|---|
| 1 | `test_config_parser` | MpvConfigParser | 纯逻辑 | mpv.conf / input.conf 解析（行内注释语义差异、profile 段、引号分词、CRLF），P4 兼容层的数据基础 |
| 2 | `test_compat_scanner` | MpvProfileScanner | 纯逻辑 | 用户 mpv 配置目录定位（XDG_CONFIG_HOME 独占语义、legacy ~/.mpv 兜底顺序、Windows %APPDATA%；base-dir 注入，不碰真实 HOME） |
| 3 | `test_import_rules` | MpvImportRules | 纯逻辑 | 导入规则：vo/osc/terminal 强制、include/profile/写路径剥离与重定向、其余选项通过、逐条审计 |
| 4 | `test_app_settings` | AppSettings | 纯 Qt | settings.json 读写契约：导入决策/选择性清单/quality（含 interpolation/icc 嵌套对象）/music 节、损坏文件回退默认、QSaveFile 原子性 |
| 5 | `test_mpv_key_name` | MpvKeyName | 纯逻辑 | Qt 键事件 → mpv 键名映射（可打印文本、命名键表、组合键、不可映射返回空） |
| 6 | `test_compat_controller` | CompatController | headless（真实 mpv core） | 决策 → initialize() 选项组装、own input.conf 恒定受控、wizard 请求状态 |
| 7 | `test_quality_profile_engine` | QualityProfileEngine | 纯逻辑 | 画质决策表：anime/live/off × tier × 分辨率分组 × fps≥50/HFR 避让 × HDR、Anime4K/SSimSuperRes 组链、非法 scale/deband 值校验 |
| 8 | `test_quality_controller` | QualityController | headless（真实 mpv core） | 应用路径：选项写入、change-list glsl-shaders 挂载/收编、inert-at-off、掉帧自动降档 |
| 9 | `test_interpolation_decision` | InterpolationDecision | 纯逻辑 | P5 v2 插值决策表：MEMC_* 预设登记、HDR/高帧率/脚本缺失/vf 链冲突让位、vf 项组装（`%len%` 逐字节引号） |
| 10 | `test_interpolation_controller` | InterpolationController | headless（真实 mpv core） | P5 v2 插值应用路径：vf change-list 挂载/读回探测（`@emoplay-interp` 标签）、不可用回退关闭 |
| 11 | `test_icc_controller` | IccController | headless（真实 mpv core） | P5 v2 ICC 应用路径：icc-profile 读写回验 + icc-cache-dir 经 P4 重定向表指向 AppData、禁用恢复导入快照 |
| 12 | `test_mpv_engine` | MpvEngine | headless（真实 mpv core） | libmpv 句柄：初始化、属性设置/回读（含字符串 setProperty 地址语义回归）、命令、观察属性信号 |
| 13 | `test_lrc_parser` | LrcParser | 纯逻辑 | .lrc 解析：1–3 位小数时间戳、多时间戳行、[ti]/[ar]/[al]、BOM、CRLF、同名候选规则 |
| 14 | `test_music_controller` | MusicController | 纯逻辑 + headless | 纯音频判定（albumart 不排除）、歌词索引镜像、10 段 EQ af 链组装与真引擎回读、预设/持久化 |
| 15 | `test_music_page` | qml/MusicPage.qml | 离屏 QML | 音乐页实例化：控件构成、EQ 滑条（视觉树）、歌词与元数据绑定（CI 冒烟只启动 Main.qml，Loader 不激活，故页面需自有测试） |
| 16 | `test_history_store` | HistoryStore | headless | 播放历史：记录/去重置顶/上限 50 条/5s position 刷新/JSON 原子落盘/损坏回退 |
| 17 | `test_plugin_manifest` | PluginManifest | 纯逻辑 | manager.json 兼容清单解析 + 随仓库内置目录（builtin_sources.json）合法性 |
| 18 | `test_plugin_manager` | PluginManager | 纯 Qt（假 git runner） | 插件状态机：安装/更新(check 只读)/卸载、scripts 选项合并、state.json、错误上报——无网络无真实 git |
| 19 | `test_remote_manifest_source` | RemoteManifestSource | 纯 Qt（假 fetcher） | P8 v2 在线清单源：远端 JSON 解析（fixture）、AppData/plugin-sources 缓存回读、本地+远端合并——同步假 PluginRemoteFetcher，无真实网络 |
| 20 | `test_library_store` | LibraryStore / LibraryScanWorker / LibraryModel | 纯 Qt | 媒体库：扫描与去重、文件夹清单持久化、worker 线程扫描、QAbstractListModel 行为 |
| 21 | `test_thumbnail_service` | ThumbnailService / ThumbnailQueue | 纯 Qt（stub 进程链） | P6 v2 缩略图：缓存键哈希（path+mtime）、时长/seek 解析、队列去重调度、ffmpeg 探测/生成链——stub 脚本驱动，不跑真 ffmpeg |
| 22 | `test_update_checker` | UpdateChecker | 纯逻辑 + 本地回环 | P9：tag 解析、版本比较（含预发布序）、releases URL 组装；网络路径全部走 127.0.0.1 回环 HTTP（找到新版、html_url 回退、同版本/404/坏 JSON 静默、in-flight 去重），**不发真实请求** |
| — | CI 冒烟（ci.yml 末步） | Main.qml 全启动 | 集成 | xvfb-run + llvmpipe：QML 能实例化渲染 3s（`--smoke-test`） |

计数口径：`ctest` 输出「N% tests passed, 0 tests failed out of N」中的 N =
上表 1–22（0.2.0-beta tag 处为 11；四路 v2 合入后为 22，main run
42391432826 实查）；冒烟不计入 ctest。各批次的具体数字见 execution-log。

### 已知自动化盲区（必须由 §2 手工矩阵覆盖）

- 真实 GPU 渲染（CI 是 llvmpipe 软渲染）：hwdec、着色器组实挂、HDR tone-mapping 观感。
- 实机交互：拖动/焦点/快捷键的手感、控件自动隐藏、抽屉与向导流程。
- 音频输出设备矩阵（PipeWire/WASAPI/coreaudio、比特流直通）。
- 打包产物的干净系统安装与启动（AppImage/zip/dmg，以及 CPack DEB/RPM 的依赖完备性——后者按 CMakeLists 注明为 minimal，未验证依赖列表）。
- 网络播放与限速环境下的更新检查（GitHub API 可达性）。

## 2. 实机手工测试清单模板（平台 × GPU × 功能矩阵）

发布前复制本节到当轮报告（或 issue）并填写。**结果取值**：✅ 通过 /
⚠️ 部分通过（备注必填）/ ❌ 失败（附 issue 链接与复现步骤）/ — 不适用。

### 2.1 轮次信息

| 项 | 值 |
|---|---|
| 轮次 / 日期 | （如 v0.2.0-beta 发布轮 / 2026-MM-DD） |
| 构建来源 | （tag、commit SHA 或产物文件名 + 尺寸） |
| 测试人 | |
| 总体结论 | （放行 / 带缺陷放行 / 阻塞） |

### 2.2 环境定义（每台实体机一行）

| 环境 ID | 平台 / 版本 | GPU（型号 + 驱动版本） | CPU / 内存 | 音频子系统 | 备注 |
|---|---|---|---|---|---|
| A | Windows 11 24H2 | NVIDIA / AMD / Intel（驱动 ___） | | WASAPI | |
| B | macOS 15 (arm64) | Apple M-series | | coreaudio | dmg 为 ad-hoc 签名，首次启动需右键打开 |
| C | Ubuntu 24.04 | Intel iGPU（mesa ___）/ NVIDIA（ proprietary ___） | | PipeWire | |
| D | （按需增行） | llvmpipe 软渲染 | | | 兜底基线 |

### 2.3 功能矩阵（行 = 功能，列 = 环境 ID）

| # | 功能 | A | B | C | D | 备注（⚠️/❌ 必填） |
|---|---|---|---|---|---|---|
| 1 | 启动与 QML 冒烟（窗口出现、无错误对话框） | | | | | |
| 2 | 打开文件播放（O / 拖放 / CLI 位置参数） | | | | | |
| 3 | 硬件解码生效（`hwdec=auto-safe`；日志/`current-vo` 核对） | | | | | |
| 4 | 播放控制（进度 seek、音量、静音、暂停、全屏、控件自动隐藏） | | | | | |
| 5 | 音轨/字幕切换、外挂字幕 sub-add | | | | | |
| 6 | 播放列表（追加/下一个/上一个/清空）与历史回放 | | | | | |
| 7 | mpv 配置导入向导（full / selective / standalone / skip；重跑入口） | | | | | |
| 8 | 导入的 input.conf 按键在嵌入 UI 生效（未被自身快捷键消费的键） | | | | | |
| 9 | 画质引擎（tier 切换、Anime4K 组按分辨率挂载、deband、HDR tone-mapping；着色器目录来源符合导入决策） | | | | | |
| 10 | 掉帧自动降档提示（构造高负载源） | | | | | |
| 11 | 音乐模式（音频文件 → MusicPage、封面、歌词高亮滚动、EQ 预设/自定义持久化） | | | | | |
| 12 | 媒体库（加文件夹、扫描去重、点击回放带 last-played） | | | | | |
| 13 | 插件商店（浏览内置目录、安装、启用后脚本生效、卸载清理） | | | | | |
| 14 | 更新角标（当前版本 < 最新 release 时出现；点击打开 Release 页；离线时静默不出现） | | | | | |
| 15 | 打包产物在干净环境可用（AppImage / zip / dmg；DEB/RPM 依赖现状如实记录） | | | | | |
| 16 | 长时间播放稳定性（≥30min 无泄漏迹象、掉帧统计合理） | | | | | |
| 17 | 崩溃/错误路径（损坏媒体、不可读目录、settings.json 损坏）不崩溃且有日志 | | | | | |
| 18 | 帧插值（预设切换生效；系统无 vapoursynth / .vpy 脚本缺失时回退关闭并提示；HDR/高帧率源让位） | | | | | |
| 19 | ICC 显示配置（profile 选择生效、icc-cache 落在 AppData、禁用后恢复导入的 icc-profile） | | | | | |
| 20 | 媒体库缩略图（真 ffmpeg 生成、缓存命中、ffmpeg 缺失降级占位；含奇数高度源条目） | | | | | |
| 21 | 插件在线源（远端清单拉取/缓存、远端插件安装/更新、断网时静默降级到内置目录） | | | | | |

### 2.4 性能小节（tools/benchmark.sh 落表）

在一台实体机上运行 `tools/benchmark.sh out_dir [样例目录]`，把生成的
`benchmark-results.md` 表格粘贴到此处，并注明环境 ID、CPU、内存、存储与
内核/驱动版本。表格框架由脚本产出；样例媒体行速率列留空。

### 2.5 缺陷登记

| 编号 | 环境 | 功能 # | 现象 | 严重级 | issue 链接 |
|---|---|---|---|---|---|
| D-1 | | | | | |
