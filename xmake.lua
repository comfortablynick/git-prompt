-- luacheck: ignore 1
-- enable debug symbols
set_version("0.0.1")
add_configfiles("$(builddir)/config.h.in", {prefix = "GP_CONFIG"})
-- set_config_header("$(buildir)/config.h", {prefix = "GP_CONFIG"})

if is_mode("debug") then
  set_symbols("debug")
end

target("git-prompt")
    set_kind("binary")
    add_files("src/*.c")
    set_languages("gnu99")
    set_warnings("all", "extra")
    add_defines("LOG_USE_COLOR", "GIT_HASH_LEN=7", "FMT_STRING=\"%b@%c\"")
    set_installdir("$(env HOME)/.local")
