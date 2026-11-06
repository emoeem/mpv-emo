# 路线图与任务状态

原始规划来自 `mpv.md`（P0–P10）。下表为实际执行视图；状态：`done / in-progress / pending / blocked`。

## 阶段总览

| 阶段 | 内容 | 状态 | 验收依据 |
|---|---|---|---|
| P0 | 选型、架构文档、仓库与 CI 骨架 | **done** | docs/*, GitHub Actions 绿 |
| P1 | libmpv 嵌入 + 渲染上下文 | **in-progress** | Linux CI 构建+冒烟通过；Win/mac 待 CI 矩阵（后续任务） |
| P2 | 播放器基础 UI（控制栏/快捷键/全屏/打开文件） | **in-progress** | Linux CI 冒烟通过；实机交互待验证 |
| P3 | 音轨/字幕/播放列表/历史 | **done** | Linux CI 构建+单测+冒烟 success（评审修复后 `6b719ac`）；实机交互验证与播放列表持久化列入后续批次 |
| P4 | mpv 配置兼容层（扫描/导入/回写） | pending（解析器已就绪+单测） | — |
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

## 后续批次（按优先级）

1. **P3 收尾**：播放列表磁盘持久化（roadmap 原文项，本批次未含）；音轨/字幕/播放列表实机交互验证。
2. **P1 收尾**：Windows（MSVC + 预编译 libmpv）/ macOS（brew mpv）CI 矩阵。
3. **P4 兼容层**：`~/.config/mpv` 扫描、导入向导（完整/选择性/独立）、脚本目录管理。
4. **工程质量**：clang-format 固化 + CI 格式检查；QML 交互的集成测试（xvfb + 真实媒体样本）；崩溃日志与 `mpv log-message` 收集。
