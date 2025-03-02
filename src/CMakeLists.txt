cmake_minimum_required(VERSION 3.16)


option(ENABLE_DELAY_STATS "Enable stats onMarketDepth callback TimeCost " OFF)
option(OUTPUT_LOG_TO_STD_OUT "Enable output log to stdout for dev " ON)

project(AlgoPulse)

set(Boost_NO_WARN_NEW_VERSIONS 1)

message("ENABLE_DELAY_STATS:"    ${ENABLE_DELAY_STATS})
message("OUTPUT_LOG_TO_STD_OUT:" ${OUTPUT_LOG_TO_STD_OUT})

link_directories( "${LinkDirs}" )
include_directories( "${IncludeDirs}" )

if(WIN32)
    set(DEPENDENT_LIBS  ws2_32 wsock32 hdf5::hdf5-shared jemalloc)
elseif(UNIX)
    set(DEPENDENT_LIBS  hdf5 pthread dl szip z)
endif()

list(APPEND DEPENDENT_LIBS 
            spdlog::spdlog_header_only 
            fmt::fmt-header-only 
            unofficial::sqlite3::sqlite3
            protobuf::libprotobuf
)

message(status ${DEPENDENT_LIBS})

find_package(ZLIB REQUIRED)

find_package(Boost COMPONENTS REQUIRED coroutine context system)

find_package(unofficial-sqlite3 CONFIG REQUIRED)

find_package(Protobuf CONFIG REQUIRED)

find_package(Threads REQUIRED)

find_package(spdlog CONFIG REQUIRED)

find_package(hdf5 CONFIG REQUIRED)

file(GLOB Algo_FILES  ${PROJECT_SOURCE_DIR}/algo/*.cpp  
        ${PROJECT_SOURCE_DIR}/client/*.cpp  
        ${PROJECT_SOURCE_DIR}/shot/*.cpp 
         ${PROJECT_SOURCE_DIR}/t0/*.cpp )

file(GLOB Requests_FILES ${PROJECT_SOURCE_DIR}/third_party/requests/*.cpp )

file(GLOB Common_FILES  ${PROJECT_SOURCE_DIR}/common/*.cpp 
        ${PROJECT_SOURCE_DIR}/quote/*.cpp
        ${PROJECT_SOURCE_DIR}/orderbook/*.cpp
        ${PROJECT_SOURCE_DIR}/services/*.cpp )


set(algo_protocal_path ${PROJECT_SOURCE_DIR}/algo_protocal)
set(GENERATED_PROTO_SRC ${algo_protocal_path}/AlgoMessages.pb.cc)
set(GENERATED_PROTO_HDR ${algo_protocal_path}/AlgoMessages.pb.h)

add_custom_command(
    OUTPUT ${GENERATED_PROTO_SRC} ${GENERATED_PROTO_HDR}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} --cpp_out=${algo_protocal_path} --proto_path ${algo_protocal_path} ${algo_protocal_path}/AlgoMessages.proto
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} --pyi_out=${algo_protocal_path} --python_out=${algo_protocal_path} --proto_path ${algo_protocal_path} ${algo_protocal_path}/AlgoMessages.proto
    DEPENDS ${algo_protocal_path}/AlgoMessages.proto
    COMMENT "Generating .pb.cc and .pb.h files from AlgoMessages.proto"
)

add_executable(algoPulse "main.cpp" ${GENERATED_PROTO_SRC} ${Requests_FILES} ${Common_FILES} ${Algo_FILES})
add_subdirs_to_target_include_directories(algoPulse ${PROJECT_SOURCE_DIR})
target_link_libraries(algoPulse Boost::coroutine Boost::system Boost::context ${DEPENDENT_LIBS})

target_compile_definitions(algoPulse PRIVATE ENABLE_DELAY_STATS=${ENABLE_DELAY_STATS})
target_compile_definitions(algoPulse PRIVATE OUTPUT_LOG_TO_STD_OUT=${OUTPUT_LOG_TO_STD_OUT})
target_compile_definitions(algoPulse PRIVATE SPDLOG_ACTIVE_LEVEL=2)

install(TARGETS algoPulse
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

if(WIN32) 

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(DEPENDENT_DLLS_PATH "${VCPKG_INSTALLED_DIR}/x64-windows/debug/bin")
        foreach(DLL "hdf5_D.dll" "libprotobufd.dll" "zlibd1.dll" "szip_D.dll" "sqlite3.dll" "jemalloc.dll")
            list(APPEND DEPENDENT_DLLS "${DEPENDENT_DLLS_PATH}/${DLL}")
        endforeach()
    else()
        set(DEPENDENT_DLLS_PATH "${VCPKG_INSTALLED_DIR}/x64-windows/bin")
         foreach(DLL "hdf5.dll" "libprotobuf.dll" "zlib1.dll" "szip.dll" "sqlite3.dll" "jemalloc.dll")
            list(APPEND DEPENDENT_DLLS "${DEPENDENT_DLLS_PATH}/${DLL}")
        endforeach()
    endif()

    foreach(DLL ${DEPENDENT_DLLS})
       install(FILES "${DLL}" DESTINATION bin)
    endforeach()

    set(TARGET_OUTPUT_DIR $<TARGET_FILE_DIR:algoPulse>)
    message(${TARGET_OUTPUT_DIR})

    add_custom_command(TARGET algoPulse POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying dependencies to ${TARGET_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DEPENDENT_DLLS} ${TARGET_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/../config/config.ini ${TARGET_OUTPUT_DIR}/../config
)
endif()

install(DIRECTORY
    ${PROJECT_SOURCE_DIR}/../config
    DESTINATION .
)
