add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})
set_encodings("utf-8")
set_languages("c++17")

target("VirtualDesktopSwitcher")
    set_kind("binary")
    add_files("src/*.cpp")
    