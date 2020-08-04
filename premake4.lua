newoption {
	trigger     = "metal",
	description = "Enable Metal backend (MacOs only)"
}

newoption {
	trigger = "vulkan",
	description = "Enable vulkan backend",
}

solution "Adl"
	configurations { "Debug", "Release" }
	configuration "Debug"
		defines { "_DEBUG" }
		flags { "Symbols" }
	configuration "Release"
		defines { "NDEBUG" }
		flags { "Optimize" }
	configuration {}
	
	language "C++"
	flags { "NoMinimalRebuild", "EnableSSE2" }

	local targetName;
	if os.is("macosx") then
		targetName = "osx"
		buildoptions{"-std=c++11 -Wno-c++11-narrowing"}
	end
	platforms {"x64"}

	if os.is("linux") then
		targetName = "linux"
		defines{ "__LINUX__" }
		-- Add -D_GLIBCXX_DEBUG for C++ STL debugging
		buildoptions{"-fopenmp -fPIC -Wno-deprecated-declarations"}
		buildoptions{"-std=c++11"}
		links{ "gomp" }
	end

	if os.is("windows") then
		targetName = "win"
		defines{ "WIN32" }
		buildoptions { "/MP"  }
		buildoptions { "/wd4305 /wd4244 /wd4661"  }
		defines {"_CRT_SECURE_NO_WARNINGS"}
		configuration {"Release"}
		buildoptions { "/openmp"  }
		configuration {}
	end
	
	platforms {"x64"}
	
	configuration {"x64", "Debug"}
		targetsuffix "64D"
		libdirs{ "lib/debug" }
	configuration {"x64", "Release"}
		targetsuffix "64"
		libdirs{ "lib/release" }
--		flags { "Optimize" } -- vs2010 fatal error C1001: An internal error has occurred in the compiler
	configuration {}
	includedirs { "./", "../" }

	if _OPTIONS["metal"] then
		if os.is("macosx") then
			print( ">> Metal backend enabled" )
			defines{"ADL_ENABLE_METAL"}
		end
	end

	if _OPTIONS["vulkan"] then
		local vulkanSdkPath = os.getenv( "VULKAN_SDK" )
		if not vulkanSdkPath then
			error( "'VULKAN_SDK' isn't defined." )
		end
		print( ">> Vulkan backend enabled. SDK path:"..vulkanSdkPath )
		defines{ "ADL_ENABLE_VULKAN" }
		includedirs{ vulkanSdkPath.."/include/" }
		if os.is( "windows" ) then
			libdirs{ vulkanSdkPath.."/Lib/" }
			links{ "vulkan-1" }
		elseif os.is( "linux" ) then
			libdirs{ vulkanSdkPath.."/lib/" }
			links{ "vulkan" }
		elseif os.is( "macosx" ) then
			error( "Fix me" )
		end
	end

	include "Adl"

	-- <PROJECT> Demo
	project "adlTest"
		kind "ConsoleApp"
		location "./build"
		links { "Adl" }
		if os.is("macosx") then
			linkoptions{ "-framework Cocoa", "-framework OpenGL", "-framework IOKit" }
			buildoptions{"-DGTEST_USE_OWN_TR1_TUPLE=1"}
		end
		if os.is("linux") then
			links {"GL", "X11", "Xi", "Xxf86vm", "Xrandr", "pthread", "dl"}
		end
		files { "./test/main.cpp", "./test/RaytraceTest.cpp", "./Adl/**.h" } 
		files { "./contrib/src/gtest-1.6.0/*.cc" } 

		includedirs { "./contrib/include/" }
		configuration {"x64", "Debug"}
			targetdir "./build/bin/debug"

		configuration {"x64", "Release"}
			targetdir "./build/bin/release"
