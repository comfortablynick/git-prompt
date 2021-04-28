-- luacheck: ignore 1
-- enable debug symbols
if is_mode("debug") then
  set_symbols("debug")
end

target("ctest")
    set_kind("binary")
    add_files("src/*.c")
    set_languages("gnu99")
    set_warnings("all", "extra")
    add_defines("LOG_USE_COLOR", "GIT_HASH_LEN=7")
