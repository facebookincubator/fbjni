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

#include <exception>
#include <typeinfo>
#include <vector>

#include <lyra/lyra.h>

namespace facebook {
namespace lyra {

namespace detail {
struct ExceptionTraceHolder {
  ExceptionTraceHolder();
  // Need some virtual function to make this a polymorphic type.
  virtual ~ExceptionTraceHolder();
  ExceptionTraceHolder(const ExceptionTraceHolder&) = delete;
  ExceptionTraceHolder(ExceptionTraceHolder&&) = default;

  std::vector<InstructionPointer> stackTrace_;
};

template <typename E, bool hasTraceHolder>
struct Holder : E, ExceptionTraceHolder {
  Holder(E&& e) : E{std::forward<E>(e)}, ExceptionTraceHolder{} {}
};
template <typename E>
struct Holder<E, true> : E {
  Holder(E&& e) : E{std::forward<E>(e)} {}
};

const ExceptionTraceHolder* getExceptionTraceHolder(std::exception_ptr ptr);
} // namespace detail

/**
 * Retrieves the stack trace of an exception
 */
const std::vector<InstructionPointer>& getExceptionTrace(
    std::exception_ptr ptr);

/**
 * Throw an exception and store the stack trace. This works like
 * std::throw_with_nested in that it will actually throw a type that is
 * publicly derived from both E and detail::ExceptionTraceHolder.
 */
template <class E>
[[noreturn]] void fbthrow(E&& exception) {
  throw detail::
      Holder<E, std::is_base_of<detail::ExceptionTraceHolder, E>::value>{
          std::forward<E>(exception)};
}

/**
 * Ensure that a terminate handler that logs traces is installed.
 * setLibraryIdentifierFunction should be called first if the stack
 * trace should log build ids for libraries.
 */
void ensureRegisteredTerminateHandler();

/**
 * Helper to convert an exception to a string
 */
std::string toString(std::exception_ptr exceptionPointer);

/**
 * Lyra needs to hook either __cxa_init_primary_exception or __cxa_throw
 * (depending on the libc++ version) in order to inject stack traces. Users are
 * required to set up this hooking using the information in this struct:
 * - original is a pointer to the address of the exception function to hook.
 *   This pointer should have the address of the unhooked function stored back
 *   to it (if the hooking mechanism produces it), so that Lyra can delegate to
 *   the original function.
 * - replacement is the address of Lyra's replacement function. After the
 *   hooking, all callers to the original function should call the replacement
 *   function instead.
 */
struct HookInfo {
  void** original;
  void* replacement;
};

const HookInfo* getHookInfo();

void enableCxaThrowHookBacktraces(bool enable);

} // namespace lyra
} // namespace facebook
