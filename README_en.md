<p align="center">
  <img src="res/icon.png" alt="icon" width="80">
</p>

<h1 align="center">Windows 10/11 Virtual Desktop Switcher & Indicator</h1>

<p align="center">
  <b>English</b> | <a href="README.md">中文</a>
</p>

> A lightweight virtual desktop quick-switching tool for Windows 10/11. Jump to any desktop instantly with `Alt + 1~9`, with a customizable on-screen desktop status indicator.

![Preview](assets/demo.gif)
![Preview](assets/demo2.gif)

## ✨ Features

- **Hotkey switching**: `Alt + 1` ~ `Alt + 9` to jump directly to the corresponding virtual desktop (keys and modifier keys are customizable)
- **Return to previous desktop**: ``Alt + ` `` to switch back to the previously active virtual desktop
- **Pin window to all desktops**: `Alt + D` to pin (or unpin) the current window to all virtual desktops, keeping it visible across desktop switches.
- **Desktop indicator**: Uses distinct symbols for different desktop states — **◉ current** desktop, **○ non-empty** desktops, and **◌ empty** desktops. Fully customizable position, size, and style.
- **Scroll to switch**: Scroll the mouse wheel while hovering over the indicator to quickly switch to the previous/next virtual desktop.
- **Drag to move**: Drag any window onto an indicator symbol to move it to that virtual desktop and automatically switch to it.
- **Cursor focus**: On multi-monitor setups, moving the mouse cursor between monitors automatically focuses the top window on the target monitor.
- **Extremely lightweight**: Executable ~140KB, memory footprint ~2MB, zero dependencies, runs silently in the background.

## 📦 Installation

- **GitHub Release**
  Download the latest [VirtualDesktopSwitcher.exe](https://api.github.com/repos/choyy/VirtualDesktopSwitcher/releases/latest) from the [Releases](https://github.com/choyy/VirtualDesktopSwitcher/releases) page, place it anywhere, and double-click to run.
- **Using `winget`**
  ```bash
  winget install Chooyy.VirtualDesktopSwitcher
  ```

To build from source, see [Building from Source](#building-from-source) below.

## 🚀 Usage

- Launch `VirtualDesktopSwitcher.exe`
- The desktop indicator appears on screen, and the program icon shows in the system tray
- Use `Alt + 1` ~ `Alt + 9` to switch between virtual desktops
- Use ``Alt + ` `` to return to the previous virtual desktop
- Use `Alt + D` to pin (or unpin) the current window to all desktops
- **Scroll** the mouse wheel while hovering over the indicator to switch desktops
- **Drag a window** onto an indicator symbol to move it to that desktop and switch to it automatically
- Right-click the tray icon to configure:
   - Adjust indicator **position**, **size**, **style**, **hotkeys**, and **display**
   - Toggle auto-start and run as administrator
- Double-click the tray icon to quickly **show/hide the indicator**

## 📝 INI Configuration

Configuration file is saved to `%LOCALAPPDATA%\VirtualDesktopSwitcher\config.ini`. You can customize the keys for switching and pinning virtual desktops in this file.

| Key | Description | Default Value |
|---|---|---|
| `DesktopKey1` ~ `DesktopKey9` | Virtual key codes for desktops 1~9 | `49`\~`57` (keys `1`\~`9`) |
| `PrevDesktopKey` | Virtual key code for returning to previous desktop | `192` (key `` ` ``) |
| `PinAllDesktopsKey` | Virtual key code for pinning/unpinning window to all desktops | `68` (key `D`) |

For virtual key codes, refer to [Virtual-Key Codes](https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes). For example, numpad 1\~9 correspond to `97`\~`105`.

To customize the keys for switching desktops, modify the configuration file according to the table above. For example, use `Alt + qweasdzxc` to switch to desktops `1~9`:

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
<summary><b>Taskbar icons flash when switching desktops (occasionally)</b></summary>

Disable taskbar flashing in Windows Settings:
**Settings → Personalization → Taskbar → Taskbar behaviors → Uncheck "Show flashing on taskbar apps"**

> ⚠️ Note: This also disables flashing alerts from all other applications.

</details>

<details>
<summary><b>Set individual wallpapers for each virtual desktop</b></summary>

In Task View, right-click on the target desktop thumbnail and select "Choose background" to set a separate wallpaper for that desktop.

</details>

<details>
<summary><b>Rename virtual desktops</b></summary>

In Task View, right-click on a desktop thumbnail and select "Rename" to give each virtual desktop a custom name.

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

<details>
<summary><b>Disable the snap layout popup at the top when dragging windows in Windows 11</b></summary>

**Settings → System → Multitasking → Snap windows**, turn it off to disable.

</details>

## 🤝 Acknowledgments

The desktop indicator component is ported from [vladelaina](https://github.com/vladelaina)'s open-source project [Catime](https://github.com/vladelaina/Catime). Thanks to the original author for the excellent work!
