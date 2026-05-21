set_version("1.2.1")
set_encodings("utf-8")
set_languages("c++20")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})

add_requires("stb")

target("VirtualDesktopSwitcher")
    set_arch("x86")
    set_kind("binary")
    set_optimize("smallest")
    set_exceptions("no-cxx")

    add_includedirs("src", "build")
    add_files("src/**.cpp", "res/*.manifest", "res/*.rc")
    add_configfiles("src/config.h.in")

    add_defines("NOMINMAX")
    add_cxflags("/GR-")
    add_ldflags("/SUBSYSTEM:WINDOWS")
    add_syslinks("shell32", "user32", "gdi32", "advapi32", "comctl32", "ole32", "dwmapi", "shcore")

    add_packages("stb")
