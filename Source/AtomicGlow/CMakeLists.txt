include_directories ( ${CMAKE_SOURCE_DIR}/Source/ThirdParty )

#FIX ME FOR EMBREE SUBMODULE

include_directories ( /usr/local/Cellar/tbb/2017_U5/include /Users/jenge/Downloads/embree-2.15.0.x86_64.macosx/include )
link_directories ( /usr/local/Cellar/tbb/2017_U5/lib /Users/jenge/Downloads/embree-2.15.0.x86_64.macosx/lib )

file (GLOB_RECURSE SOURCE_FILES *.cpp *.h)

add_executable(AtomicGlow ${SOURCE_FILES})

target_link_libraries(AtomicGlow ToolCore AtomicApp AtomicJS AtomicNETScript Poco embree tbb)
