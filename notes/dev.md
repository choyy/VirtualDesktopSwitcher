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
