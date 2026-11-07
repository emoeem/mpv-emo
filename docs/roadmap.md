# 路线图与任务状态

原始规划来自 `mpv.md`（P0–P10）。下表为实际执行视图；状态：`done / in-progress / pending / blocked`。

## 阶段总览

| 阶段 | 内容 | 状态 | 验收依据 |
|---|---|---|---|
| P0 | 选型、架构文档、仓库与 CI 骨架 | **done** | docs/*, GitHub Actions 绿 |
| P1 | libmpv 嵌入 + 渲染上下文 | **in-progress** | Linux CI 构建+冒烟通过；Win/mac 待 CI 矩阵（后续任务） |
| P2 | 播放器基础 UI（控制栏/快捷键/全屏/打开文件） | **in-progress** | Linux CI 冒烟通过；实机交互待验证 |
| P3 | 音轨/字幕/播放列表/历史 | **done** | Linux CI 构建+单测+冒烟 success（评审修复后 `6b719ac`）；实机交互验证与播放列表持久化列入后续批次 |
| P4 | mpv 配置兼容层（扫描/导入/回写） | **done**（扫描/导入/向导/脚本与 input.conf 生效已落地并附单测，批次 5 收尾 CI 全绿终审通过；GUI 键位编辑器与设置页入口、回写属后续批次） | Linux CI 构建 + 单测（ctest 8/8）+ Xvfb QML 冒烟 success（评审修复后 head `a35774a`，run 42296377325）；批次记录见 execution-log 批次 5；T13–T19 commits：`d815234` 扫描器、`9df56d0` 导入规则、`1c480ed` settings 持久化、`85e74c3` 按键转发、`d75b6bb` CompatController、`6cfb1c9` 启动序、`9848ceb` 向导；设计见 architecture.md「配置兼容层」 |
| P5 | 高级画质（滤镜/HDR/tone-mapping UI） | **in-progress**（v1 子集 done，批次 6 收尾 CI 全绿终审通过） | v1 子集已落地：Anime4K/SSimSuperRes 着色器可挂载（分辨率分组决策表 + 面板管理列表）、HDR→SDR tone-mapping 可配置（auto/bt.2390/mobius/hable，spline/bt.2446a 为 gpu-next 专属、当前 vo=libmpv 管线不可用）、画质集中 UI（控制栏 ✦ 抽屉）、掉帧自动降档、settings.json quality 节持久化；单测 10/10（含 QualityProfileEngine 决策表与 settings quality 读写）。最终 CI 全绿 head `6915d2d`，run 42314902255（日志实查 ctest 10/10）；批次记录与评审处置见 execution-log 批次 6；T20–T26 commits：`7959f01` 决策表、`d1a1b62` 属性观察、`ab6de2e` MpvItem 镜像+掉帧采样、`d4e3e51` settings 持久化、`9a191ba` QualityController、`6d471be` 启动接线、`7c643c9` deband 偏好、`167dc5b` 画质抽屉。**未含项**（整体保持 in-progress 的原因）：RIFE/VS 补帧、ICC 自动加载、着色器商店、GPU 档位检测/压制组词库、off 档恢复用户导入值（现为 mpv 默认值）。设计见 architecture.md「画质引擎（P5 v1）」 |
| P6 | 媒体库与文件管理 | **in-progress**（v1 媒体库 done（PR #2，批次记录未单列）；v2 缩略图 done，批次 8） | v1：目录清单 + 扫描（扩展名分类/去重/排序）+ library.json 持久化 + 抽屉列表与搜索（`src/library/LibraryStore/LibraryScanWorker/LibraryModel`）。v2：ThumbnailService——启动异步探测 ffmpeg（`ffmpeg -version`）、按需串行后台生成（`-i` 时长解析 → `-ss <时长10%> -i file -frames:v 1 -vf scale=320:-1 -y`）、缓存 `AppData/thumbnails/<sha1(path+mtime)>.jpg`、ThumbnailQueue（QSet 去重 + FIFO）纯逻辑、ffmpeg 缺失优雅降级（类型图标占位；已缓存仍命中）；LibraryModel `thumbnailRole` + LibraryDrawer 双列网格（Image 异步加载）。单测不跑真 ffmpeg（stub 脚本驱动进程链）。**未含项**：缓存 GC、生成进度/取消 UI、`scale=320:-1` 奇数高度源的实机验证。设计见 architecture.md「媒体库与缩略图（P6 v1 + v2）」 |
| P7 | 音乐模式（歌词/EQ） | **in-progress** | v1 已落地（nativeization-plan music-mode + equalizer-gui 简化版）：`.lrc` 解析（多时间戳/BOM/CRLF/[ti][ar][al]，同名文件规则）、纯音频判定（track-list 无 video 且音频扩展名，albumart 封面轨不排除）、10 段均衡器（`af=equalizer=...` 链 + 平直/低音增强/人声/现场预设 + music 节持久化，superequalizer 经 mpv 0.37/FFmpeg 6.1 源码核实后弃用）、`qml/MusicPage.qml`（Loader 按 audioMode 切换、歌词高亮滚动、EQ 折叠面板）、单测含真引擎 af 回读与离屏 QML 实例化。**未含项**：专辑浏览/封面墙、gapless/loop 行为、passthrough 互锁、EQ 防抖。设计见 architecture.md「音乐模式（P7 v1）」 |
| P8 | 插件生态（脚本/着色器管理；格式目标 = manager.json 兼容，见 nativeization-plan.md §4） | pending | — |
| P9 | 平台集成（打包/更新/系统集成） | **in-progress** | 打包/发布管线已落地：release.yml 三平台构建（Linux AppImage / Windows zip / macOS dmg）+ tag push 自动发布，v0.1.0-alpha prerelease 已发布（execution-log 批次 4）；更新器已落地（本批次）：`src/update/UpdateChecker` 每启动一次 GitHub releases/latest 检查（User-Agent 必设、失败静默）+ 控制栏 UpdateBadge 角标点击打开 Release 页，**绝不自动下载**；CPack DEB/RPM 最小配置（包名 emoplay，注明 minimal——无完整依赖列表，正式 Linux 分发仍走 AppImage）；**未做**：签名/公证（Windows 无 Authenticode、macOS 仅 ad-hoc）、系统集成（文件/协议关联）、deploy 式体检、键位编辑器/设置中心 |
| P10 | 全面测试与发布 | **in-progress** | 已落地（本批次）：`docs/testing-report.md`（自动化 17 套件覆盖总览 + 自动化盲区清单 + 实机平台×GPU×功能手工矩阵模板）、`tools/benchmark.sh` 解码基准框架（无系统 ffmpeg/mpv 时明确跳过、输出 docs 可引用表格）、`CHANGELOG.md`（alpha/beta/rc 三节）。**未做**：手工矩阵实体机执行与留档、性能基线落表、崩溃日志收集链路、正式版发布 |

## 当前批次任务清单

| # | 任务 | 状态 | 提交 |
|---|---|---|---|
| T1 | 仓库初始化、LICENSE(MIT)、README | done | 见 execution-log |
| T2 | P0 文档（architecture / tech-selection / roadmap） | done | 同上 |
| T3 | MpvEngine（libmpv 封装、事件泵、属性观察） | done | CI 验证中 |
| T4 | MpvItem（QQuickFramebufferObject 嵌入渲染） | done | CI 验证中 |
| T5 | QML 播放器外壳（控制栏、快捷键、FileDialog、自动隐藏） | done | CI 验证中 |
| T6 | MpvConfigParser + 单元测试 | done | CI 验证中 |
| T7 | Engine 冒烟单测（headless mpv init / 属性 / 信号） | done | CI 验证中 |
| T8 | GitHub Actions（构建+单测+Xvfb QML 冒烟） | done | CI 验证中 |
| T9 | 音轨/字幕抽屉（track-list UI、外挂字幕 sub-add） | done | 6cc4ec0 |
| T10 | 播放列表（observe playlist、侧栏 UI、append/prev/next/跳转/清空） | done | 38e608d |
| T11 | HistoryStore（JSON 持久化、5s position 刷新、idle 界面最近播放） | done | 7e202b0 |
| T12 | HistoryStore 单测（持久化/去重/上限/损坏恢复） | done | ef2395a（评审修复 986262b/6b719ac，CI run 42259020019 success） |
| T13 | P4：MpvProfileScanner（定位+扫描 ImportSummary）+ fixtures + 单测 | done | d815234 |
| T14 | P4：MpvImportRules（强制 vo/osc/terminal、写入路径重定向、include/profile 剥离）+ 单测 | done | 9df56d0 |
| T15 | P4：AppSettings（settings.json 决策持久化）+ 单测 | done | 1c480ed |
| T16 | P4：MpvKeyName + keyPress 按键转发（input.conf 在嵌入 UI 生效）+ 单测 | done | 85e74c3 |
| T17 | P4：CompatController（决策→选项组装）+ 单测 | done | d75b6bb |
| T18 | P4：main.cpp 启动序（settings→应用→initialize→向导） | done | 6cfb1c9 |
| T19 | P4：ImportWizard.qml 首启向导（完整/选择性/独立 + 不再询问） | done | 9848ceb |
| T20 | P5：QualityProfileEngine 决策表（anime/live/off × tier × fps × HDR）+ 单测 | done | 7959f01（CI 修复 7e7ff26/8cd4c87） |
| T21 | P5：MpvEngine 观察 video-params/container-fps/frame-drop-count/current-vo/video-format | done | d1a1b62 |
| T22 | P5：MpvItem 镜像 videoWidth/videoHeight/fps/droppedFrames/hdrActive + 2s 掉帧采样 | done | ab6de2e |
| T23 | P5：AppSettings quality 节持久化 + 读写单测 | done | d4e3e51 |
| T24 | P5：QualityController 应用路径（change-list 挂载、inert-at-off、自动降档）+ headless 单测 | done | 9a191ba（CI 修复 cc9472c：MPV_FORMAT_STRING setProperty 段错误） |
| T25 | P5：main.cpp 启动接线（着色器目录按导入决策、QML 桥）+ deband 偏好 | done | 6d471be、7c643c9 |
| T26 | P5：画质设置抽屉（内容类型/档位/缩放/deband/tone-mapping/着色器管理） | done | 167dc5b |
| T27 | P7：LrcParser（.lrc 解析 + 同名匹配规则）+ 单测 | done | d88afed |
| T28 | P7：MpvEngine 观察 metadata(NODE) | done | 805811a |
| T29 | P7：MusicController（纯音频判定 + 歌词行镜像）+ 单测 | done | 6233547 |
| T30 | P7：10 段均衡器（af 链 + 预设 + music 节持久化）+ 单测（含真引擎回读） | done | db906ea |
| T31 | P7：MusicPage.qml + Main.qml 入口 + main.cpp 接线 + 离屏 QML 实例化测试 | done | 712cf51 |
| T32 | P6 v2：ThumbnailService（ffmpeg 探测 + 串行生成 + sha1 缓存 + 队列纯逻辑）+ 单测（stub 链 / 缺失降级） | done | ba115d0 |
| T33 | P6 v2：LibraryModel thumbnailRole（按需请求 + 按行 dataChanged）+ 模型单测 | done | 2c834bb |
| T34 | P6 v2：LibraryDrawer 双列缩略图网格 + main.cpp 启动接线 | done | 784b012、280c8d1 |

## 后续批次（按优先级）

1. **P3 收尾**：播放列表磁盘持久化（roadmap 原文项，本批次未含）；音轨/字幕/播放列表实机交互验证。
2. **P1 收尾**：Windows（MSVC + 预编译 libmpv）/ macOS（brew mpv）CI 矩阵。
3. **P4 收尾**：GUI 键位编辑器、设置页导入入口（`compatController.requestWizard`）、配置回写；导入/向导/按键转发的实机交互验证。
4. **P5 收尾**：off 档让位快照（恢复用户导入值而非 mpv 默认）、GPU 档位检测与压制组词库、着色器商店（P8 资产分发）、RIFE/VS 补帧与 ICC 自动加载评估；画质面板与自动降档的实机交互验证。
5. **P6 收尾**：缩略图缓存 GC、生成进度/取消 UI、`scale=320:-1` 奇数高度源与实机 ffmpeg 行为验证；文件管理（删除到回收站）与原盘/edition 浏览（nativeization-plan §2.2 低优先级项）。
6. **P7 收尾**：专辑浏览/封面墙、gapless-audio/loop 行为语义、EQ 与比特流直通互锁、EQ 滑条防抖（每步重建 af 链的简化）、歌词 `[offset:]` 标签；音乐页与歌词高亮的实机交互验证。
7. **工程质量**：clang-format 固化 + CI 格式检查；QML 交互的集成测试（xvfb + 真实媒体样本）；崩溃日志与 `mpv log-message` 收集。
