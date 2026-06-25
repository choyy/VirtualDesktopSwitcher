# xmake.lua

以下编译参数是为了减小二进制体积：

```xmake
    set_optimize("smallest")
    set_exceptions("no-cxx") -- 禁用 C++ 异常
    add_cxflags("/GR-") -- 禁用 C++ RTTI
```

# 激活顶层窗口

## 解决任务栏图标闪烁问题

`VirtualDesktopSwitcher::ActivateTopWindowOnMonitor(HMONITOR hMon)`

- `ActivateTopWindowOnMonitor` 内部无条件调用 `SendInput(MOUSEEVENTF_MOVE)` 重置输入来源，绕过 Alt 后前台锁限制
- **跨屏**：`MouseFocus` 钩子检测显示器切换 → `ActivateTopWindowOnMonitor(hMon)`
- **切桌面**：`OnDesktopSwitch` → `SwitchToDesktop` → `ActivateTopWindowOnMonitor(nullptr)`（自动获取光标位置）
- `AllowSetForegroundWindow(ASFW_ANY)` 内置在 `ActivateWindow` 中（`src/util/Utils.cpp`），调用方无需关心前台锁

| 问题 | 方案 |
|---|---|
| **任务栏图标闪烁** | `AllowSetForegroundWindow(ASFW_ANY)` 授予前台权限，`ActivateWindow` 不闪烁 |
| 即使有AllowSetForegroundWindow，但是如果**按过一次 Alt 后，再跨屏仍会偶发闪烁** | `SendInput(MOUSEEVENTF_MOVE)` 重置输入来源为鼠标，绕过 Alt 后的前台锁限制 |

## 窗口过滤（FindTopWndProc，Z 序枚举）

```
可见性        → IsWindowVisible        → 不可见跳过
无标题        → GetWindowTextW         → 跳过
工具窗口      → WS_EX_TOOLWINDOW      → 跳过
隐藏覆盖层    → DwmGetAttribute(CLOAK) → 跳过
最小化        → IsIconic               → 跳过
基于显示器    → MonitorFromRect        → 不匹配目标显示器跳过
```

过滤顺序按开销从小到大排列。不检查 `IsWindowOnCurrentDesktop`——该 COM API 不可靠，且 `ActivateWindow` 不会自动切换虚拟桌面，无副作用。

`IsWindowOnCurrentDesktop`——该 COM API 实现似乎有问题，待确认(#todo)。

# 快捷键

代码原来用 `LLKHF_ALTDOWN` 标志判断 Alt 是否被按下：

```cpp
const bool alt = (pKeyboard->flags & LLKHF_ALTDOWN) != 0;
```

但在 Windows 低层键盘钩子中，`LLKHF_ALTDOWN` **只在 `WM_SYSKEYDOWN` 消息上设置**。而 `Ctrl+Alt+数字键` 的组合，Windows 会把它当作**应用程序快捷键**发送 `WM_KEYDOWN`，**不会**设置 `LLKHF_ALTDOWN`。

所以按 `Ctrl+Alt+1` 时：
- `ctrl = true` ✅
- `alt = false` ❌（因为 `LLKHF_ALTDOWN` 没设置）
- `match = true && false = false`

这就是只有 `CtrlAlt` 模式不生效的原因——其他模式要么不涉及 Alt，要么是纯 `Alt`/`AltShift`（会触发 `WM_SYSKEYDOWN`，`LLKHF_ALTDOWN` 正常设置）。

- **修复**

把 Alt 检测改为同时检查物理按键状态：

```cpp
const bool alt = (pKeyboard->flags & LLKHF_ALTDOWN) != 0 || (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
```

保留 `LLKHF_ALTDOWN` 是为了兼容纯 `Alt` 组合的原有行为，新增的 `GetAsyncKeyState(VK_MENU)` 用来捕获 `Ctrl+Alt` 这种被 Windows 归类为 `WM_KEYDOWN` 的情况。