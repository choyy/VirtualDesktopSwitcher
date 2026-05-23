<p align="center">
  <img src="res/icon.png" alt="icon" width="80">
</p>

<h1 align="center">Windows 10/11 Virtual Desktop Switcher & Indicator</h1>

<p align="center">
  <b>English</b> | <a href="README.md">中文</a>
</p>

> A lightweight virtual desktop quick-switching tool for Windows 10/11. Jump to any desktop instantly with `Alt + 1~9`, with a customizable on-screen desktop status indicator.

![Preview](assets/demo.gif)

## ✨ Features

- **Hotkey switching**: `Alt + 1` ~ `Alt + 9` to jump directly to the corresponding virtual desktop
- **Desktop indicator**: Uses distinct symbols for different desktop states — **◉ current** desktop, **○ non-empty** desktops, and **◌ empty** desktops. Fully customizable position, size, and style.
- **Extremely lightweight**: Executable ~130KB, memory footprint ~2MB, zero dependencies, runs silently in the background.

## 📦 Installation

Download the latest [VirtualDesktopSwitcher.exe](https://api.github.com/repos/choyy/VirtualDesktopSwitcher/releases/latest) from the [Releases](https://github.com/choyy/VirtualDesktopSwitcher/releases) page, place it anywhere, and double-click to run.

To build from source, see [Building from Source](#building-from-source) below.

## 🚀 Usage

1. Launch `VirtualDesktopSwitcher.exe`
2. The desktop indicator appears on screen, and the program icon shows in the system tray
3. Use `Alt + 1` ~ `Alt + 9` to switch between virtual desktops
4. Use the mouse scroll wheel on the desktop indicator to switch to the previous/next virtual desktop
5. Right-click the tray icon to configure:
   - Adjust indicator **position**, **size**, **style**, **hotkeys**, and **display**
   - Toggle auto-start and run as administrator
6. Double-click the tray icon to quickly **show/hide the indicator**

All settings and log files are saved to `%LOCALAPPDATA%\VirtualDesktopSwitcher`.

## 🔧 Building from Source

### Prerequisites

- MSVC C++ toolchain (C++20 support required)
- [xmake](https://xmake.io/) build system

### Build Steps

```bash
# Build directly
xmake

# Generate Visual Studio project files (optional)
xmake project -k vsxmake
```

Build output is located in the `build` directory.

## ⚙️ Run as Administrator & Auto-start

Some applications (e.g., Task Manager) ignore keyboard shortcuts when the switcher runs with normal privileges. Running as administrator resolves this.

- **Run as admin**: Right-click tray icon → `Run as admin`. This setting is persisted — the app will auto-elevate on next launch (UAC prompt will appear).
- **Auto-start (normal privileges)**: Writes to registry `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.
- **Auto-start (admin privileges)**: When auto-start is enabled under admin mode, the app automatically creates a **Task Scheduler** login task (`/rl highest`), so startup has no UAC prompts.
- **Clean uninstall**: Tray icon right-click → `Exit` automatically removes both registry keys and scheduled tasks. The program can then be safely deleted without leaving traces.

## 💡 FAQ

<details>
<summary><b>Taskbar icons flash when switching desktops</b></summary>

Disable taskbar flashing in Windows Settings:
**Settings → Personalization → Taskbar → Taskbar behaviors → Uncheck "Show flashing on taskbar apps"**

> ⚠️ Note: This also disables flashing alerts from all other applications.

</details>

<details>
<summary><b>Show window icons from all desktops on the taskbar</b></summary>

**Settings → System → Multitasking → Desktops → On the taskbar, show all open windows**
Change the dropdown to **"On all desktops"** to see window icons from other virtual desktops on the taskbar.

</details>

<details>
<summary><b>Windows virtual desktop keyboard shortcuts</b></summary>

- Open virtual desktop view: `Win + Tab`
- Create a new virtual desktop: `Win + Ctrl + D`
- Switch to previous/next virtual desktop: `Win + Ctrl + ← / →`
- Close current virtual desktop: `Win + Ctrl + F4`

</details>

## 🤝 Acknowledgments

The desktop indicator component is ported from [vladelaina](https://github.com/vladelaina)'s open-source project [Catime](https://github.com/vladelaina/Catime). Thanks to the original author for the excellent work!
