CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

PROJECT(ODP_SYSCALLS C)


IF(k1_tools)
  MESSAGE(STATUS "K1 Toolchain path set to \"${k1_tools}\"")
ELSE()
  MESSAGE(FATAL_ERROR "K1 Toolchain path not set, please used -Dk1_tools=... in your cmake command line")
ENDIF()

if(CMAKE_CROSSCOMPILING)
	add_subdirectory(k1_program)
else()
	INCLUDE_DIRECTORIES(include)
	INCLUDE_DIRECTORIES(${k1_tools}/include/syscalls)
	INCLUDE_DIRECTORIES(${k1_tools}/include/)

	FILE(GLOB c_files sources/*.c )

	ADD_LIBRARY(odp_syscall SHARED ${c_files})
	SET_TARGET_PROPERTIES(odp_syscall PROPERTIES COMPILE_FLAGS "-Wall -fexceptions")
endif()
