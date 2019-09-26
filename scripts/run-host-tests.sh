#!/bin/bash

set -exo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
CMAKE=$ANDROID_HOME/cmake/3.10.2.4988404/bin/cmake

cd "$DIR/.."

# Configure CMake project
$CMAKE -S . -B host-build-cmake -DJAVA_HOME="$JAVA_HOME"
# Build binaries and libraries
$CMAKE --build host-build-cmake
# Run C++ tests
$CMAKE --build host-build-cmake --target test
# LD_LIBRARY_PATH is needed for native library dependencies to load cleanly
TEST_LD_LIBRARY_PATH=$PWD/host-build-cmake:$PWD/host-build-cmake/test/jni
# Build and run JNI tests
env LD_LIBRARY_PATH="$TEST_LD_LIBRARY_PATH" ./gradlew -b host.gradle -PbuildDir=host-build-gradle test
