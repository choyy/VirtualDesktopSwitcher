# xmake.lua

以下编译参数是为了减小二进制体积：

```xmake
    set_optimize("smallest")
    set_exceptions("no-cxx") -- 禁用 C++ 异常
    add_cxflags("/GR-") -- 禁用 C++ RTTI
```

# 激活顶层窗口


## 解决任务栏图标闪烁问题

前台锁是 Windows 阻止后台进程调用 `SetForegroundWindow` 的机制，违禁调用会导致任务栏图标闪烁但不激活。

```cpp
AllowSetForegroundWindow(ASFW_ANY);  // 授予当前线程前台权限
ActivateWindow(hwnd);                // 不闪烁
```

此 API 无输入干扰，不涉及按键模拟，跨屏和切桌面统一行为。

## 窗口过滤（FindTopWndProc，Z 序从高到低枚举）

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