# GitHub Actions 允许清单与固定策略

> 适用范围：本仓库所有 GitHub Actions workflow。当前在 `.github/workflows/release.yml`
> （唯一持有 `contents: write` 的 workflow）中按"完整 commit SHA + 版本注释"执行；
> `ci.yml` 只使用 `actions/checkout`（tag 引用，job 无写权限），后续如加平台矩阵
> 应同样改为 SHA 固定。

## 为什么固定 SHA

`uses: owner/action@v2` 这类大版本 tag 是**可变引用**——仓库维护者随时可以把
tag 指到新 commit。持 `contents: write` 的 action（如发布用的
`softprops/action-gh-release`）一旦被投毒或上游被攻破，tag 引用会静默引入恶意
代码。固定到完整 commit SHA 后，引用内容不可变；升级必须显式改 SHA，即
显式评审。

## 允许清单（release.yml 实际使用的全部 action）

| Action | 固定 SHA | 版本 | 用途 | 备注 |
|---|---|---|---|---|
| `actions/checkout` | `11d5960a326750d5838078e36cf38b85af677262` | v4.4.0 | 拉取源码 | GitHub 官方 |
| `actions/upload-artifact` | `ea165f8d65b6e75b540449e92b4886f43607fa02` | v4.6.2 | 上传各平台产物 | GitHub 官方 |
| `actions/download-artifact` | `d3f86a106a0bac45b974a628896c90dbdf5c8093` | v4.3.0 | release job 汇总产物 | GitHub 官方 |
| `jurplel/install-qt-action` | `b344308e4483fc892e0d27b3cd1b173ebdd95f06` | v4.5.0 | 安装 Qt 6.4.3 | 第三方，唯一安装 Qt 的途径 |
| `ilammy/msvc-dev-cmd` | `0b201ec74fa43914dc39ae48a89fd1d8cb592756` | v1.13.0 | MSVC 环境注入 | 第三方 |
| `softprops/action-gh-release` | `3bb12739c298aeb8a4eeaf626c5b8d85266b0e65` | v2.6.2 | 创建 prerelease 并上传资产 | 第三方，持 `contents: write`，仅授予 release job |

除上述清单外不允许新增第三方 action；新增前必须：

1. 确认无可用的官方 `actions/*` 等价物或一步 shell 替代；
2. 固定到具体 release 的完整 commit SHA，并在 `uses:` 行尾用注释写明版本；
3. 更新本清单。

## 升级某个 action 的流程

1. 查看上游 release notes / diff（重点：`action.yml` 的新输入、权限变化）；
2. `gh api repos/<owner>/<repo>/commits/<tag> --jq .sha` 取新 commit SHA；
3. 更新 workflow 中的 SHA 与版本注释，并同步更新本清单的"固定 SHA/版本"列；
4. 手动 `workflow_dispatch` 干跑一遍 release.yml，确认三平台构建仍绿。

## 权限模型

- workflow 顶层 `permissions: contents: read`；
- 仅 `release` job 额外声明 `contents: write`（创建 Release 的唯一位置）；
- 不引用任何 secrets，只使用默认 `GITHUB_TOKEN`。

## 外部下载的固定与校验

workflow 中所有第三方二进制下载均固定版本化 tag 并校验 SHA256
（校验和为维护者本地下载该资产一次算得，记录在 workflow `env:` 中）：

| 下载 | 固定 tag | SHA256 前缀 | 用途 |
|---|---|---|---|
| `zhongfly/mpv-winbuild` mpv-dev | `2026-11-04-ff8ca979b0` | `1779751d…` | Windows libmpv SDK |
| `linuxdeploy/linuxdeploy` | `1-alpha-20251107-1` | `c20cd71e…` | AppImage 组装 |
| `linuxdeploy/linuxdeploy-plugin-qt` | `1-alpha-20250213-1` | `15106be8…` | AppImage Qt 插件部署 |

（核实日期：2026-11-07。Homebrew 的 mpv 随上游滚动，未做版本 pin，
但 macOS job 会把安装到的 `pkg-config --modversion mpv` 记入构建日志与
`::notice`，便于追溯 ABI 漂移。）
