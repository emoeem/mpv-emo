# EmoPlay 架构设计

> 对应总体规划见 `mpv.md`（P0–P10）。本文档记录已实施和已定的架构决策，随阶段推进持续更新。

## 分层总览

```
┌─────────────────────────────────────────────────────────┐
│ UI 层 (QML)                                             │
│   Main.qml — 播放器窗口、控制栏、快捷键、文件对话框        │
│             音轨/字幕抽屉、播放列表抽屉、最近播放          │
├─────────────────────────────────────────────────────────┤
│ 播放器服务层 (C++ / Qt)                                  │
│   MpvItem   — QQuickItem 视频渲染控件（状态镜像给 QML）   │
│   MpvEngine — libmpv 客户端句柄封装（命令/属性/事件桥）    │
│   MpvConfigParser — mpv.conf / input.conf 纯逻辑解析      │
│   HistoryStore — 播放历史（JSON 持久化，≤50 条）          │
├─────────────────────────────────────────────────────────┤
│ 引擎适配层 (libmpv render API)                           │
│   mpv_render_context + QQuickFramebufferObject (OpenGL)  │
└─────────────────────────────────────────────────────────┘
```

## 模块与职责

| 模块 | 文件 | 职责 |
|---|---|---|
| 引擎封装 | `src/mpv/MpvEngine.{h,cpp}` | 拥有 `mpv_handle`；初始化（vo=libmpv, hwdec=auto-safe, keep-open）；`command/setProperty/getProperty/loadFile`、`toMediaPath`（file://→本地路径）；wakeup 回调→GUI 线程事件泵；观察属性（time-pos/duration/pause/volume/mute/media-title/track-list/playlist/idle-active）→Qt 信号 |
| 渲染控件 | `src/mpv/MpvItem.{h,cpp}` | `QQuickFramebufferObject` 子类；render 线程持有 `mpv_render_context`；把引擎状态（含 trackList/playlist）镜像为 Q_PROPERTY；提供 play/pause/seek、音轨/字幕选择（set aid/sid）、`sub-add` 外挂字幕、loadfile append-play / playlist-next/prev / playlist-play-index / playlist-clear 等命令方法 |
| 配置解析 | `src/config/MpvConfigParser.{h,cpp}` | 解析 `mpv.conf`（无行内注释、profile 段）与 `input.conf`（引号分词、空白后 # 才是注释）；纯逻辑、可单元测试；P4 兼容层的数据基础 |
| 播放历史 | `src/history/HistoryStore.{h,cpp}` | QObject 服务；engine `fileLoaded` 时记录 path/title/duration，5 秒 QTimer 从 `time-pos` 刷新当前条目 position；按 path 去重置顶、上限 50 条；`AppDataLocation/history.json`（QSaveFile 原子写入），QML 经 `entries` 属性读取 |

## 关键决策与理由

1. **Qt6/QML + libmpv 动态链接**（P0 选型结论，详见 `tech-selection.md`）。
2. **视频嵌入用 QQuickFramebufferObject + mpv render API**：这是 mpv 官方示例（mpv-examples/libmpv/qml）验证过的模式；render context 在 render 线程的 GL context 上创建，`MPV_RENDER_PARAM_FLIP_Y=1` 配合 Qt 的 FBO 镜像合成。相比 `QQuickRenderControl`/自绘 render node，FBO 方案实现面最小、可回滚。
3. **单一 MpvEngine 实例**：`MpvEngine::instance()` 静态可达，`MpvItem` 据此取 handle。播放状态只有一份权威来源（mpv 本体）；UI 写操作只下发请求，读状态一律来自观察属性事件回显，避免双向绑定打架。
4. **线程模型**：libmpv client API 线程安全；事件通过 `mpv_set_wakeup_callback` → `QMetaObject::invokeMethod(queued)` 泵到 GUI 线程；render context 严格在 Qt Quick render 线程创建/使用/销毁（`mpv_render_context_create/free`）。
5. **销毁顺序契约**：`main.cpp` 中 `MpvEngine` 声明在 `QQmlApplicationEngine` 之前，栈回收时 QML 对象（含 render context）先于 mpv core 析构。
6. **配置解析是纯函数库**：无 mpv、无 GUI 依赖，直接单测。P4 兼容层在此之上做扫描/导入/回写。
7. **播放历史是被动服务**：只订阅 `MpvEngine` 信号（`fileLoaded` / `idleActiveChanged`）并按需 `getProperty`（path / media-title / duration / time-pos），不缓存播放状态；存储路径可注入以便测试（`QStandardPaths::setTestModeEnabled` 下 headless 可测），每次变更即 QSaveFile 原子落盘。

## QML 状态同步模式（重要约定）

QML 中 Slider 等控件的值用 **命令式同步**（`Connections { function onXxxChanged() }`）而不是声明式绑定：用户拖动会对 `value` 做命令式写，声明式绑定会被一次性销毁导致后续失联。新增进度/音量类控件时遵循同一模式。

## 已知限制 / 待改进（记入 roadmap）

- 渲染后端目前固定走 OpenGL render API；Vulkan/D3D/Metal 直通（P5+）。
- 音量 UI 语义：mpv volume 可 >100，滑条钳制 0–100，快捷键可到 130（待统一）。
- 播放列表只存在于 mpv 会话内，未持久化到磁盘（roadmap P3 原文含「持久化」，本批次未包含，待后续批次）；历史记录按上次 position 点击回放，暂不支持断点续播选项。
- 配置目录扫描与导入未实现（P4，解析器已就绪）。
