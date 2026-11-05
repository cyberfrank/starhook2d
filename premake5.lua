-- premake5.lua

workspace "workspace"
    configurations { "Debug", "Release" }
    language "C"
    flags {
        "FatalWarnings", 
        "MultiProcessorCompile"
    }
    warnings "Extra"
    inlining "Auto"
    sysincludedirs { "" }
    editAndContinue "Off"
    includedirs "code"
    targetdir "bin"
    location "bin"
    characterset "MBCS" 

filter "system:windows"
    platforms { "Win64" }
    systemversion("latest")

filter "platforms:Win64"
    defines { 
        "OS_WINDOWS", 
        "_CRT_SECURE_NO_WARNINGS"
    }
    staticruntime "On"
    architecture "x64"
    disablewarnings {
        "4057", -- Slightly different base types.
        "4100", -- Unused formal parameter.
        "4152", -- Conversion from function pointer to void *.
        "4200", -- Zero-sized array. Valid C99.
        "4201", -- Nameless struct/union. Valid C11.
        "4204", -- Non-constant aggregate initializer. Valid C99.
        "4206", -- Translation unit is empty. Might be #ifdefed out.
        "4214", -- Bool bit-fields. Valid C99.
        "4221", -- Pointers to locals in initializers. Valid C99.
        "4702", -- Unreachable code.
        "4706", -- Assignment within conditional. Valid C99.
    }
    linkoptions { "/ignore:4099" }

filter "configurations:Debug"
    defines { "DEBUG_MODE", "DEBUG" }
    symbols "On"

filter "configurations:Release"
    defines { "RELEASE_MODE", "RELEASE" }
    optimize "On"

project "glfw3"
    kind "StaticLib"
    warnings "Default"
    includedirs "libs/glfw/include"
    targetname "glfw3"
    targetdir "bin/%{cfg.buildcfg}"
    files {
        "libs/glfw/src/glfw_config.h",
		"libs/glfw/src/context.c",
		"libs/glfw/src/init.c",
		"libs/glfw/src/input.c",
		"libs/glfw/src/monitor.c",
		"libs/glfw/src/vulkan.c",
		"libs/glfw/src/window.c"
    }
    filter "system:windows"
        files {
            "libs/glfw/src/win32_init.c",
			"libs/glfw/src/win32_joystick.c",
			"libs/glfw/src/win32_monitor.c",
			"libs/glfw/src/win32_time.c",
			"libs/glfw/src/win32_thread.c",
			"libs/glfw/src/win32_window.c",
			"libs/glfw/src/wgl_context.c",
			"libs/glfw/src/egl_context.c",
			"libs/glfw/src/osmesa_context.c"
        }
        defines {
            "_GLFW_WIN32",
        }
    filter "system:linux"
        files {
            "libs/glfw/src/x11_init.c",
			"libs/glfw/src/x11_monitor.c",
			"libs/glfw/src/x11_window.c",
			"libs/glfw/src/xkb_unicode.c",
			"libs/glfw/src/posix_time.c",
			"libs/glfw/src/posix_thread.c",
			"libs/glfw/src/glx_context.c",
			"libs/glfw/src/egl_context.c",
			"libs/glfw/src/osmesa_context.c",
			"libs/glfw/src/linux_joystick.c"
        }
        defines {
            "_GLFW_X11"
        }

project "lua"
    kind "StaticLib"
    warnings "Default"
    includedirs "libs/lua/include"
    targetname "lua"
    targetdir "bin/%{cfg.buildcfg}"
    defines { "MAKE_LIB" }
    files { "libs/lua/src/onelua.c" }

project "foundation"
    kind "StaticLib"
    targetname "foundation"
    files { "code/foundation/**.h", "code/foundation/**.c" }

project "engine"
    kind "ConsoleApp"
    targetname "sh2d"
    files { "code/**.h", "code/**.c" }
    excludes { "code/foundation/**.h", "code/foundation/**.c" } 
    includedirs { "libs/glfw/include", "libs/lua/include" }
    dependson { "foundation" }
    links { "glfw3", "lua", "foundation" }

project "hash"
    kind "ConsoleApp"
    targetname "hash"
    files { "tools/hash.c" }
    dependson { "foundation" }
    links { "foundation" }