include_directories ( ${CMAKE_SOURCE_DIR}/Source/ThirdParty )
include_directories ( ${CMAKE_SOURCE_DIR}/Source/ThirdParty/embree/include )

add_definitions(-DEMBREE_STATIC_LIB=1)

if (WIN32)
    add_definitions(-DATOMIC_WIN32_CONSOLE)
endif(WIN32)

file (GLOB_RECURSE SOURCE_FILES *.cpp *.h)

add_executable(AtomicGlow ${SOURCE_FILES})

target_link_libraries(AtomicGlow ToolCore AtomicApp AtomicJS AtomicNETScript Poco embree)

if (WIN32)
    target_link_libraries(AtomicGlow Iphlpapi Wldap32)
endif()

