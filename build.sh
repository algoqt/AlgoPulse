#!/bin/bash

# may need:  dos2unix build.sh

BUILD_DIR="build"

if [ ! -d "${BUILD_DIR}" ]; then
    echo "create BUILD_DIR  ${BUILD_DIR}"
    mkdir -p "${BUILD_DIR}" 
fi

echo "vcpkg root: ${VCPKG_ROOT}"

vcpkg install --x-install-root=./"${BUILD_DIR}"/vcpkg_installed

if [ $? -ne 0 ]; then
    echo "vcpkg install Failed."
    exit 1
fi

cmake  -B "${BUILD_DIR}" -S . -G "Ninja" -DCMAKE_BUILD_TYPE="Release"  -DCMAKE_INSTALL_PREFIX=./"${BUILD_DIR}"

if [ $? -ne 0 ]; then
    echo "Error CMake Config Failed."
    exit 1
fi

cd "${BUILD_DIR}"

for target in algoPulse clientTest; do

    cmake --build . --target ${target} --config Release
    if [ $? -ne 0 ]; then
        echo "Error CMake Build Failed : ${target}"
        exit 1
    fi
done

echo "Build Success"

cmake --build . --target install --config Release

if [ $? -ne 0 ]; then
    echo "Error CMake install Failed."
    exit 1
fi