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

## 2026-11-07 · 批次 5：原生化计划文档 + P4 配置兼容层（批次收尾）

- **任务**：原生化计划文档；T13–T19（P4 兼容层首批：扫描、导入规则、决策持久化、按键转发、CompatController、启动序、首启向导及测试）
- **产出**：
  - `docs/nativeization-plan.md`（`c85604a`）：mpv-Emo（只读快照 `70d6eb4`）逐项映射到 EmoPlay 六类落点（P5 画质引擎 / P6 媒体库 / P7 音乐模式 / P8 插件仓库 / P9 平台集成 / 保留为脚本），作为 P5–P8 排期依据；§4 定义 P8 插件清单**格式目标 = manager.json 兼容**（字段语义 + check/replace 双模式 + 「本地定制永不覆盖」原则）
  - 扫描/导入：`src/compat/MpvProfileScanner.{h,cpp}`（T13，`d815234`）定位 mpv 配置目录并产出 ImportSummary；`src/compat/MpvImportRules.{h,cpp}`（T14，`9df56d0`）强制 `vo=libmpv`/`osc=no`/`terminal=no`、剥离 include/profile/scripts/input-conf/config-dir、写入路径重定向（watch-later/screenshot/shader-cache/icc-cache/demuxer-cache → AppData），其余选项按 mpv 语义通过
  - 决策持久化：`src/settings/AppSettings.{h,cpp}`（T15，`1c480ed`）`AppDataLocation/settings.json`（QSaveFile 原子写）记录 importDecision / 选择性清单 / ImportSummary 快照 / dontAskAgain
  - 按键转发：`src/compat/MpvKeyName.{h,cpp}` + `MpvEngine::keyPress`（T16，`85e74c3`）未消费按键 Qt 键名→mpv `keypress`，使 input.conf 在嵌入 UI 生效
  - 编排与启动：`src/compat/CompatController.{h,cpp}`（T17，`d75b6bb`）决策→选项组装；`src/main.cpp` 启动序 settings→应用→initialize→向导（T18，`6cfb1c9`）；`qml/ImportWizard.qml` 首启向导（T19，`9848ceb`）完整/选择性/独立 + 不再询问
  - 设计文档：architecture.md「配置兼容层（P4）」章节（`6dd35d4`；评审后 `48087c4` 同步）
  - 测试：新增 test_compat_scanner / test_import_rules / test_app_settings / test_mpv_key_name / test_compat_controller，扩展 test_mpv_engine（load-scripts 强制 no 等）；`tests/fixtures/`
- **CI 结论**：**success**（最终 head `a35774a`，run 42296377325，1m16s，Ubuntu 24.04 构建 + ctest 8/8 + Xvfb QML 冒烟；`env -u GH_TOKEN gh run list` / `gh run view --log-failed` 实查）。批次期间为无本地编译下的 CI 修复链（失败 run 逐轮实查）：
  1. `d815234`/`9df56d0`/`1c480ed`：test_compat_scanner、test_import_rules Subprocess aborted（`qt.qpa.xcb: could not connect to display`，QTEST_MAIN 需 GUI 平台）→ `7cb8827` 纯逻辑测试改 QTEST_GUILESS_MAIN；`1c480ed` 另有 test_app_settings.cpp 编译错（QCOMPARE 被 braced-init 逗号拆成 3 参数、`file` 变量被 QString 遮蔽）
  2. `85e74c3`→`7cb8827` 五连败：QT_NO_CAST_FROM_ASCII 下 MpvKeyName.cpp `QString(const char*)` is private → `afba3e7`；test_app_settings 宏/遮蔽错误 → `449f46e`；随后 ctest 期望失败 → `59cc462` 修正两处期望（profiles.conf fixture 计数 5、标点键规范形 `shift+.`）
  3. ctest 8/8 全过后 QML 冒烟 "QQmlApplicationEngine failed to load component"（ImportWizard 相邻字符串字面量在 JS 是语法错误，整个类型不可用）→ `182cc2c` 修复，run 42290232856 success
  4. 评审修复期 4 连败：`ensureOwnInputConf() const` 内 emit `errorOccurred`（discards qualifiers，CompatController.cpp:186）→ `a35774a` 改非 const，最终 run 42296377325 success
- **涉及 commit**（`git log --oneline` 实查，`c85604a`…`a35774a` 共 19 条）：
  - `c85604a` docs: add nativeization plan mapping mpv-Emo scripts to EmoPlay phases
  - `d815234` feat(compat): add MpvProfileScanner for mpv config discovery（T13）
  - `9df56d0` feat(compat): add import rules with forced embedding options（T14）
  - `1c480ed` feat(settings): persist mpv import decision to settings.json（T15）
  - `85e74c3` feat(compat): forward unhandled keys to mpv input layer（T16）
  - `d75b6bb` feat(compat): add CompatController for import decision and option assembly（T17）
  - `6cfb1c9` feat(app): wire compat startup sequence in main.cpp（T18）
  - `9848ceb` feat(ui): add first-run mpv config import wizard（T19）
  - `7cb8827` fix(compat): use QTEST_GUILESS_MAIN for pure-logic tests（CI 修复）
  - `afba3e7` fix(compat): build key names without ASCII casts under QT_NO_CAST_FROM_ASCII（CI 修复）
  - `449f46e` fix(compat): repair test macro commas and shadowed variable（CI 修复）
  - `59cc462` fix(compat): correct two test expectations（CI 修复）
  - `6dd35d4` docs(arch): record P4 compat layer design and module table
  - `182cc2c` fix(ui): replace adjacent string literals in QML bindings（CI 修复）
  - `b98d7bb` fix(compat): make script and input.conf loading exact and consent-driven（评审修复）
  - `e24f143` fix(compat): match mpv's Unix config dir lookup order（评审修复）
  - `66d1c3b` fix(compat): redirect demuxer-cache-dir writes to app data（评审修复）
  - `48087c4` docs(arch): sync compat docs with review fixes
  - `a35774a` fix(compat): make ensureOwnInputConf non-const（评审修复期编译修复）
- **评审发现的问题与处置**（4 项，均对 mpv v0.37.0 源码核实后处置）：
  1. **scripts 分隔符错误 + libmpv 无同意自动加载用户配置**（`b98d7bb`）：`--scripts` 是 OPT_PATHLIST（Unix `:` / Windows `;`，options.rst `--scripts=file1.lua:file2.lua:...`），此前以 `,` 连接使整串成为单个不存在的路径、显式脚本导入静默失效（完整导入模式下被下述自动加载 bug 掩盖）；且 load-scripts 默认 yes 会自动加载 config 目录 `scripts/` 下全部脚本（player/scripting.c `mp_load_scripts`），input.conf 在 `mpv_initialize` 内无条件加载、input-conf 打不开时回退 config 目录（player/main.c，不受 `config=no` 门控）。处置：`initialize` 强制 `load-scripts=no`（显式列表仍生效）、按平台分隔符连接；用户未显式导入 input.conf（未决策/skip/未勾选/独立模式）时 `input-conf` 恒定钉到 EmoPlay 自有空 input.conf，创建失败经 `errorOccurred` 上报——杜绝静默回退加载用户文件。
  2. **Unix config 目录查找顺序不符 mpv 语义**（`e24f143`）：mpv 在 `XDG_CONFIG_HOME` 已设时**独占**使用（osdep/path-unix.c:44-48），不再查 `~/.config/mpv`；legacy `~/.mpv` 仅在主候选不存在时回退（:70-76）。原「home 优先」顺序可能扫描并导入 mpv 自身忽略的目录（如设了 XDG_CONFIG_HOME 又留有旧 ~/.config/mpv）；macOS 同 path-unix.c，Unix 分支覆盖之，Windows 维持 %APPDATA%/mpv。顺序断言翻转并补 legacy 目录用例。
  3. **demuxer-cache-dir 缺席写路径重定向**（`66d1c3b`）：用户值（如 `~~` 相对路径）+ cache-on-disk 会把缓存写进用户 mpv 目录，违反「绝不写用户目录」原则；其默认值本指向系统缓存目录，仅显式值触发——重定向到 AppData/demuxer_cache，与其余写路径一致。
  4. **ensureOwnInputConf 误标 const**（`a35774a`）：Qt 信号方法非 const 成员，const 函数内 emit `errorOccurred` 无法编译（discards qualifiers）；该函数本就写文件、可能 emit——去 const。
- **风险与待验证**：无本地编译，CI 只验证「构建 + 单测（8/8）+ QML 能启动」；真实 `~/.config/mpv` 的导入、向导交互、按键转发覆盖面（Slider 焦点消费方向键等已知边界见 architecture.md）待实机验证；profile 定义与 `profile=` 引用不应用（原生等价物归 P5）；GUI 键位编辑器、设置页入口（`compatController.requestWizard`）、配置回写属后续批次

## 2026-11-07 · 批次 6：P5 画质引擎 v1（批次收尾）

- **任务**：T20–T26（P5 v1：决策表、属性观察与镜像、settings 持久化、QualityController、启动接线、deband 偏好、画质抽屉及测试）
- **涉及 commit**（`git log --oneline` 实查，`7959f01`…`6915d2d` 共 18 条）：
  - 实现：
    - `7959f01` feat(quality): add QualityProfileEngine decision table with tests（T20）
    - `d1a1b62` feat(mpv): observe video params, fps and drop-count properties（T21）
    - `ab6de2e` feat(mpv): mirror video geometry, fps, drop stats and HDR state on MpvItem（T22，含 2s 掉帧采样）
    - `d4e3e51` feat(settings): persist the quality section in settings.json（T23）
    - `9a191ba` feat(quality): add QualityController with apply path and auto-downgrade（T24）
    - `6d471be` feat(app): wire the quality controller into startup（T25）
    - `7c643c9` feat(quality): user deband preference override（T25 deband 偏好）
    - `167dc5b` feat(ui): add the quality settings drawer（T26）
    - `78d4e06` docs: record the P5 v1 quality engine architecture and roadmap status
  - CI 修复：`7e7ff26`（namespace alias）、`8cd4c87`（测试着色器文件集）、`cc9472c`（MPV_FORMAT_STRING setProperty）
  - 评审修复：`826d992`、`c8bbbc6`、`00229a2`、`0ff92e6`、`7fa132e`、`bcc34dc`、`6915d2d`（docs 计数更正）
- **CI 结论**：**success**（`env -u GH_TOKEN gh run list/view` 实查）——最终 head `6915d2d`，run 42314902255（job Linux/Ubuntu 24.04 (Qt 6.4, GCC 13) 1m27s，日志实查「100% tests passed, 0 tests failed out of 10」）；v1 主体落地时 head `167dc5b` run 42305521632 亦 success。批次内 3 轮失败（无本地编译 CI 修复链，失败 run 日志实查）：
  1. `7959f01` run 42300687754 failure：test_quality_profile_engine.cpp 编译败（`using QPE = QualityProfileEngine;` 是类型别名不能别名命名空间，GCC `'QPE' does not name a type` 连锁报错）→ `7e7ff26` 改 namespace alias
  2. `7e7ff26` run 42301137837 failure：ctest SIGSEGV，1/9——anime 用例只提供 SSimSuperRes.glsl，group-A 解析为空列表后 first() 解引用 → `8cd4c87` 为 anime 路径提供 Anime4K 文件集
  3. `9a191ba` run 42303386212 failure：ctest SIGSEGV，1/10——`mpv_set_property(MPV_FORMAT_STRING)` 传 char* 本体而非 char* 地址（client.c 的 mpv_set_property_string 转发 &data），首个字符串属性写入即崩 → `cc9472c` 修复并补回读回归测试（P3 轨道选择的同型隐患一并消除）
- **评审发现的问题与处置**（6 项 + 1 项 docs 更正，处置均以对应修复 commit 记载为准）：
  1. **换文件后旧决策残留**（`826d992`）：file-loaded 仅在发生过自动降档时重算，且 video-params/container-fps 迟到无重算触发——旧文件的 scale/tone-mapping/分辨率分组着色器会带进下一个文件。处置：file-loaded 无条件重置降档并 applyProfile()；连接 videoParamsChanged/containerFpsChanged 重跑决策表。
  2. **参数未知时挂最重着色器组**（`c8bbbc6`）：anime4kGroupForHeight(0) 落到 A+A 全链，且 height/fps=0、isHdr=false 绕过 HFR/HDR 避让——file-loaded 时点（参数未解码）恒挂最重组。处置：shader 组块以 height>0 门控，参数未知不挂组（scale/deband 仍应用），参数到达后重跑（与上条联动）。
  3. **SDR 源不管理 tone-mapping 系列**（`00229a2`）：活动非 HDR 分支不发 tone-mapping/target-colorspace-hint，HDR 文件设置的 bt.2390 + hint=yes 残留到后续 SDR 文件，还会覆盖用户导入的 mpv.conf 值。处置：引擎活动期对称管理（HDR→tier/用户选择 + hint=yes；SDR→auto + hint=no）；inert-at-off 规则不变。
  4. **deband/scale 覆盖值未校验**（`0ff92e6`）：手改 settings.json 可把 mpv v0.37.0 不支持的 `--scale` 值（如 spline36）逐字下发。处置：新增 isSupportedScale 按手册 --scale 选项校验、非法回退档位选择；deband 显式规范化（"yes"/"no" 之外一律 auto）。
  5. **Anime4K A+A 链顺序依据过时**（`7fa132e`）：原顺序出自 v4.0.1 GLSL_Instructions.md，上游已缩为 stub；现行链定义在 md/Template/*/input.conf，Mode A+A (Fast)（CTRL+4）的第二枚 Restore_CNN_S 应在首次 upscale 之后、downscale 准备之前。处置：调整链序并更新引证注释，测试期望移至 index 3。
  6. **属性不可取回的事件被整体丢弃**（`bcc34dc`）：mpv 对取不到的观察属性报 MPV_FORMAT_NONE（client.h mpv_observe_property 注明，且每次观察都有首次通知），P5 分支要求精确值格式致事件全弃、跨文件残留镜像（上一文件的 HDR 状态点亮到纯音频文件）。处置：video-params/container-fps/frame-drop-count/current-vo/video-format 五分支接受 MPV_FORMAT_NONE 并发清除值（空 map/0/空串）；既有播放属性语义不动（改动超出本批次范围）。
  7. （docs）**roadmap 单测计数 11/11 有误**（`6915d2d`）：run 42305908172 日志实为「0 tests failed out of 10」→ 更正为 10/10。
- **风险与待验证**：无本地编译，CI 只验证「构建 + 单测（ctest 10/10）+ QML 能启动」；画质抽屉、自动降档、着色器挂载/让位的实机交互待验证；tone-mapping 仅限嵌入 classic gpu 渲染器可用值（spline/bt.2446a 为 gpu-next 专属）；RIFE/VS 补帧、ICC 自动加载、着色器商店、GPU 档位检测/压制组词库、off 档恢复用户导入值均未含（roadmap「后续批次」P5 收尾项）

## 2026-11-07 · 批次 7：P7 音乐模式 v1（music-mode + equalizer-gui 简化版）

- **任务**：T27–T31（nativeization-plan music-mode + equalizer-gui 简化版；分支 `feat/p7-music-mode`，一个功能一个 commit）
- **产出**：
  - 歌词解析：`src/music/LrcParser.{h,cpp}`（T27，`d88afed`）纯逻辑 .lrc 解析（`[mm:ss(.xx/.xxx)]` 1–3 位小数、多时间戳行、`[ti]/[ar]/[al]`、UTF-8 BOM、CRLF；无时间戳行与未知标签跳过）+ `lrcCandidates()` 同名 .lrc（→.LRC）匹配规则；test_lrc_parser 全覆盖
  - 引擎观察：MpvEngine `metadataChanged(QVariantMap)`（T28，`805811a`）照 P5 模式观察 metadata(NODE)，不可用（NONE）清空镜像
  - 音乐模式服务：`src/music/MusicController.{h,cpp}`（T29，`6233547`）纯音频判定（track-list 无 video **且** 音频扩展名；`albumart` 封面轨不排除——mpv 0.37 input.rst `track-list/N/albumart` 核实）+ 同名 .lrc 解析 + `lyrics`/`lyricIndex`（time-pos 驱动）+ metadata 中继；判定/索引为纯函数可无引擎单测
  - 均衡器 v1（T30，`db906ea`）：10 段（31.25 Hz–16 kHz，`equalizer=f=..:t=q:w=1.4:g=<dB>` 链）+ 预设（flat/bass/vocal/live，拖动转 custom）+ settings.json `music` 节持久化（AppSettings 复用 QSaveFile 契约）；inert 规则（未启用绝不写 af）；单测含真引擎 `af` 回读（m_option.c name/params 节点序列化）——mpv 0.37 + libavfilter 接受该链的运行时证明
  - QML 与接线（T31，`712cf51`）：`qml/MusicPage.qml` 独立文件（大标题 media-title、歌词高亮滚动、EQ 折叠面板，命令式同步）；Main.qml 仅加 `musicMode` 只读绑定 + Loader 挂载点；main.cpp 构造 MusicController（共享 compat 层 AppSettings）+ 上下文属性；test_music_page 离屏实例化（CI 冒烟不触发 Loader，页面需自有启动测试）
- **mpv/FFmpeg 核实结论**（本地无工具链，逐项对上游源码实查后落码）：
  1. mpv v0.37.0 无内置 superequalizer：filters/user_filters.c 仅注册 `af_lavfi`/`af_lavfi_bridge`，其余名字经 libavfilter 桥（check_unknown_entry→mp_lavfi_is_usable）
  2. FFmpeg n6.1 superequalizer 为 18 固定频段（65.4 Hz–20 kHz，`1b`..`18b`），增益 0..20 无负值、**无 `bands=` 参数**（任务书预设格式不存在）→ 按 fallback 条款改用 `equalizer` 串联（af_biquads.c：f/t/w/g，g ∈ -900..900，与 10 段 ±12 dB UI 一一对应）
  3. `af` 读回为 `{name,label,enabled,params}` 节点表（m_option.c obj settings list NODE 序列化），据此设计真引擎回读断言
- **涉及 commit**：`d88afed`、`805811a`、`6233547`、`db906ea`、`712cf51` + docs（本条与 architecture/roadmap 更新）
- **已知限制（记入 roadmap「P7 收尾」）**：写 af 整链替换（EQ 启用期覆盖导入 af，唯一管理者语义同 P5 glsl-shaders）；无 passthrough 互锁；滑条每整数步重建 af 链（无防抖）；专辑浏览/封面墙、gapless/loop、歌词 `[offset:]` 不在 v1
- **CI 结论**：待本批次 PR run 实查后回填
