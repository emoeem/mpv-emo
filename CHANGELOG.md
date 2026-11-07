# Changelog

从 `docs/execution-log.md` 提炼；细节以执行记录、各 PR 与 `docs/architecture.md`
为准。格式参考 Keep a Changelog；预发布序列 alpha → beta → rc。各版本内容以
对应 tag 的实际快照为准（0.2.0-beta 起 `git ls-tree <tag>` 核对）。

## [0.1.0-alpha] — 2026-11-06 已发布（prerelease）

首批可用版本：播放器核心 + 三平台发布管线（execution-log 批次 1–4）。

### Added

- **P0 奠基**：Qt6/QML + C++20 + libmpv 动态链接选型落地；CMake/Ninja 工程、
  MIT LICENSE、docs（architecture / tech-selection / roadmap）。
- **P1 引擎嵌入**：`MpvEngine`（vo=libmpv、wakeup→GUI 线程事件泵、观察属性→
  Qt 信号）与 `MpvItem`（QQuickFramebufferObject 嵌入渲染，render 线程持有
  mpv_render_context，状态镜像为 Q_PROPERTY）。
- **P2 基础 UI**：控制栏（进度/音量/播放/停止/静音/全屏/打开）、快捷键、控件
  自动隐藏、暂停指示、文件对话框；`--smoke-test` CI 冒烟通道。
- **P3 播放管理**：音轨/字幕切换与外挂字幕加载、播放列表（append-play /
  next / prev / play-index / clear）、播放历史（去重置顶、上限 50 条、5s
  position 刷新、JSON 原子落盘）。
- **P9（部分）发布管线**：`release.yml` 三平台构建（Linux AppImage / Windows
  zip / macOS dmg），tag push 自动发布 prerelease；外部 action 固定 SHA、
  下载校验 SHA256、最小权限。
- **CI**：Ubuntu 24.04（Qt 6.4, GCC 13）构建 + ctest + Xvfb QML 启动冒烟。

### 发布资产

`v0.1.0-alpha` prerelease：AppImage ≈106 MB / zip ≈91 MB / dmg ≈71 MB
（见 https://github.com/emoeem/mpv-emo/releases/tag/v0.1.0-alpha ）。

### 已知限制

- Windows/macOS 产物未经实体机验证；无 Authenticode / Developer ID 签名公证。
- 音量 UI 语义未统一（滑条 0–100，快捷键可至 130）；播放列表未持久化。

## [0.2.0-beta] — 2026-11-07 已发布（prerelease）

P4–P6 功能（execution-log 批次 5–6 + P6 v1，批次 9 补记）。
内容以 tag `f810e662`（`1379497`）快照为准：不含音乐模式、插件商店与
更新器（批次 7/8 的产物在 beta 之后合入，见下节「未发布」）。

### Added

- **P4 mpv 配置兼容层**：用户 mpv 目录扫描（XDG/legacy/%APPDATA% 语义对齐
  mpv 上游）、full/selective/standalone/skip 四种导入决策与首启向导、导入
  规则（强制 vo=libmpv/osc=no/terminal=no、include/profile/写路径剥离与
  AppData 重定向、逐条审计）、input.conf 恒定受控（未导入时钉到自有空文件）、
  按键转发（Qt 键 → mpv 键名 → keypress）。绝不写用户原配置目录。
- **P5 画质引擎 v1**：决策表（anime/live/off × 档位 × 分辨率 × fps/HDR 避让）
  + Anime4K/SSimSuperRes 分组挂载（对照上游 v4.0.1 链序）+ deband/tone-mapping
  （classic gpu 可用值域）+ 掉帧自动降档（会话级）+ 画质抽屉 + settings.json
  quality 节持久化；off 档 inert 规则（不碰导入的渲染选项）。
- **P6 媒体库 v1**：文件夹清单持久化、后台扫描线程、去重与 last-played 时间戳
  （file-loaded 驱动）、QAbstractListModel + LibraryDrawer。

### 发布资产

`v0.2.0-beta` prerelease（release.yml tag push run 42319680121 success）：
AppImage ≈106 MB / zip ≈91 MB / dmg ≈72 MB
（见 https://github.com/emoeem/mpv-emo/releases/tag/v0.2.0-beta ）。

### Tests

- 发布时 CI 共 **11 个测试套件**全绿（批次 5 收尾 8 套件 + P5 扩展 +
  test_library_store）；CI 全绿记录见 execution-log 各批次「CI 结论」。

### 已知限制

- 同 0.1.0-alpha：签名/公证未做，Windows/macOS 未经实体验证。
- 实机试用报告暴露三处启动阻塞，已在 tag 之后以 `09fba73` 修复（见下节）。

## [未发布] — v1.0-rc 候选（0.2.0-beta 之后合入 main 的全部变更）

### Added

- **P7 音乐模式 v1**（PR #3）：.lrc 解析（多时间戳/BOM/CRLF/元数据标签）、
  纯音频判定（albumart 封面轨不排除）、歌词高亮滚动、10 段均衡器
  （`af=equalizer=...` 链 + 平直/低音增强/人声/现场预设 + music 节持久化，
  真引擎 af 回读测试）。
- **P8 插件商店 v1**（PR #1）：manager.json 兼容清单解析、内置目录（qrc
  资产）、git 驱动的安装/更新（check 只读）/卸载、启用插件的 scripts 选项
  合并（不替换用户导入清单）、state.json 与错误上报、PluginDrawer。
- **P8 v2 在线插件源**（PR #6）：RemoteManifestSource（多远端清单 +
  AppData/plugin-sources 缓存 + 本地/远端合并）、远端目录安装/更新、
  PluginRemoteSection（QNetworkAccessManager seam 可注入，测试不碰真实网络）。
- **P5 v2 帧插值与 ICC**（PR #7）：VapourSynth MEMC 预设登记 + 准入让位
  决策表、`vf=vapoursynth` 带 `@emoplay-interp` 标签挂载/读回验证（诚实
  外部依赖：.vpy 脚本与 VS 运行时为外部资产）；ICC 显示配置（icc-profile +
  icc-cache 重定向 + 读回验证 + 禁用恢复导入快照）；InterpolationPanel/
  IccPanel；settings `interpolation`/`icc` 持久化。
- **P6 v2 缩略图**（PR #5）：ThumbnailService（ffmpeg 探测 + 串行生成 +
  `sha1(path+mtime)` 缓存）、LibraryModel `thumbnailRole`、LibraryDrawer
  双列网格、ffmpeg 缺失优雅降级。
- **P9 更新器**（PR #4）：`UpdateChecker` 每次启动一次 GitHub
  releases/latest 检查（User-Agent 必设、15s 传输超时、失败静默），命中后
  控制栏角标提示、点击打开 Release 页；**绝不自动下载/安装**。
- **打包**：CPack DEB/RPM 最小配置（包名 emoplay；注明 minimal，正式 Linux
  分发仍走 AppImage）。
- **P10 工具与文档**：`tools/benchmark.sh` 解码基准框架（缺工具时明确跳过）、
  `docs/testing-report.md`（自动化套件总览 + 实机手工矩阵模板）、本 CHANGELOG。

### Fixed

- **实机启动阻塞**（`09fba73`）：0.2.0-beta 实机报告的三处启动阻塞修复。
- **P7/P8/P6v2 评审修复**（批次 7/8/10）：歌词索引 epsilon、EQ 持久化契约、
  缩略图在途去重与析构静默、插件安装头记录/check 只读、远端源列表去重等
  （明细见 execution-log 各批次；P5 v1 与 P6 v1 的评审修复在 0.2.0-beta
  tag 之前合入，已随 beta 发布）。

### Tests

- 套件从 11（0.2.0-beta tag 处）扩到 **22**（新增 lrc/music×2/plugin×2/
  library/thumbnail/interpolation×2/icc/remote_manifest/update_checker）；
  最终 main CI：run 42391432826 success（ctest 22/22 + Xvfb QML 冒烟）。
  明细与盲区清单见 `docs/testing-report.md`。

## [计划中] — v1.0-rc 放行门槛

v1.0-rc **尚未发布**（远端 tag 实查仅 alpha/beta）。放行前需要：

- **backlog R1–R6 六包执行**（docs/backlog.md，2026-11-07 所有者批准；
  全部合并后发布 v1.0）。
- **P9 收尾**：Windows Authenticode / macOS Developer ID + 公证（backlog
  Blocked，需购买证书）；系统集成（文件/协议关联、外部播放器移交）；deploy
  式首启体检（四态 available/degraded/unavailable/unverified）；设置中心与
  键位编辑器（冲突检测、捕获录入）、窗口行为设置项。
- **P10 收尾**：`docs/testing-report.md` 手工矩阵在实体机铺开并留档；
  `tools/benchmark.sh` 性能基线落表；崩溃日志与 `mpv log-message` 收集链路
  （backlog R2/R5）。
- **既有收尾项**（roadmap「后续批次」）：Windows/macOS CI 矩阵；P4 配置
  回写；P5 off 档恢复用户导入值、GPU 档位检测与压制组词库、着色器商店；
  P7 专辑浏览/封面墙、gapless/loop、passthrough 互锁、EQ 防抖。
