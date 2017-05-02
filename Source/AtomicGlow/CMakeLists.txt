include_directories ( ${CMAKE_SOURCE_DIR}/Source/ThirdParty )
include_directories ( ${CMAKE_SOURCE_DIR}/Source/ThirdParty/embree/include )


file (GLOB_RECURSE SOURCE_FILES *.cpp *.h)

add_executable(AtomicGlow ${SOURCE_FILES})

target_link_libraries(AtomicGlow ToolCore AtomicApp AtomicJS AtomicNETScript Poco embree)
