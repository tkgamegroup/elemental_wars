cmake_minimum_required(VERSION 3.25.4)
project(elemental_wars)

set(flame_path "$ENV{FLAME_PATH}")
include("${flame_path}/utils.cmake")
include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_definitions(-W0 -std:c++latest)

set_output_dir("${CMAKE_SOURCE_DIR}/bin")

file(GLOB_RECURSE source_files "cpp/*.h*" "cpp/*.c*")
add_executable(elemental_wars ${source_files})

find_package(glm CONFIG REQUIRED)
target_link_libraries(elemental_wars glm::glm-header-only)

find_package(pugixml CONFIG REQUIRED)
target_link_libraries(elemental_wars pugixml::shared)

set(IMGUI_DIR "")
file(STRINGS "${flame_path}/build/CMakeCache.txt" flame_cmake_cache)
foreach(s ${flame_cmake_cache})
	if(IMGUI_DIR STREQUAL "")
		string(REGEX MATCH "IMGUI_DIR:PATH=(.*)" res "${s}")
		if(NOT res STREQUAL "")
			set(IMGUI_DIR ${CMAKE_MATCH_1})
		endif()
	endif()
endforeach()

assign_source_group("${CMAKE_CURRENT_SOURCE_DIR}" "" ${source_files})	
target_compile_options(elemental_wars PRIVATE -ZI)
target_link_options(elemental_wars PRIVATE /FIXED:NO)
target_compile_definitions(elemental_wars  PUBLIC USE_IMGUI)
target_compile_definitions(elemental_wars  PUBLIC USE_AUDIO_MODULE)
target_compile_definitions(elemental_wars  PUBLIC "IMPORT=__declspec(dllimport)")
target_compile_definitions(elemental_wars  PUBLIC "EXPORT=__declspec(dllexport)")
target_compile_definitions(elemental_wars  PUBLIC IMGUI_USER_CONFIG="${flame_path}/source/imgui_config.h")
target_include_directories(elemental_wars  PUBLIC "${IMGUI_DIR}")
target_include_directories(elemental_wars  PUBLIC "${flame_path}/include")
target_link_libraries(elemental_wars "${flame_path}/bin/debug/imgui.lib")
target_link_libraries(elemental_wars "${flame_path}/bin/debug/flame_foundation.lib")
target_link_libraries(elemental_wars "${flame_path}/bin/debug/flame_audio.lib")
target_link_libraries(elemental_wars "${flame_path}/bin/debug/flame_graphics.lib")
target_link_libraries(elemental_wars "${flame_path}/bin/debug/flame_universe.lib")

file(GENERATE OUTPUT "$<TARGET_FILE_DIR:elemental_wars>/elemental_wars.typedesc" CONTENT "${CMAKE_CURRENT_SOURCE_DIR}/code" TARGET elemental_wars)
add_custom_command(TARGET elemental_wars POST_BUILD COMMAND "${flame_path}/bin/debug/typeinfogen.exe" $<TARGET_FILE:elemental_wars>)

file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}" current_path)
add_custom_target(copy_binaries 
	COMMAND xcopy "${flame_path}\\bin\\debug\\*.dll"				"${current_path}\\bin\\debug"			/Y /D	
	COMMAND xcopy "${flame_path}\\bin\\debug\\*.typeinfo"			"${current_path}\\bin\\debug"			/Y /D
	COMMAND xcopy "${flame_path}\\bin\\relwithdebinfo\\*.dll"		"${current_path}\\bin\\relwithdebinfo"	/Y /D	
	COMMAND xcopy "${flame_path}\\bin\\relwithdebinfo\\*.typeinfo"	"${current_path}\\bin\\relwithdebinfo"	/Y /D)

add_dependencies(elemental_wars copy_binaries)
set_target_properties(elemental_wars PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
