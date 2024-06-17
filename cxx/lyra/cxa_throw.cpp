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

#include <atomic>
#include <cassert>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#ifndef _WIN32
#include <cxxabi.h>
#endif

#include <lyra/lyra_exceptions.h>

namespace facebook {
namespace lyra {

using namespace detail;

namespace {
std::atomic<bool> enableBacktraces{true};

const ExceptionTraceHolder* getExceptionTraceHolderInException(
    std::exception_ptr ptr) {
  try {
    std::rethrow_exception(ptr);
  } catch (const ExceptionTraceHolder& holder) {
    return &holder;
  } catch (...) {
    return nullptr;
  }
}
} // namespace

void enableCxaThrowHookBacktraces(bool enable) {
  enableBacktraces.store(enable, std::memory_order_relaxed);
}

// We want to attach stack traces to C++ exceptions. Our API contract is that
// calling lyra::getExceptionTrace on the exception_ptr for an exception should
// return the stack trace for that exception.
//
// We accomplish this by providing a hook for __cxa_init_primary_exception or
// __cxa_throw (depending on the libc++ version), which creates an
// ExceptionTraceHolder object (which captures the stack trace for the
// exception), and creates a mapping from the pointer to the exception object
// (which is a function parameter) to its ExceptionTraceHolder object.  This
// mapping can then be queried by lyra::getExceptionTrace to get the stack trace
// for the exception. We have a custom exception destructor to destroy the trace
// object and call the original destructor for the exception object, and our
// hook calls the original function for the actual exception functionality and
// passes this custom destructor.
//
// This works because the hooked function is only called when creating a new
// exception object, so at that point, we're able to capture the original stack
// trace for the exception.  Even if that exception is later rethrown, we'll
// still maintain its original stack trace, assuming that std::current_exception
// creates a reference to the current exception instead of copying it (which is
// true for both libstdc++ and libc++), such that we can still look up the
// exception object via pointer.
//
// We don't have to worry about any pointer adjustments for the exception object
// (e.g. for converting to or from a base class subobject pointer), because a
// std::exception_ptr will always capture the pointer to the exception object
// itself and not any subobjects.
//
// Our map must be global, since exceptions can be transferred across threads.
// Consequently, we must use a mutex to guard all map operations.

typedef void (*destructor_type)(void*);

namespace {
struct ExceptionState {
  ExceptionTraceHolder trace;
  destructor_type destructor;
};

// We create our map and mutex as function statics and leak them intentionally,
// to ensure they've been initialized before any global constructors and are
// also available to use inside any global destructors.
std::unordered_map<void*, ExceptionState>* get_exception_state_map() {
  static auto* exception_state_map =
      new std::unordered_map<void*, ExceptionState>();
  return exception_state_map;
}

std::mutex* get_exception_state_map_mutex() {
  static auto* exception_state_map_mutex = new std::mutex();
  return exception_state_map_mutex;
}

void trace_destructor(void* exception_obj) {
  destructor_type original_destructor = nullptr;

  {
    std::lock_guard<std::mutex> lock(*get_exception_state_map_mutex());
    auto* exception_state_map = get_exception_state_map();
    auto it = exception_state_map->find(exception_obj);
    if (it == exception_state_map->end()) {
      // This really shouldn't happen, but if it does, just leaking the trace
      // and exception object seems better than crashing.
      return;
    }

    original_destructor = it->second.destructor;
    exception_state_map->erase(it);
  }

  if (original_destructor) {
    original_destructor(exception_obj);
  }
}

// always_inline to avoid an unnecessary stack frame in the trace.
[[gnu::always_inline]]
void add_exception_trace(void* obj, destructor_type destructor) {
  if (enableBacktraces.load(std::memory_order_relaxed)) {
    std::lock_guard<std::mutex> lock(*get_exception_state_map_mutex());
    get_exception_state_map()->emplace(
        obj, ExceptionState{ExceptionTraceHolder(), destructor});
  }
}
} // namespace

#ifndef _WIN32
#if _LIBCPP_AVAILABILITY_HAS_INIT_PRIMARY_EXCEPTION
__attribute__((annotate("dynamic_fn_ptr"))) static abi::__cxa_exception* (
    *original_cxa_init_primary_exception)(
    void*,
    std::type_info*,
    destructor_type) = &abi::__cxa_init_primary_exception;

abi::__cxa_exception* cxa_init_primary_exception(
    void* obj,
    std::type_info* type,
    destructor_type destructor) {
  add_exception_trace(obj, destructor);
  return original_cxa_init_primary_exception(obj, type, trace_destructor);
}

const HookInfo* getHookInfo() {
  static const HookInfo info = {
      .original =
          reinterpret_cast<void**>(&original_cxa_init_primary_exception),
      .replacement = reinterpret_cast<void*>(&cxa_init_primary_exception),
  };
  return &info;
}
#else
[[gnu::noreturn]]
__attribute__((annotate("dynamic_fn_ptr"))) static void (*original_cxa_throw)(
    void*,
    std::type_info*,
    destructor_type) = &abi::__cxa_throw;

[[noreturn]] void
cxa_throw(void* obj, std::type_info* type, destructor_type destructor) {
  add_exception_trace(obj, destructor);
  original_cxa_throw(obj, type, trace_destructor);
}

const HookInfo* getHookInfo() {
  static const HookInfo info = {
      .original = reinterpret_cast<void**>(&original_cxa_throw),
      .replacement = reinterpret_cast<void*>(&cxa_throw),
  };
  return &info;
}
#endif
#endif

const ExceptionTraceHolder* detail::getExceptionTraceHolder(
    std::exception_ptr ptr) {
  {
    std::lock_guard<std::mutex> lock(*get_exception_state_map_mutex());
    // The exception object pointer isn't a public member of std::exception_ptr,
    // and there isn't any public method to get it. However, for both libstdc++
    // and libc++, it's the first pointer inside the exception_ptr, and we can
    // rely on the ABI of those libraries to remain stable, so we can just
    // access it directly.
    void* exception_obj = *reinterpret_cast<void**>(&ptr);
    auto* exception_state_map = get_exception_state_map();
    auto it = exception_state_map->find(exception_obj);
    if (it != exception_state_map->end()) {
      return &it->second.trace;
    }
  }

  // Fall back to attempting to retrieve the ExceptionTraceHolder directly from
  // the exception (to support e.g. fbthrow).
  return getExceptionTraceHolderInException(ptr);
}

} // namespace lyra
} // namespace facebook
