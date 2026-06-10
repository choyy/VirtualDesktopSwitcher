# xmake.lua

以下编译参数是为了减小二进制体积：

```xmake
    set_optimize("smallest")
    set_exceptions("no-cxx") -- 禁用 C++ 异常
    add_cxflags("/GR-") -- 禁用 C++ RTTI
```

# 激活顶层窗口

## 方案

跨屏聚焦和切桌面复用同一套机制：`PostWindowActivation` → `WM_FOCUS_ACTIVATE` → `SetTimer(150ms)` → `ActivateTopWindowOnMonitor`。

```
跨屏：MouseFocus 检测跨屏 → PostWindowActivation(hMon)
切桌面：OnDesktopSwitch → SwitchToDesktop → SyncDesktopState → PostWindowActivation()
                                                   ↓
                            WindowProc → WM_FOCUS_ACTIVATE
                              → m_pendingFocusMonitor = hMon
                              → SetTimer(kTimerFocusDelay, 150ms)
                                                   ↓
                            WindowProc → WM_TIMER
                              → ActivateTopWindowOnMonitor(m_pendingFocusMonitor)
                                → FindTopWindowOnMonitor(hMon)
                                → ActivateWindow × 3 重试 (90ms)
```

## 窗口过滤（FindTopWndProc，Z 序从高到低枚举）

```
可见性        → IsWindowVisible        → 不可见跳过
无标题        → GetWindowTextW         → 跳过
工具窗口      → WS_EX_TOOLWINDOW      → 跳过
隐藏覆盖层    → DwmGetAttribute(CLOAK) → 跳过
最小化        → IsIconic               → 跳过
基于显示器    → MonitorFromRect        → 不匹配目标显示器跳过
```

过滤顺序按开销从小到大排列。
`FindTopWndProc` 不再检查 `IsWindowOnCurrentDesktop`——该 COM API 在多显示器场景下不可靠，且 `ActivateWindow` 不会自动切换虚拟桌面，无副作用。

## 激活延迟

`ActivateWindow` 受前台锁限制，在用户无新输入时可能被拒绝，从而造成程序图标闪烁（无论激活成功或失败都有可能闪烁）。150ms 延迟让前台锁自然释放，配合 3 次 × 30ms 重试，覆盖绝大多数场景。

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