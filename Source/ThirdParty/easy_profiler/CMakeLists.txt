#
# Copyright (c) 2016 Rokas Kupstys.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
set (TARGET_NAME easy_profiler)
define_source_files (GLOB_CPP_PATTERNS easy_profiler_core/*.cpp GLOB_H_PATTERNS easy_profiler_core/*.h easy_profiler_core/include/*.h)
set (INCLUDE_DIRS easy_profiler_core/include)
setup_library ()

# Get version
file (STRINGS easy_profiler_core/version.info EASY_PRODUCT_VERSION_STRING)
string(REPLACE "." ";" VERSION_LIST ${EASY_PRODUCT_VERSION_STRING})
list(GET VERSION_LIST 0 EASY_PROGRAM_VERSION_MAJOR)
list(GET VERSION_LIST 1 EASY_PROGRAM_VERSION_MINOR)
list(GET VERSION_LIST 2 EASY_PROGRAM_VERSION_PATCH)
target_compile_definitions(easy_profiler PUBLIC -DBUILD_WITH_EASY_PROFILER=1 -D_BUILD_PROFILER
    -DEASY_PROFILER_VERSION_MAJOR=${EASY_PROGRAM_VERSION_MAJOR}
    -DEASY_PROFILER_VERSION_MINOR=${EASY_PROGRAM_VERSION_MINOR}
    -DEASY_PROFILER_VERSION_PATCH=${EASY_PROGRAM_VERSION_PATCH}
)

set(EASY_DEFAULT_PORT 28077 CACHE STRING "Default listening port")
set(EASY_OPTION_LISTEN OFF CACHE BOOL "Enable automatic startListen on startup")
set(EASY_OPTION_PROFILE_SELF OFF CACHE BOOL "Enable self profiling (measure time for internal storage expand)")
set(EASY_OPTION_PROFILE_SELF_BLOCKS_ON OFF CACHE BOOL "Storage expand default status (profiler::ON or profiler::OFF)")
set(EASY_OPTION_LOG OFF CACHE BOOL "Print errors to stderr")
set(EASY_OPTION_PREDEFINED_COLORS ON CACHE BOOL "Use predefined set of colors (see profiler_colors.h). If you want to use your own colors palette you can turn this option OFF.")

if (WIN32)
    set(EASY_OPTION_EVENT_TRACING ON CACHE BOOL "Enable event tracing by default")
    set(EASY_OPTION_LOW_PRIORITY_EVENT_TRACING ON CACHE BOOL "Set low priority for event tracing thread")
    if (EASY_OPTION_EVENT_TRACING)
        target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_EVENT_TRACING_ENABLED=true)
    else ()
        target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_EVENT_TRACING_ENABLED=false)
    endif ()
    if (EASY_OPTION_LOW_PRIORITY_EVENT_TRACING)
        target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_LOW_PRIORITY_EVENT_TRACING=true)
    else ()
        target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_LOW_PRIORITY_EVENT_TRACING=false)
    endif ()
    if (MSVC)
        target_compile_definitions(easy_profiler PUBLIC -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
    endif ()
elseif (NOT ANDROID)
    target_link_libraries(easy_profiler pthread)
endif ()
target_compile_definitions(easy_profiler PUBLIC -DEASY_DEFAULT_PORT=${EASY_DEFAULT_PORT})
if (EASY_OPTION_LISTEN)
    target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_START_LISTEN_ON_STARTUP=1)
else ()
    target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_START_LISTEN_ON_STARTUP=0)
endif ()
#
if (EASY_OPTION_PROFILE_SELF)
    target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_MEASURE_STORAGE_EXPAND=1)
    if (EASY_OPTION_PROFILE_SELF_BLOCKS_ON)
        target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_STORAGE_EXPAND_BLOCKS_ON=true)
    else ()
        target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_STORAGE_EXPAND_BLOCKS_ON=false)
    endif ()
else ()
    target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_MEASURE_STORAGE_EXPAND=0)
endif ()

if (EASY_OPTION_LOG)
    target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_LOG_ENABLED=1)
else()
    target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_LOG_ENABLED=0)
endif ()

if (EASY_OPTION_PREDEFINED_COLORS)
    target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_BUILTIN_COLORS=1)
else ()
    target_compile_definitions(easy_profiler PUBLIC -DEASY_OPTION_BUILTIN_COLORS=0)
endif ()

if (NOT ANDROID)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_INCLUDE_CURRENT_DIR ON)

    find_package(Qt5Widgets)
    if (Qt5Widgets_FOUND)
        if (MSVC)
            # /!\ THIS IS A HACK /!\
            # Official Qt builds on windows link to dynamic vc runtime. Engine links to static vc runtime. Since Profiler
            # tool must use same (dynamic) runtime as Qt libraries and it also must link to easy_profiler library we build
            # a second copy of easy_profiler targetting dynamic runtime and have Profiler link to that.
            set(CompilerFlags CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE)
            foreach(CompilerFlag ${CompilerFlags})
                string(REPLACE "/MT" "/MD" ${CompilerFlag} "${${CompilerFlag}}")
            endforeach()
            set (TARGET_NAME easy_profiler_md)
            define_source_files (GLOB_CPP_PATTERNS easy_profiler_core/*.cpp GLOB_H_PATTERNS easy_profiler_core/*.h easy_profiler_core/include/*.h)
            set (INCLUDE_DIRS easy_profiler_core/include)
            setup_library ()
            get_property(EASY_PROFILER_COMPILE_DEFINITIONS TARGET easy_profiler PROPERTY COMPILE_DEFINITIONS)
            target_compile_definitions(easy_profiler_md PUBLIC ${EASY_PROFILER_COMPILE_DEFINITIONS})
            set (easy_profiler easy_profiler_md)
            set (APPLICATION_PLATFORM WIN32)
            # /!\ HACK IS NO MORE /!\
        else ()
            set (easy_profiler easy_profiler)
        endif ()
        set (TARGET_NAME Profiler)
        define_source_files (GLOB_CPP_PATTERNS profiler_gui/*.cpp GLOB_H_PATTERNS profiler_gui/*.h profiler_gui/*.qrc profiler_gui/*.rc)
        setup_executable(TOOL ${APPLICATION_PLATFORM})
        if (CMAKE_VERSION VERSION_GREATER "3.0")
            set_target_properties(Profiler PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
        endif ()
        target_link_libraries(Profiler Qt5::Widgets ${easy_profiler})
        if (WIN32)
            target_link_libraries(Profiler ws2_32 psapi)
        endif ()
    else ()
        message(STATUS "INFO\n\n\tQt5 not found! Generating EasyProfiler projects without GUI.\n")
    endif (Qt5Widgets_FOUND)
endif (NOT ANDROID)
