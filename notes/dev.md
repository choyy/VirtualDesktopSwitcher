## 窗口激活问题

可靠的窗口激活：使用 AttachThreadInput + BringWindowToTop + SetForegroundWindow 替代单纯的 SetForegroundWindow。这是绕过 Windows 前台锁限制的标准技术。

## xmake.lua

    set_exceptions("no-cxx"): 禁用 C++ 异常处理，减少文件大小。