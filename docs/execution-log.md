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
