# Changelog

从 `docs/execution-log.md` 提炼；细节以执行记录、各 PR 与 `docs/architecture.md`
为准。格式参考 Keep a Changelog；预发布序列 alpha → beta → rc。当前项目版本见
`CMakeLists.txt` 的 `project(VERSION)`，`--version` 输出同源。

## [0.1.0-alpha] — 2026-11-07 已发布（prerelease）

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

## [0.2.0-beta] — 未发布

P4–P8 功能与 P9 更新器（execution-log 批次 5–7 及 P6/P8 批次、本批次）。

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
- **P6 媒体库**：文件夹清单持久化、后台扫描线程、去重与 last-played 时间戳
  （file-loaded 驱动）、QAbstractListModel + LibraryDrawer。
- **P7 音乐模式 v1**：.lrc 解析（多时间戳/BOM/CRLF/元数据标签）、纯音频判定
  （albumart 封面轨不排除）、歌词高亮滚动、10 段均衡器（`af=equalizer=...`
  链 + 平直/低音增强/人声/现场预设 + music 节持久化，真引擎 af 回读测试）。
- **P8 插件商店**：manager.json 兼容清单解析、内置目录（qrc 资产）、git 驱动
  的安装/更新（check 只读）/卸载、启用插件的 scripts 选项合并（不替换用户
  导入清单）、state.json 与错误上报、PluginDrawer。
- **P9 更新器**：`UpdateChecker` 每次启动一次 GitHub releases/latest 检查
  （User-Agent 必设、15s 传输超时、失败静默），tag 与运行版本比对命中后
  控制栏角标提示、点击打开 Release 页（Qt.openUrlExternally）；
  **绝不自动下载/安装**。
- **打包**：CPack DEB/RPM 最小配置（包名 emoplay；注明 minimal，正式 Linux
  分发仍走 AppImage）。
- **P10 工具与文档**：`tools/benchmark.sh` 解码基准框架（缺工具时明确跳过）、
  `docs/testing-report.md`（自动化 17 套件总览 + 实机平台×GPU×功能手工矩阵
  模板）、本 CHANGELOG。

### Tests

- 单测从批次 5 收尾的 8 套件扩到 **17 套件**（新增 quality×2、lrc、music×2、
  plugin×2、library、update_checker 等）；CI 全绿记录见 execution-log 各批次
  「CI 结论」。明细与盲区清单见 `docs/testing-report.md`。

## [计划中] — v1.0-rc

目标（mpv.md P10 验收）：三平台全面测试无严重 bug、性能达标、正式版发布。

### Planned

- **P9 收尾**：Windows Authenticode / macOS Developer ID + 公证；系统集成
  （文件/协议关联、外部播放器移交）；deploy 式首启体检（四态
  available/degraded/unavailable/unverified）；设置中心与键位编辑器
  （冲突检测、捕获录入）、窗口行为设置项。
- **P10 收尾**：`docs/testing-report.md` 手工矩阵在实体机铺开并留档；
  `tools/benchmark.sh` 性能基线落表；崩溃日志与 `mpv log-message` 收集链路；
  clang-format 固化 + CI 格式检查。
- **既有「后续批次」项**（roadmap）：播放列表磁盘持久化；Windows/macOS CI
  矩阵；P4 配置回写；P5 off 档恢复用户导入值、GPU 档位检测与压制组词库、
  RIFE/VS 补帧与 ICC 评估；P7 专辑浏览/封面墙、gapless/loop、passthrough
  互锁、EQ 防抖。
