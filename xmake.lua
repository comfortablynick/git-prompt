-- luacheck: ignore 1
target("ctest")
    set_kind("binary")
    add_files("src/*.c")
    -- set_languages("c99")
    set_warnings("all", "extra")
    -- on_load(function (target)
    --     target:add(find_packages("sqlite3"))
    -- end)
