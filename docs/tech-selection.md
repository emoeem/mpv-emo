# 技术选型

## GUI 框架对比（调研结论）

候选：Qt6/QML、Rust(egui/GPUI)、Flutter、Electron。核心约束是 **libmpv 嵌入质量** 与 **三平台一致性**。

| 维度 | Qt6/QML | Rust egui/GPUI | Flutter | Electron |
|---|---|---|---|---|
| libmpv 集成 | 官方 render API 模式成熟（mpv-examples、Haruna、Celluloid 同类经验） | 需手写 GL/wgpu 桥，生态例证少 | FFI + 纹理桥复杂，桌面案例少 | 无法内嵌 libmpv 渲染，只能子进程 |
| GPU 渲染 | Qt RHI（GL/Vulkan/D3D/Metal） | wgpu（潜力大，成熟度中） | Skia/Impeller | WebGL 受限 |
| 成熟度/文档 | 最高 | 中 | 高（移动强、桌面弱） | 高 |
| 包体/内存 | 中 | 小 | 大 | 最大 |
| 许可 | LGPLv3（动态链接合规） | MIT/Apache | BSD | MIT |

**决策：Qt6/QML（C++20）**。理由：唯一同时满足「libmpv 官方集成范式 + 三平台 GPU 一致性 + 成熟控件生态」的选项。Rust/egui 记为远期评估项（P10 之后）。

构建系统：CMake ≥3.21 + Ninja。测试框架：Qt Test（随 Qt 一起分发，零额外依赖）。

## 同类项目对比与借鉴

调研对象（均为活跃开源项目）：

| 项目 | 技术栈 | 借鉴点 |
|---|---|---|
| [mpv-examples/libmpv/qml](https://github.com/mpv-player/mpv-examples) | C++/Qt Quick | render context 创建/渲染范式（本项目遵循同一公开 API 契约，代码为独立实现） |
| [Haruna](https://invent.kde.org/multimedia/haruna) (KDE) | QML + libmpv | QML 状态与 mpv 属性同步方式、设置界面组织 |
| [Celluloid](https://github.com/celluloid-player/celluloid) | GTK + libmpv | 播放列表/mpv 选项 GUI 映射的模型划分 |
| [IINA](https://github.com/iina/iina) | macOS 原生 | 播放器 UX（控制栏自动隐藏、 OSD 反馈） |
| [SMPlayer](https://github.com/smplayer-dev/smplayer) | Qt Widgets | 兼容配置管理、平台打包经验 |

## 依赖清单（当前）

- libmpv（LGPLv2.1+，动态链接）— 播放内核
- Qt 6.4+：Core / Gui / Quick / QuickControls2 — 应用框架
- 无其他运行时依赖

## 合规要点

- 本项目代码 MIT；libmpv 动态链接保持 LGPL 合规。
- 不引用 Mpv-infinity（非自由许可）的任何代码与资源，仅作功能参照。
