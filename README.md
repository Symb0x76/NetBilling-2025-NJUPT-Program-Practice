# NJUPT 程序设计 上网计费

> 个人自用，请勿用于商业用途！

南京邮电大学程序设计实践周作业，该项目为上网计费系统的源代码。

## 设计思路

TODO

## 使用方法

1. 安装 Qt，只需要最基本组件库即可
2. 配置 CMake, Ninja, C++ 环境
3. 下载 [ElaWidgetTools](https://github.com/Liniyous/ElaWidgetTools)

```bash
git submodule add https://github.com/Liniyous/ElaWidgetTools
```

> 如果不能通过编译请使用我修改过的版本
>
> ```bash
> git submodule add https://github.com/Symb0x76/ElaWidgetTools
> ```

4. 编译项目

```pwsh
cmake --build ./build
```

5. 打包可运行文件
   将 `build/ElaWidgetTools/ElaWidgetTools/ElaWidgetTools.dll`, `Qt/<Qt_version>/mingw_64/bin/Qt6Widgets.dll`, `Qt/<Qt_version>/mingw_64/bin/Qt6Core.dll`, `Qt/<Qt_version>/mingw_64/bin/Qt6Gui.dll`, `Qt/<Qt_version>/mingw_64/bin/libgcc_s_seh-1.dll` 与 `/build/NetBilling.exe` 放在同一目录下即可正常运行

## 外部依赖

-   [Qt](https://www.qt.io/): 框架
-   [ElaWidgetTools](https://github.com/Liniyous/ElaWidgetTools): UI
