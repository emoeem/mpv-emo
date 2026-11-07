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
| P5 | 高级画质（滤镜/HDR/tone-mapping UI） | **done**（v1 批次 6 + v2 插值/ICC PR #7；遗留项移交 backlog） | v1：决策表（anime/live/off × 档位 × 分辨率分组 × fps/HDR 避让）、Anime4K/SSimSuperRes 分组挂载、HDR→SDR tone-mapping（classic gpu 可用值域）、掉帧自动降档、画质抽屉、quality 节持久化（批次 6 终审 run 42314902255 success，ctest 10/10）。v2（PR #7，`00d61c7`）：帧插值——nativeization-plan §2.1 的 MEMC_* 预设登记 + 准入让位决策表、`vf=vapoursynth` 带 `@emoplay-interp` 标签挂载/读回验证（诚实外部依赖：.vpy 脚本与 VS 运行时为外部用户资产，缺失/不可用回退关闭并提示）；ICC 显示配置——icc-profile 选择 + `icc-cache-dir` 经 `MpvImportRules::redirectTarget` 重定向 AppData/icc_cache、读回验证、禁用恢复导入快照；InterpolationPanel/IccPanel；settings `interpolation`/`icc` 持久化。验收 CI：分支 head run 42388095956 success（out of 19）、merge run 42389458524 success；四路合并后 main 最终 run 42391432826 success（ctest 22/22 + Xvfb QML 冒烟）。**遗留项**（backlog/实机）：off 档恢复用户导入值、GPU 档位检测/压制组词库、着色器商店（P8 联动）、VS 插值与 ICC 的实机观感验证。设计见 architecture.md「画质引擎（P5 v1）」「帧插值 v2 与 ICC（P5 v2）」 |
| P6 | 媒体库与文件管理 | **done**（v1 PR #2（批次 9 补记）+ v2 缩略图 PR #5；遗留项移交 backlog） | v1：目录清单 + 扫描（扩展名分类/去重/排序）+ library.json 持久化 + 抽屉列表与搜索（`src/library/LibraryStore/LibraryScanWorker/LibraryModel`；merge run 42319359427 success，批次记录见 execution-log 批次 9 补记）。v2：ThumbnailService——启动异步探测 ffmpeg、按需串行后台生成（`-i` 时长解析 → `-ss <时长10%> -i file -frames:v 1 -vf scale=320:-1 -y`）、缓存 `AppData/thumbnails/<sha1(path+mtime)>.jpg`、ThumbnailQueue（QSet 去重 + FIFO）纯逻辑、ffmpeg 缺失优雅降级（类型图标占位；已缓存仍命中）；LibraryModel `thumbnailRole` + LibraryDrawer 双列网格；评审修复在途去重与析构静默（`3f4df37`）。验收 CI：分支最终 run 42390343430 success、merge run 42390605356 success；main 最终 run 42391432826 success（ctest 22/22）。**遗留项**：缓存 GC（backlog R3 LRU 上限）、生成进度/取消 UI、`scale=320:-1` 奇数高度源与真 ffmpeg 行为实机验证、文件管理/原盘浏览（nativeization-plan §2.2 低优先级）。设计见 architecture.md「媒体库与缩略图（P6 v1 + v2）」 |
| P7 | 音乐模式（歌词/EQ） | **done**（v1 批次 7，PR #3；遗留项移交 backlog） | `.lrc` 解析（多时间戳/BOM/CRLF/[ti][ar][al]，同名文件规则）、纯音频判定（track-list 无 video 且音频扩展名，albumart 封面轨不排除）、10 段均衡器（`af=equalizer=...` 链 + 平直/低音增强/人声/现场预设 + music 节持久化，superequalizer 经 mpv 0.37/FFmpeg 6.1 源码核实后弃用）、`qml/MusicPage.qml`（Loader 按 audioMode 切换、歌词高亮滚动、EQ 折叠面板）、单测含真引擎 af 回读与离屏 QML 实例化。验收 CI：分支 run 42379553921 success（ctest 14/14，execution-log 批次 7）；PR #3 merge run 42381637240 success；main 最终 run 42391432826 success（ctest 22/22）。**遗留项**：专辑浏览/封面墙、gapless/loop 行为、passthrough 互锁、EQ 防抖、歌词 `[offset:]`。设计见 architecture.md「音乐模式（P7 v1）」 |
| P8 | 插件生态（脚本/着色器管理；格式目标 = manager.json 兼容，见 nativeization-plan.md §4） | **done**（v1 PR #1 + v2 在线清单源 PR #6） | v1：manager.json 兼容清单解析（内置目录 assets/plugins/builtin_sources.json 经 qrc 分发）、PluginManager git 驱动安装/更新（check 只读）/卸载 + state.json + 错误上报、启用插件的 scripts 选项合并（不替换用户导入清单）、PluginDrawer。v2（PR #6，`f2ddf6d`）：PluginRemoteFetcher（QNetworkAccessManager 可注入 seam）、RemoteManifestSource（多远端清单 + AppData/plugin-sources 缓存 + 本地/远端合并）、远端目录安装/更新、PluginRemoteSection.qml；`test_remote_manifest_source`（fixture + 假 fetcher，无真实网络）。验收 CI：v1 merge run 42382311492 success；v2 head run 42390719297 success、merge run 42390945673 success；main 最终 run 42391432826 success（ctest 22/22）。**遗留项**：插件信任模型 UI（backlog R6）、着色器商店联动、architecture.md 尚无插件章节（P8 设计记录缺口）。 |
| P9 | 平台集成（打包/更新/系统集成） | **in-progress** | 打包/发布管线：release.yml 三平台构建 + tag push 自动发布，v0.1.0-alpha 与 v0.2.0-beta 两轮 prerelease 实跑成功（Release run 42276002560、42319680121 均 success）。更新器（PR #4，`9064d2d`）：`src/update/UpdateChecker` 每启动一次 GitHub releases/latest 检查（User-Agent 必设、15s 超时、失败静默）+ UpdateBadge 角标点击打开 Release 页，**绝不自动下载/安装**；`test_update_checker` 走 127.0.0.1 回环不发真实请求。CPack DEB/RPM 最小配置（包名 emoplay，注明 minimal，正式 Linux 分发仍走 AppImage）。验收 CI：分支 head run 42391144449 success、merge/main 最终 run 42391432826 success（ctest 22/22）。**未做**：签名/公证（Windows 无 Authenticode、macOS 仅 ad-hoc——backlog Blocked）、系统集成（文件/协议关联）、deploy 式体检、键位编辑器/设置中心 |
| P10 | 全面测试与发布 | **in-progress** | 已落地：`docs/testing-report.md`（自动化套件覆盖总览——已随四路 v2 更新至 **22 套件**，`tests/CMakeLists.txt` 22 个 add_test 实查——+ 自动化盲区清单 + 实机平台×GPU×功能手工矩阵模板）、`tools/benchmark.sh` 解码基准框架（无系统 ffmpeg/mpv 时明确跳过）、`CHANGELOG.md`（0.1.0-alpha / 0.2.0-beta 已发布两节 + 未发布 rc 候选节）。验收 CI：main 最终 run 42391432826 success（ctest 22/22 + Xvfb QML 冒烟）。**未做**：v1.0-rc **未发布**（`git ls-remote --tags` 实查远端仅 v0.1.0-alpha/v0.2.0-beta；放行门槛 = backlog R1–R6 六包 + Blocked 项解除 + 手工矩阵铺开）、手工矩阵实体机执行与留档、性能基线落表、崩溃日志收集链路（backlog R2/R5） |

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

**排期已移交 docs/backlog.md（R1–R6，2026-11-07 所有者批准）**：六包并行 worktree 批次，全部合并后发布 v1.0。原「各 P 收尾」清单的去向：

- **已入 backlog**：播放列表磁盘持久化（R3）、缩略图缓存 GC（R3 LRU）、崩溃日志收集（R2）、QML/真实媒体集成测试与 ASAN/UBSAN/覆盖率（R5）、插件信任模型与更新检查信任强化（R6）、make install 与 AppImage 更新元数据/DEB 依赖精细化（R6）。
- **未入 backlog、保留在 CHANGELOG「计划中」节**：P1 Windows/macOS CI 矩阵；P4 GUI 键位编辑器、设置页导入入口、配置回写；P5 off 档恢复用户导入值、GPU 档位检测/压制组词库、着色器商店；P6 生成进度/取消 UI；P7 专辑浏览/封面墙、gapless/loop、passthrough 互锁、EQ 防抖。
- **P5 v2 已消项**：RIFE/VS 补帧（v2 以 VapourSynth MEMC 预设 + 诚实外部依赖模式落地）与 ICC 自动加载（v2 ICC 控制器落地）。
- **不受 backlog 管辖的实机验证**：testing-report §2 手工矩阵（平台 × GPU × 功能）在实体机铺开并留档，rc 放行门槛之一。
