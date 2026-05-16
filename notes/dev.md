# xmake.lua

以下编译参数是为了减小二进制体积：

```xmake
    set_optimize("smallest")
    set_exceptions("no-cxx") -- 禁用 C++ 异常
    add_cxflags("/GR-") -- 禁用 C++ RTTI
```

# 激活顶层窗口

## 方案

每次 `SwitchToDesktop` 时实时查找目标桌面 Z 序最顶层窗口，不再记录/恢复历史窗口。

## 窗口过滤（FindTopWndProc，Z 序从高到低枚举）

```
可见性        → IsWindowVisible          → 不可见跳过
工具窗口      → WS_EX_TOOLWINDOW        → 跳过
无标题        → GetWindowTextW           → 跳过
隐藏覆盖层    → DwmGetAttribute(CLOAKED) → 跳过（需 dwmapi.h + dwmapi.lib）
跨桌面固定    → IsWindowOnCurrentDesktop → 不在当前桌面跳过
```

过滤顺序按开销从小到大排列。CLOAKED 用于排除 Alt+Tab 中隐藏的系统覆盖层（如 `Windows 输入体验`），这些窗口 Z 序高于用户窗口且 `IsWindowOnCurrentDesktop` 返回 true。

## 注意

- `EnumWindows` 返回天然 Z 序，第一个匹配就是最顶层，无需排序
- 空桌面激活 `Program Manager` 是空操作，无害

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