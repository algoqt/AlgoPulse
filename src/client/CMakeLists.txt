cmake_minimum_required(VERSION 3.16)

project(client)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

message("in client project :${PROJECT_SOURCE_DIR}")

link_directories( "${LinkDirs}" )
include_directories( "${IncludeDirs}" )
include_directories( "${PROJECT_SOURCE_DIR}/../algo_protocal/")
include_directories( "${PROJECT_SOURCE_DIR}/../common/")

file(GLOB ClientLibSourceFiles ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB protocalFiles  ${PROJECT_SOURCE_DIR}/../algo_protocal/*.cc)

foreach(profile ${protocalFiles})
    message(${profile})
endforeach()

if(WIN32)
    set(DEPENDENT_LIBS ws2_32 wsock32)

elseif(UNIX)
    set(DEPENDENT_LIBS pthread )
endif()

list(APPEND DEPENDENT_LIBS 
            Boost::system Boost::context
            spdlog::spdlog_header_only 
            fmt::fmt-header-only 
            protobuf::libprotobuf
)


find_package(Boost COMPONENTS REQUIRED coroutine context system)

find_package(Protobuf CONFIG REQUIRED)

find_package(Threads REQUIRED)

find_package(spdlog CONFIG REQUIRED)

# ¶¯Ì¬¿â
#set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
#add_library(AlgoClientSDK SHARED ${ClientLibSourceFiles} ${protocalFiles})
#set_target_properties(AlgoClientSDK PROPERTIES OUTPUT_NAME "AlgoClientSDK")
#target_link_libraries(AlgoClientSDK  ${DEPENDENT_LIBS})
#target_compile_options(AlgoClientSDK PRIVATE -fPIC)
#target_compile_definitions(AlgoClientSDK PRIVATE ALGOCLIENT_EXPORTS)


#¾²Ì¬¿â
add_library(AlgoClientSDK STATIC ${ClientLibSourceFiles} ${protocalFiles})
set_target_properties(AlgoClientSDK PROPERTIES OUTPUT_NAME "AlgoClientSDK")
target_link_libraries(AlgoClientSDK ${DEPENDENT_LIBS})
target_compile_options(AlgoClientSDK PRIVATE -fvisibility=default)


# example 
add_executable(clientTest ./example/clientTest.cpp)
target_include_directories(clientTest PRIVATE ${PROJECT_SOURCE_DIR} )
target_link_libraries(clientTest AlgoClientSDK)
