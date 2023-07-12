/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <fbjni/Application.h>
#include <fbjni/File.h>
#include <fbjni/fbjni.h>

namespace facebook {
namespace jni {

class AContext : public JavaClass<AContext> {
 public:
  static constexpr const char* kJavaDescriptor = "Landroid/content/Context;";

  std::string getPackageName() {
    static const auto method =
        getClass()->getMethod<local_ref<JString>()>("getPackageName");
    return method(self())->toStdString();
  }

  // Define a method that calls into the represented Java class
  local_ref<JFile> getCacheDir() {
    static const auto method = getClass()->getMethod<JFile()>("getCacheDir");
    return method(self());
  }

  local_ref<JFile> getFilesDir() {
    static const auto method = getClass()->getMethod<JFile()>("getFilesDir");
    return method(self());
  }

  local_ref<AApplication> getApplication() {
    static const auto method =
        getClass()->getMethod<AApplication()>("getApplication");
    return method(self());
  }
};

} // namespace jni
} // namespace facebook
