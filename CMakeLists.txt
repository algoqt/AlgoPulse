cmake_minimum_required(VERSION 3.16)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


option(ENABLE_DELAY_STATS "Enable stats onMarketDepth callback TimeCost " OFF)
option(OUTPUT_LOG_TO_STD_OUT "Enable output log to stdout for dev " ON)

project(AlgoPulse)

set(Boost_NO_WARN_NEW_VERSIONS 1)

set(VCPKG_INSTALLED_DIR "${PROJECT_SOURCE_DIR}/build/vcpkg_installed")

message(${VCPKG_INSTALLED_DIR})

if(WIN32)

    if (MSVC)
        add_compile_options(/utf-8)
        #set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    add_definitions(-D_WIN32_WINNT=0x0A00)

    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${VCPKG_INSTALLED_DIR}/x64-windows/share")

    set(LinkDirs    "${VCPKG_INSTALLED_DIR}/x64-windows/lib")
    set(IncludeDirs "${VCPKG_INSTALLED_DIR}/x64-windows/include")

    set(ZLIB_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/x64-windows/include")
    set(ZLIB_LIBRARY     "${VCPKG_INSTALLED_DIR}/x64-windows/lib")

    set(Boost_DIR         "${VCPKG_INSTALLED_DIR}/x64-windows/share")
    set(Boost_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/x64-windows/include")

elseif(UNIX)

    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${VCPKG_INSTALLED_DIR}/x64-linux/share")

    set(LinkDirs    "${VCPKG_INSTALLED_DIR}/x64-linux/lib")
    set(IncludeDirs "${VCPKG_INSTALLED_DIR}/x64-linux/include")

    set(Boost_DIR         "${VCPKG_INSTALLED_DIR}/x64-linux/share")
    set(Boost_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/x64-linux/include")

endif()

message(${LinkDirs})
message(${IncludeDirs})

function(add_subdirs_to_target_include_directories target base_dir)
    file(GLOB_RECURSE subdirs LIST_DIRECTORIES true "${base_dir}/*")
    foreach(subdir ${subdirs})
        if(IS_DIRECTORY ${subdir})
            target_include_directories(${target} PUBLIC ${subdir})
        endif()
    endforeach()
endfunction()

add_subdirectory(src)

add_subdirectory(src/client)