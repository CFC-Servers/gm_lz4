PROJECT_GENERATOR_VERSION = 2

newoption({
    trigger = "gmcommon",
    description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
    value = "../garrysmod_common"
})

local gmcommon = assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
    "you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")
include(gmcommon)

CreateWorkspace({name = "lz4", abi_compatible = true})
    CreateProject({serverside = true})
        includedirs({"./lz4/lib"})
        links({"lz4"})
        IncludeLuaShared()
        files({
            "source/*.cpp",
            "source/*.h"
        })
