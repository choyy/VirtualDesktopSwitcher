<p align="center">
  <img src="res/icon.png" alt="icon" width="80">
</p>

<h1 align="center">Windows 10/11 虚拟桌面切换和指示工具</h1>

<p align="center">
  <a href="README_en.md">English</a> | <b>中文</b>
</p>

> 适用于 Windows 10/11 的虚拟桌面快速切换工具，通过 `Alt + 1~9` 直达指定桌面，并配有可自定义的桌面状态指示器。

![预览](assets/demo.gif)
![预览](assets/demo2.gif)

## ✨ 功能特性

- **快捷键切换**：`Alt + 1` ~ `Alt + 9` 快速跳转到对应虚拟桌面（按键和修饰键均可自定义）
- **返回上一桌面**：``Alt + ` `` 切换到上一个所在的虚拟桌面
- **窗口固定到所有桌面**：`Alt + D` 将当前窗口固定到所有虚拟桌面（或取消固定），切换桌面时窗口保持可见。
- **桌面指示器**：使用不同的符号表示不同的虚拟桌面，可表示 **◉ 当前虚拟桌面**、**○ 非空桌面**和 **◌ 空桌面**，支持自定义位置、大小和样式
- **滚轮切换**：鼠标悬停在指示器上滚动滚轮可快速切换上一个/下一个虚拟桌面
- **拖动窗口到指示器**：将任意窗口拖到指示器符号上即可将该窗口移动到对应虚拟桌面，并自动切换到该虚拟桌面。
- **极其轻量**：可执行文件 ~140KB，内存占用 ~2MB，零依赖、静默运行。

## 📦 安装

- Github Release
  从 [Releases](https://github.com/choyy/VirtualDesktopSwitcher/releases) 页面下载最新的 [VirtualDesktopSwitcher.exe](https://api.github.com/repos/choyy/VirtualDesktopSwitcher/releases/latest)，放在任何位置，双击运行即可。
- 使用`winget`安装
  ```bash
  winget install Chooyy.VirtualDesktopSwitcher
  ```

如需从源码编译，请参考下方 [编译](#-从源码编译) 部分。

## 🚀 使用方法

- 启动 `VirtualDesktopSwitcher.exe`
- 屏幕显示桌面状态指示器，系统托盘出现程序图标
- 使用 `Alt + 1` ~ `Alt + 9` 切换虚拟桌面
- 使用 ``Alt + ` `` 返回上一个虚拟桌面
- 使用 `Alt + D` 将当前窗口固定到所有桌面（或取消固定）
- 鼠标悬停在指示器上**滚动滚轮**切换上一个/下一个虚拟桌面
- **拖拽窗口**到指示器符号上，将窗口移动到对应桌面并自动切换过去
- 右键托盘图标进行设置：
   - 调整指示器**位置**、**大小**、**样式**、**快捷键**、**显示**
   - 管理开机自启、管理员启动
8. 双击托盘图标快速**切换指示器显示/隐藏**

## 📝 INI 配置

配置文件保存至 `%LOCALAPPDATA%\VirtualDesktopSwitcher\config.ini`，在配置文件中可自定义切换、固定虚拟桌面的键为其他按键。

| 键 | 说明 | 默认值 |
|---|---|---|
| `DesktopKey1` ~ `DesktopKey9` | 1~9 号桌面按键的虚拟键码 | `49`\~`57`（数字`1`\~`9`） |
| `PrevDesktopKey` | 返回上一桌面的按键虚拟键码 | `192`（按键`` ` ``） |
| `PinAllDesktopsKey` | 固定/取消固定窗口到所有桌面的按键虚拟键码 | `68`（按键`D`） |

虚拟键码可参考 [Virtual-Key Codes](https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes)。例如数字键盘 1\~9 对应 `97`\~`105`。

如果需要自定义切换虚拟桌面的键，可按照上述表格说明修改配置文件，例如使用 `Alt + qweasdzxc` 切换桌面 `1~9`：

```ini
[General]
DesktopKey1=81    ; Q
DesktopKey2=87    ; W
DesktopKey3=69    ; E
DesktopKey4=65    ; A
DesktopKey5=83    ; S
DesktopKey6=68    ; D
DesktopKey7=90    ; Z
DesktopKey8=88    ; X
DesktopKey9=67    ; C
```

## 🔧 从源码编译

### 环境要求

- MSVC C++ 工具链（需支持C++20）
- [xmake](https://xmake.io/) 构建工具

### 编译步骤

```bash
# 直接编译
xmake

# 生成 Visual Studio 工程文件（可选）
xmake project -k vsxmake
```

编译产物位于 `build` 目录。

## ⚙️ 以管理员身份运行与开机自启

部分应用（如任务管理器）的键盘快捷键在普通权限下无效，需要以管理员身份运行程序。

- **以管理员启动**：右键托盘图标 → `以管理员启动`。该设置持久化保存，下次启动时自动提权（弹 UAC 确认框）。
- **开机自启（普通权限）**：写入注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`。
- **开机自启（管理员权限）**：以管理员身份勾选开机自启时，自动使用**任务计划程序**创建登录时启动任务（`/rl highest`），启动过程无 UAC 弹窗。
- **启动项清理**：托盘图标右键点击退出时会自动删除注册表键和计划任务，此时可直接删除程序不留残留。

## 💡 常见问题

<details>
<summary><b>切换虚拟桌面时任务栏图标闪烁（偶发）</b></summary>

可在 Windows 设置中禁用任务栏闪烁：
**设置 → 个性化 → 任务栏 → 任务栏行为 → 取消勾选“显示任务栏应用上的闪烁”**

> ⚠️ 注意：此操作会同时禁止所有应用的闪烁提醒（如消息通知）。

</details>

<details>
<summary><b>让任务栏显示所有虚拟桌面的窗口图标</b></summary>

**设置 → 系统 → 多任务处理 → 桌面 → 在任务栏上，显示所有打开的窗口**
将下拉选项改为 **“在所有桌面上”**，即可在每个虚拟桌面的任务栏中看到其他桌面的窗口。

</details>

<details>
<summary><b>Windows中有关虚拟桌面的快捷键</b></summary>

- 打开虚拟桌面视图：`Win + Tab`
- 创建一个新的虚拟桌面：`Win + Ctrl + D`
- 切换上一个/下一个虚拟桌面：`Win + Ctrl + ← / →`
- 关闭当前虚拟桌面：`Win + Ctrl + F4`

</details>

<details>
<summary><b>为每个虚拟桌面单独设置壁纸</b></summary>

在任务视图中右键点击目标桌面缩略图，选择 “选择背景”，即可为该桌面单独设置壁纸。

</details>

<details>
<summary><b>重命名虚拟桌面</b></summary>

在任务视图中右键点击桌面缩略图，选择 “重命名” 选项，可对每个虚拟桌面进行重命名。

</details>

<details>
<summary><b>关闭Win11拖动窗口时顶部的贴靠窗口自动布局</b></summary>

**设置 → 系统 → 多任务处理 → 贴靠窗口**，将其关闭即可。

</details>

## 🤝 致谢

本项目的桌面指示器部分移植自 [vladelaina](https://github.com/vladelaina) 的开源项目 [Catime](https://github.com/vladelaina/Catime)，感谢原作者的优秀作品!
