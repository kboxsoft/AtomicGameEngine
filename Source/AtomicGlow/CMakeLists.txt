include_directories ( ${CMAKE_SOURCE_DIR}/Source/ThirdParty )

#FIX ME FOR EMBREE SUBMODULE

include_directories ( /usr/local/Cellar/tbb/2017_U5/include /usr/local/Cellar/embree/2.7.0.x86_64.macosx/include )
link_directories ( /usr/local/Cellar/tbb/2017_U5/lib /usr/local/Cellar/embree/2.7.0.x86_64.macosx/lib )

file (GLOB_RECURSE SOURCE_FILES *.cpp *.h)

add_executable(AtomicGlow ${SOURCE_FILES})

target_link_libraries(AtomicGlow ToolCore AtomicApp AtomicJS AtomicNETScript Poco embree tbb)
