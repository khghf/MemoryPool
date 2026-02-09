workspace "Delegate"
	architecture"x64"
	configurations{
		"Debug",
		"Release",
	}
	language"C++"
	cppdialect "C++20"  
project"Delegate"
	kind"ConsoleApp"
	files{
		"./Delegate/include/**.h",
		"./include/main.cpp",
	}
	includedirs{
		"./"
	}

	