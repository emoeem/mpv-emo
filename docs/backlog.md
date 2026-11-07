# EmoPlay 全面优化 Backlog

> 本文档是 2026-11-07 全面评估后经所有者批准的优化清单。执行方式：六包并行 worktree 批次（R1–R6），全部合并后发布 v1.0。
> 状态标记：`in-flight`（本轮批次）/ `planned`（已批准待排）/ `blocked`（超出自主范围，见文末）。

## R1 构建现代化
- [ ] CI 增加 Qt 6.8 矩阵（install-qt-action，保留 Qt 6.4 兼容 job）
- [ ] qt_add_qml_module 迁移（qmlcachegen 字节码缓存；同步调整 main.cpp 的 qrc 加载 URL）
- [ ] HDR 直通调研文档（render API 仅 OpenGL→SDR 的架构边界；候选路径：wid 原生窗口嵌入 / libplacebo swapchain；需 HDR 实机验证）

## R2 可靠性
- [ ] OSD 错误 toast 层（errorOccurred/字幕加载失败 → 屏幕可见反馈）
- [ ] 日志系统：mpv log-message 观察 + QLoggingCategory 滚动文件日志 + 日志查看器抽屉
- [ ] 单实例（QLocalServer 文件参数转发）
- [ ] 崩溃信号捕获（SIGSEGV/SIGABRT → 崩溃日志落盘 + 重启提示）

## R3 数据层
- [ ] LibraryStore SQLite 后端（Qt6::Sql，接口不变，JSON 迁移）
- [ ] 播放列表持久化（退出保存、启动恢复可选）
- [ ] 缩略图缓存 LRU 上限（默认 500 张，启动清理）

## R4 用户体验
- [ ] 拖拽打开（文件与 URL）
- [ ] 章节 UI（chapter-list 抽屉 + 点击跳转）
- [ ] 截图封装（screenshot 命令 + 保存目录设置 + toast）
- [ ] 画质/EQ 预设导入导出（JSON）
- [ ] i18n 管线（lupdate/lrelease + zh_CN.ts）
- [ ] 双语 README（README.zh-CN.md）

## R5 工程质量
- [ ] 真实媒体集成测试（CI ffmpeg 生成样例 → headless loadfile → 断言播放属性）
- [ ] ASAN + UBSAN CI job
- [ ] qmllint 进 CI
- [ ] gcovr 覆盖率报告

## R6 安全与发布
- [ ] 插件信任模型 UI（权限明示 + 仅校验源开关）
- [ ] 更新检查默认关闭 + 低频 + SHA256 校验（release.yml 增加 sha256sums 产物）
- [ ] make install 目标（GNUInstallDirs + .desktop/图标）
- [ ] AppImage 更新元数据（--update-information）
- [ ] DEB 依赖列表精细化

## Blocked（超出自主范围，需所有者）
- Windows 代码签名 / macOS 公证（需购买证书）
- 真 HDR 直通的实机验证（需 HDR 硬件）
- 匿名崩溃上报服务端（需基础设施）
