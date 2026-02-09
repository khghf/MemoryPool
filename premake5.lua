workspace "MemoryPool"
	architecture"x64"
	configurations{
		"Debug",
		"Release",
	}
	language"C++"
	cppdialect "C++17"  
project"MemoryPool"
	kind"ConsoleApp"
	files{
		"./MemoryPool/include/**.h",
		"./MemoryPool/Src/**.cpp",
		"./MemoryPool/main.cpp",
	}
	includedirs{
		"./MemoryPool"
	}

	