# Windows 11 虚拟桌面切换工具

这是一个用C++开发的Windows 11虚拟桌面切换工具，可以通过快捷键快速切换虚拟桌面。

## 功能特性

- 使用 `Alt + 1` 切换到桌面1
- 使用 `Alt + 2` 切换到桌面2
- 使用 `Alt + 3` 切换到桌面3
- ...以此类推，支持 Alt + 1 到 Alt + 9
- 按 `ESC` 键退出程序

## 编译方法

### 使用xmake编译

1. 确保已安装xmake构建工具
2. 在项目目录下执行以下命令：

```bash
# 编译
xmake
```

3. 编译后的可执行文件位于 `build/windows/x64/debug` 或 `build/windows/x64/release` 目录下

### 使用Visual Studio编译

1. 使用xmake生成Visual Studio项目文件：

```bash
xmake project -k vsxmake
```

2. 使用Visual Studio打开生成的 `.sln` 文件进行编译

## 使用方法

1. 运行编译后的 `VirtualDesktopSwitcher.exe`
2. 程序会在控制台显示当前虚拟桌面数量和当前桌面编号
3. 使用 `Alt + 数字键` 切换到对应的虚拟桌面
4. 按 `ESC` 键退出程序

## 技术实现

- 使用Windows COM接口与虚拟桌面管理器交互
- 通过全局键盘钩子捕获快捷键
- 使用Windows 11的虚拟桌面API实现桌面切换

## 系统要求

- Windows 11
- 支持虚拟桌面的系统配置

## 注意事项

- 确保系统已启用虚拟桌面功能
