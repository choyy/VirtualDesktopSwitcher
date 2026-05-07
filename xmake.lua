set_version("1.1.3")
set_encodings("utf-8")
set_languages("c++20")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})

add_requires("stb")

target("VirtualDesktopSwitcher")
    set_kind("binary")
    set_optimize("smallest")
    set_exceptions("no-cxx")

    add_includedirs("src")
    add_files("src/**.cpp", "res/*.manifest", "res/*.rc")

    add_defines("NOMINMAX")
    add_ldflags("/SUBSYSTEM:WINDOWS")
    add_syslinks("shell32", "user32", "gdi32", "advapi32", "comctl32", "ole32")

    add_packages("stb")

    on_load(function (target)
        target:add("defines", string.format([[APP_VERSION="%s"]], target:version()))
    end)
