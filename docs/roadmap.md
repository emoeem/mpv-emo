# 路线图与任务状态

原始规划来自 `mpv.md`（P0–P10）。下表为实际执行视图；状态：`done / in-progress / pending / blocked`。

## 阶段总览

| 阶段 | 内容 | 状态 | 验收依据 |
|---|---|---|---|
| P0 | 选型、架构文档、仓库与 CI 骨架 | **done** | docs/*, GitHub Actions 绿 |
| P1 | libmpv 嵌入 + 渲染上下文 | **in-progress** | Linux CI 构建+冒烟通过；Win/mac 待 CI 矩阵（后续任务） |
| P2 | 播放器基础 UI（控制栏/快捷键/全屏/打开文件） | **in-progress** | Linux CI 冒烟通过；实机交互待验证 |
| P3 | 音轨/字幕/播放列表/历史 | **done** | Linux CI 构建+单测+冒烟 success（评审修复后 `6b719ac`）；实机交互验证与播放列表持久化列入后续批次 |
| P4 | mpv 配置兼容层（扫描/导入/回写） | **in-progress**（实现说明：扫描/导入/向导/脚本与 input.conf 生效已落地并附单测；状态终审待 CI 全绿后由文档管理员确认；GUI 键位编辑器与设置页入口、回写属后续批次） | execution-log 批次 5：`d815234` 扫描器、`9df56d0` 导入规则、`1c480ed` settings 持久化、`85e74c3` 按键转发、`d75b6bb` CompatController、`6cfb1c9` 启动序、`9848ceb` 向导；设计见 architecture.md「配置兼容层」 |
| P5 | 高级画质（滤镜/HDR/tone-mapping UI） | pending | — |
| P6 | 媒体库与文件管理 | pending | — |
| P7 | 音乐模式（歌词/EQ） | pending | — |
| P8 | 插件生态（脚本/着色器管理） | pending | — |
| P9 | 平台集成（打包/更新/系统集成） | **in-progress** | 打包/发布管线已落地：release.yml 三平台构建（Linux AppImage / Windows zip / macOS dmg）+ tag push 自动发布，v0.1.0-alpha prerelease 已发布（execution-log 批次 4）；**签名/公证未做**（Windows 无 Authenticode、macOS 仅 ad-hoc 签名），更新器与系统集成（文件关联/协议关联等）未做 |
| P10 | 全面测试与发布 | pending | — |

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

## 后续批次（按优先级）

1. **P3 收尾**：播放列表磁盘持久化（roadmap 原文项，本批次未含）；音轨/字幕/播放列表实机交互验证。
2. **P1 收尾**：Windows（MSVC + 预编译 libmpv）/ macOS（brew mpv）CI 矩阵。
3. **P4 兼容层**：`~/.config/mpv` 扫描、导入向导（完整/选择性/独立）、脚本目录管理。
4. **工程质量**：clang-format 固化 + CI 格式检查；QML 交互的集成测试（xvfb + 真实媒体样本）；崩溃日志与 `mpv log-message` 收集。
