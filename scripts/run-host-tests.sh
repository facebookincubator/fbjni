#!/bin/bash
# Copyright (c) Facebook, Inc. and its affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -exo pipefail


mkdir -p "host-build-cmake"
cd "host-build-cmake"

# Configure CMake project
cmake -DJAVA_HOME="$JAVA_HOME" ..

# Build binaries and libraries
cmake --build .

# Run C++ tests
make test

cd ..

# LD_LIBRARY_PATH is needed for native library dependencies to load cleanly
export LD_LIBRARY_PATH="./host-build-cmake:./host-build-cmake/test/jni"

# Build and run JNI tests
./gradlew -b host.gradle -PbuildDir=host-build-gradle test
