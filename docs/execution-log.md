# 执行记录

按批次记录任务、提交、CI 结果与发现的问题。最新在最下。

## 2026-11-06 · 批次 1：P0 项目奠基

- **任务**：T1, T2
- **产出**：仓库初始化（main 分支）、MIT LICENSE、README、docs/{architecture,tech-selection,roadmap}.md、.gitignore
- **决策**：
  - 技术栈 Qt6/QML + C++20 + libmpv 动态链接 + CMake/Ninja（依据 mpv.md 选型表与同类项目调研）
  - 本地环境无编译工具链，**编译/测试统一走 GitHub Actions**（用户指定）
  - 仓库 https://github.com/emoeem/mpv-emo （public，MIT）
- **环境事实**：本地为 Kali 容器，仅 git/python3；gh CLI keyring 认证可用（GH_TOKEN 环境变量已失效，需 `env -u GH_TOKEN` 调用）

## 2026-11-06 · 批次 2：P1+P2 核心实现与 CI

- **任务**：T3–T8
- **产出**：
  - `src/mpv/MpvEngine.{h,cpp}`：mpv_create/initialize、vo=libmpv、wakeup→GUI 事件泵、8 个观察属性、command/set/get/loadFile、node↔QVariant 转换
  - `src/mpv/MpvItem.{h,cpp}`：QQuickFramebufferObject 嵌入；render 线程 render context；状态镜像 Q_PROPERTY
  - `src/config/MpvConfigParser.{h,cpp}`：mpv.conf/input.conf 解析（行内注释语义差异、profile 段、引号分词、CRLF）
  - `qml/Main.qml`：控制栏（进度/音量/播放/停止/静音/全屏/打开）、快捷键（Space/←→/↑↓/M/F/O/Q/Esc）、控件自动隐藏、暂停指示、FileDialog
  - `src/main.cpp`：QCommandLineParser（位置参数加载媒体、--smoke-test）、销毁顺序契约
  - `tests/`：parser 全覆盖单测 + engine headless 冒烟（真实 mpv init、属性回环、观察信号、命令错误）
  - `.github/workflows/ci.yml`：Ubuntu 24.04 构建 + ctest + Xvfb/llvmpipe QML 启动冒烟
- **参考**：mpv-examples/libmpv/qml（公开 API 范式）、Haruna（QML 同步模式）、mpv 手册（config/input.conf 语法）
- **风险与待验证**：无本地编译，全部依赖 CI 反馈；QML 交互正确性 CI 只能验「能启动」，实机行为待用户验证；Windows/macOS 矩阵为下一批次

## 2026-11-07 · 批次 3：P3 音轨/字幕/播放列表/历史

- **任务**：T9–T12
- **产出**：
  - 音轨/字幕：`MpvItem` 新增 selectAudioTrack/selectSubtitleTrack（set aid/sid，id 或 "no"）与 loadExternalSubtitle（sub-add <path> select）；`MpvEngine::toMediaPath` 共享 file://→路径转换；Main.qml 右侧抽屉列出音轨/字幕（选中项高亮）、外挂字幕 FileDialog、T/S 快捷键
  - 播放列表：`MpvEngine` observe "playlist"(NODE)→playlistChanged 信号，`MpvItem` 镜像 Q_PROPERTY；UI 抽屉显示条目（playing 标记）、点击跳转（playlist-play-index）、多选加入（loadfile append-play）、prev/next（playlist-next/prev，</> 快捷键）、清空（playlist-clear）
  - 播放历史：`src/history/HistoryStore.{h,cpp}`——fileLoaded 记录 path/title/duration、5 秒 QTimer 从 time-pos 刷新 position、按 path 去重置顶、上限 50、`AppDataLocation/history.json`（QSaveFile 原子写）；main.cpp 以 context property `historyStore` 暴露；Main.qml idle 界面最近播放列表，点击 loadFile
  - 测试：`tests/test_history_store.cpp`（持久化回读、去重置顶+50 条上限、position 刷新不重排、损坏文件恢复、空 path/无媒体守卫），QStandardPaths::setTestModeEnabled + 注入文件路径，offscreen 可跑
- **决策**：
  - 播放历史为被动 QObject 服务，路径注入便于测试；每次变更原子落盘（≤50 条小文件）
  - aid/sid 一律以字符串下发（"2"/"no"），规避 mpv choice 属性的数值格式边界
  - 播放列表暂不落盘（mpv 会话内）；roadmap P3 原文的「持久化」移入 P3 收尾
- **风险与待验证**：同样无本地编译，CI 只验证「构建+单测+能启动」；抽屉/字幕/列表的实机交互待用户验证；test_history_store 仅覆盖无媒体场景，真实 loadfile 路径需集成测试（后续批次）

## 2026-11-07 · 批次 3 收尾：CI 结论与评审处置

- **任务**：T9–T12（P3：音轨/字幕、播放列表、播放历史及测试）
- **涉及 commit**（`git log --oneline` 实查；P3 批次实际 7 条——T9/T10 的 feature 提交在「最近 5 条」窗口之外，一并录入）：
  - `6cc4ec0` feat(ui): audio/subtitle track drawer with external subtitle loading (P3)（T9）
  - `38e608d` feat(mpv): playlist mirroring and playlist side drawer (P3)（T10）
  - `7e202b0` feat(history): HistoryStore with JSON persistence and idle-screen recent list (P3)（T11）
  - `ef2395a` test(history): headless Qt Test coverage for HistoryStore（T12）
  - `5e34bd0` docs: record P3 modules (tracks/playlist/history) and batch status
  - `986262b` fix(history): resolve the current path live in the 5s position refresh（评审修复）
  - `6b719ac` fix(ui): give the Tracks drawer button a distinct glyph（评审修复）
- **CI 结论**：**success**（`env -u GH_TOKEN gh run list --repo emoeem/mpv-emo` 实查）——批次主体推送（head `5e34bd0`，run 42257300914，1m10s）与评审修复推送（head `6b719ac`，run 42259020019，1m6s）两次 run 均 success，覆盖 Ubuntu 24.04 构建 + ctest + Xvfb QML 启动冒烟
- **评审发现的问题与处置**：
  1. **HistoryStore 5s position 刷新竞态**（`986262b` 修复）：`onTimerTick` 依赖 `fileLoaded` 时缓存的 `m_currentPath`；播放列表切到下一文件时，mpv core 可能已暴露新文件的 time-pos 而 FILE_LOADED 事件仍排队在 wakeup→GUI 事件泵中，落在该窗口的 tick 会把新文件的 ~0 位置写进上一条历史、毁掉断点续播。处置：tick 内与 time-pos 一道实时读 `getProperty("path")`（空 path = 加载间隙 = no-op），删除缓存成员，同时符合 architecture.md 决策 7「不缓存播放状态」。
  2. **Tracks 抽屉按钮字形易混**（`6b719ac` 修复）：♪♪（\u266B）与同排静音按钮的 ♪（\u266A）易误触。处置：改用 ⚙（\u2699）；评审另一候选 ≡（\u2261）因与相邻播放列表按钮 ☰（\u2630）形似被否决。
- **风险与待验证**：实机交互（抽屉/字幕/列表/历史回放）与播放列表磁盘持久化仍未完成，已列入 roadmap「后续批次」的 P3 收尾项

## 2026-11-07 · 批次 4：P9 三平台打包/发布管线与 v0.1.0-alpha 发布

- **任务**：release.yml（tag push 发布 / workflow_dispatch 干跑）三平台构建、干跑修复、评审加固、发布 v0.1.0-alpha
- **涉及 commit**（`git log --oneline` 实查）：
  - `fa9a89a` feat(release): three-platform build and release pipeline
  - `5057543` fix(release): build on macos-15 and force-include <iterator> on MSVC
  - `3ebb1d3` fix(release): apply /FIiterator before target creation so it reaches all TUs
  - `9156eb5` fix(release): patch Qt 6.4.3 stdext macros for MSVC 14.51 (VS 18)
  - `742e7a2` fix(release): apply review hardening (SHA pins, checksums, least privilege)
- **干跑轮次**（workflow_dispatch，run 列表/详情与失败日志实查）：
  | run | head | 结论 | 说明 |
  |---|---|---|---|
  | 42264806002 | `fa9a89a` | cancelled | 首轮：Windows Build 失败；macOS job（当时仍为 macos-14）cancelled。macos-14 已于 2026-11-02 从 GitHub Actions 退役（runner-images #15226，release.yml 注释） |
  | 42266499484 | `5057543` | failure | Windows Build：Qt 6.4.3 `qvarlengtharray.h` C3861/C2065（stdext 命名空间不存在），Linux 成功 |
  | 42267615552 | `3ebb1d3` | failure | Windows 同错误（/FIiterator 已在 cl 命令行上仍无效）；macOS Package 步骤 `hdiutil: create failed - Resource busy` |
  | 42268937918 | `9156eb5` | success | 三平台全绿，首次完整干跑 |
  | 42270913087 | `9156eb5` | success | 复跑确认可重复 |
  | 42274936655 | `742e7a2` | success | 评审加固后复跑 |
- **评审发现的问题与处置**：
  1. **macOS runner 退役**（`5057543`）：macos-14 退役 → 改用 macos-15（arm64）；Qt 6.4.3 的 clang_64 包为 universal，在 arm64 可用。
  2. **MSVC 14.51（VS 18）STL 删除 stdext::checked_array_iterator**（`5057543`→`3ebb1d3`→`9156eb5` 三轮）：Qt 6.4.3 `qcompilerdetection.h` 无条件把 QT_MAKE_CHECKED/UNCHECKED_ARRAY_ITERATOR 映射到 stdext 工厂函数。先试 /FIiterator（含修正到 target 创建前以覆盖 autogen TU），失败日志证明 namespace 本身不存在、强包含头文件无济于事；最终按 Qt 6.6 对 _MSC_VER >= 1938 的官方做法，改写 CI 安装目录中两处宏体为恒等并校验无 `stdext::` 残留，同时移除 /FIiterator（`9156eb5`）。
  3. **macOS hdiutil "Resource busy"**：Package 步骤代码未改动，下一轮干跑即通过 → 判定 runner 瞬态；macdeployqt 对 libiodbc/libpq sqldrivers 的 ERROR 行为非致命跳过，不影响 dmg 产出（42268937918 日志实查）。
  4. **评审加固**（`742e7a2`）：第三方 action 全部固定完整 commit SHA（版本注释见 release.yml，允许清单见 docs/allowed-actions.md）；外部下载固定版本化 tag + SHA256 校验（linuxdeploy 1-alpha-20251107-1、linuxdeploy-plugin-qt 1-alpha-20250213-1、zhongfly/mpv-winbuild mpv-dev 2026-11-04-ff8ca979b0）；权限最小化（顶层 contents: read，写权限仅 release job）；release job `fail_on_unmatched_files` 防空 Release；prerelease body 注明验证范围与 Gatekeeper 提示。
- **发布**：tag `v0.1.0-alpha` push → run 42276002560（success，3m23s）→ prerelease **https://github.com/emoeem/mpv-emo/releases/tag/v0.1.0-alpha**
- **最终资产**（`env -u GH_TOKEN gh release view v0.1.0-alpha` 实查）：
  - `EmoPlay-0.1.0-alpha-linux-x86_64.AppImage`（111,409,656 B ≈ 106 MB）
  - `EmoPlay-0.1.0-alpha-windows-x86_64.zip`（95,421,195 B ≈ 91 MB）
  - `EmoPlay-0.1.0-alpha-macos.dmg`（74,336,763 B ≈ 71 MB，仅 ad-hoc 签名）
- **风险与待验证**：Windows/macOS 产物未经实体机验证（experimental）；**签名/公证未做**（Windows 无 Authenticode，macOS 无 Developer ID + notarization）；Windows libmpv 固定 zhongfly 滚动 tag、macOS brew mpv 随上游滚动，ABI 漂移需跟踪；AppImage 组装失败时的 tar.gz 降级预案尚未实际触发过
