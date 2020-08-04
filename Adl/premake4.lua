newoption {
	trigger     = "profiling",
	description = "enable device opencl profiling"
}

	if _OPTIONS["profiling"] then
		print( ">> OpenCL profiling enabled" )
		defines {"PROF_BUILD"}
	end

	if os.is("macosx") then
		if _OPTIONS["metal"] then
			linkoptions{ "-framework Metal" }
			linkoptions{ "-framework QuartzCore -framework IOSurface" }
			defines{"ADL_ENABLE_METAL"}
		end
	end


 	project "Adl"
		kind "StaticLib"
		location "../build"
		dofile ("../tools/premake4/OpenCLSearch.lua" )
		defines {"CL_USE_DEPRECATED_OPENCL_1_1_APIS" }
		files { "../Adl/**.h", "../Adl/**.cpp", "../Adl/**.inl" }
		if os.is("macosx") then
			if _OPTIONS["metal"] then
				files {  "../Adl/**.mm" }
			end
		end

		if _OPTIONS["vulkan"] then
			files { "../Adl/Vulkan/**.h", "../Adl/Vulkan/**.cpp" }
		else
			excludes { "../Adl/Vulkan/**.h", "../Adl/Vulkan/**.cpp" }			
		end
