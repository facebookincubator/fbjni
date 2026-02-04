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

#include "CoreClasses.h"
#include "Log.h"

#ifndef FBJNI_NO_EXCEPTION_PTR
#include <lyra/lyra.h>
#include <lyra/lyra_exceptions.h>
#endif

#include <stdio.h>
#include <cstdlib>
#include <ios>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

#include <jni.h>

#ifndef _WIN32
#include <cxxabi.h>
#endif

namespace facebook {
namespace jni {

namespace {
class JRuntimeException : public JavaClass<JRuntimeException, JThrowable> {
 public:
  static auto constexpr kJavaDescriptor = "Ljava/lang/RuntimeException;";

  static local_ref<JRuntimeException> create(const char* str) {
    return newInstance(make_jstring(str));
  }

  static local_ref<JRuntimeException> create() {
    return newInstance();
  }
};

class JIllegalArgumentException
    : public JavaClass<JIllegalArgumentException, JThrowable> {
 public:
  static auto constexpr kJavaDescriptor =
      "Ljava/lang/IllegalArgumentException;";

  static local_ref<JIllegalArgumentException> create(const char* str) {
    return newInstance(make_jstring(str));
  }
};

class JIOException : public JavaClass<JIOException, JThrowable> {
 public:
  static auto constexpr kJavaDescriptor = "Ljava/io/IOException;";

  static local_ref<JIOException> create(const char* str) {
    return newInstance(make_jstring(str));
  }
};

class JOutOfMemoryError : public JavaClass<JOutOfMemoryError, JThrowable> {
 public:
  static auto constexpr kJavaDescriptor = "Ljava/lang/OutOfMemoryError;";

  static local_ref<JOutOfMemoryError> create(const char* str) {
    return newInstance(make_jstring(str));
  }
};

class JArrayIndexOutOfBoundsException
    : public JavaClass<JArrayIndexOutOfBoundsException, JThrowable> {
 public:
  static auto constexpr kJavaDescriptor =
      "Ljava/lang/ArrayIndexOutOfBoundsException;";

  static local_ref<JArrayIndexOutOfBoundsException> create(const char* str) {
    return newInstance(make_jstring(str));
  }
};

class JUnknownCppException
    : public JavaClass<JUnknownCppException, JThrowable> {
 public:
  static auto constexpr kJavaDescriptor =
      "Lcom/facebook/jni/UnknownCppException;";

  static local_ref<JUnknownCppException> create() {
    return newInstance();
  }

  static local_ref<JUnknownCppException> create(const char* str) {
    return newInstance(make_jstring(str));
  }
};

class JCppSystemErrorException
    : public JavaClass<JCppSystemErrorException, JThrowable> {
 public:
  static auto constexpr kJavaDescriptor =
      "Lcom/facebook/jni/CppSystemErrorException;";

  static local_ref<JCppSystemErrorException> create(
      const std::system_error& e) {
    return newInstance(make_jstring(e.what()), e.code().value());
  }
};

// Exception throwing & translating functions
// //////////////////////////////////////////////////////

// Functions that throw Java exceptions

void setJavaExceptionAndAbortOnFailure(JNIEnv* env, jthrowable throwable) {
  if (throwable) {
    env->Throw(throwable);
  }
  if (env->ExceptionCheck() != JNI_TRUE) {
    FBJNI_LOGF("Failed to set Java exception");
  }
}

} // namespace

// Functions that throw C++ exceptions

// TODO(T6618159) Inject the c++ stack into the exception's stack trace. One
// issue: when a java exception is created, it captures the full java stack
// across jni boundaries. lyra will only capture the c++ stack to the jni
// boundary. So, as we pass the java exception up to c++, we need to capture
// the c++ stack and then insert it into the correct place in the java stack
// trace. Then, as the exception propagates across the boundaries, we will
// slowly fill in the c++ parts of the trace.
void throwPendingJniExceptionAsCppException() {
  JNIEnv* env = Environment::current();
  if (env->ExceptionCheck() == JNI_FALSE) {
    return;
  }

  auto throwable = env->ExceptionOccurred();
  if (!throwable) {
    throw std::runtime_error("Unable to get pending JNI exception.");
  }
  env->ExceptionClear();

  throw JniException(adopt_local(throwable));
}

void throwCppExceptionIf(bool condition) {
  if (!condition) {
    return;
  }

  auto env = Environment::current();
  if (env->ExceptionCheck() == JNI_TRUE) {
    throwPendingJniExceptionAsCppException();
    return;
  }

  throw JniException();
}

void throwNewJavaException(jthrowable throwable) {
  throw JniException(wrap_alias(throwable));
}

void throwNewJavaException(const char* throwableName, const char* msg) {
  // If anything of the fbjni calls fail, an exception of a suitable
  // form will be thrown, which is what we want.
  auto throwableClass = findClassLocal(throwableName);
  auto throwable = throwableClass->newObject(
      throwableClass->getConstructor<jthrowable(jstring)>(),
      make_jstring(msg).release());
  throwNewJavaException(throwable.get());
}

// jthrowable
// //////////////////////////////////////////////////////////////////////////////////////

local_ref<JThrowable> JThrowable::initCause(alias_ref<JThrowable> cause) {
  static auto meth =
      javaClassStatic()->getMethod<javaobject(alias_ref<javaobject>)>(
          "initCause");
  return meth(self(), cause);
}

auto JThrowable::getStackTrace() -> local_ref<JStackTrace> {
  static auto meth =
      javaClassStatic()->getMethod<JStackTrace::javaobject()>("getStackTrace");
  return meth(self());
}

void JThrowable::setStackTrace(alias_ref<JStackTrace> stack) {
  static auto meth = javaClassStatic()->getMethod<void(alias_ref<JStackTrace>)>(
      "setStackTrace");
  return meth(self(), stack);
}

auto JThrowable::getMessage() -> local_ref<JString> {
  static auto meth =
      javaClassStatic()->getMethod<JString::javaobject()>("getMessage");
  return meth(self());
}

auto JStackTraceElement::create(
    const std::string& declaringClass,
    const std::string& methodName,
    const std::string& file,
    int line) -> local_ref<javaobject> {
  return newInstance(declaringClass, methodName, file, line);
}

std::string JStackTraceElement::getClassName() const {
  static auto meth =
      javaClassStatic()->getMethod<local_ref<JString>()>("getClassName");
  return meth(self())->toStdString();
}

std::string JStackTraceElement::getMethodName() const {
  static auto meth =
      javaClassStatic()->getMethod<local_ref<JString>()>("getMethodName");
  return meth(self())->toStdString();
}

std::string JStackTraceElement::getFileName() const {
  static auto meth =
      javaClassStatic()->getMethod<local_ref<JString>()>("getFileName");
  return meth(self())->toStdString();
}

int JStackTraceElement::getLineNumber() const {
  static auto meth = javaClassStatic()->getMethod<jint()>("getLineNumber");
  return meth(self());
}

// Translate C++ to Java Exception

namespace {

// For each exception in the chain of the exception_ptr argument, func
// will be called with that exception (in reverse order, i.e. innermost first).
#ifndef FBJNI_NO_EXCEPTION_PTR
void denest(
    const std::function<void(std::exception_ptr)>& func,
    std::exception_ptr ptr) {
  FBJNI_ASSERT(ptr);
  try {
    std::rethrow_exception(ptr);
  } catch (const std::nested_exception& e) {
    denest(func, e.nested_ptr());
  } catch (...) {
    // ignored.
  }
  func(ptr);
}
#endif

} // namespace

#ifndef FBJNI_NO_EXCEPTION_PTR
local_ref<JStackTraceElement> createJStackTraceElement(
    const lyra::StackTraceElement& cpp) {
  return JStackTraceElement::create(
      "|lyra|{" + cpp.libraryName() + "}",
      cpp.functionName(),
      cpp.buildId(),
      cpp.libraryOffset());
}

void addCppStacktraceToJavaException(
    alias_ref<JThrowable> java,
    std::exception_ptr cpp) {
  auto cppStack = lyra::getStackTraceSymbols(
      (cpp == nullptr) ? lyra::getStackTrace() : lyra::getExceptionTrace(cpp));

  auto javaStack = java->getStackTrace();
  auto newStack =
      JThrowable::JStackTrace::newArray(javaStack->size() + cppStack.size());
  size_t i = 0;
  for (size_t j = 0; j < cppStack.size(); j++, i++) {
    (*newStack)[i] = createJStackTraceElement(cppStack[j]);
  }
  for (size_t j = 0; j < javaStack->size(); j++, i++) {
    (*newStack)[i] = (*javaStack)[j];
  }
  java->setStackTrace(newStack);
}

local_ref<JThrowable> convertCppExceptionToJavaException(
    std::exception_ptr ptr) {
  FBJNI_ASSERT(ptr);
  local_ref<JThrowable> current;
  try {
    std::rethrow_exception(ptr);
  } catch (JniException& ex) {
    current = ex.releaseThrowable();
  } catch (const std::ios_base::failure& ex) {
    current = JIOException::create(ex.what());
  } catch (const std::invalid_argument& ex) {
    current = JIllegalArgumentException::create(ex.what());
  } catch (const std::bad_alloc& ex) {
    current = JOutOfMemoryError::create(ex.what());
  } catch (const std::out_of_range& ex) {
    current = JArrayIndexOutOfBoundsException::create(ex.what());
  } catch (const std::system_error& ex) {
    current = JCppSystemErrorException::create(ex);
  } catch (const std::runtime_error& ex) {
    current = JRuntimeException::create(ex.what());
  } catch (const std::exception& ex) {
    current = JCppException::create(ex.what());
  } catch (const char* msg) {
    current = JUnknownCppException::create(msg);
  } catch (...) {
#ifdef _WIN32
    current = JUnknownCppException::create();
#else
    const std::type_info* tinfo = abi::__cxa_current_exception_type();
    if (tinfo) {
      std::string msg = std::string("Unknown: ") + tinfo->name();
      current = JUnknownCppException::create(msg.c_str());
    } else {
      current = JUnknownCppException::create();
    }
#endif
  }

  addCppStacktraceToJavaException(current, ptr);
  return current;
}
#endif

local_ref<JThrowable> getJavaExceptionForCppBackTrace() {
  return getJavaExceptionForCppBackTrace(nullptr);
}

local_ref<JThrowable> getJavaExceptionForCppBackTrace(const char* msg) {
  local_ref<JThrowable> current =
      msg ? JUnknownCppException::create(msg) : JUnknownCppException::create();
#ifndef FBJNI_NO_EXCEPTION_PTR
  addCppStacktraceToJavaException(current, nullptr);
#endif
  return current;
}

#ifndef FBJNI_NO_EXCEPTION_PTR
local_ref<JThrowable> getJavaExceptionForCppException(std::exception_ptr ptr) {
  FBJNI_ASSERT(ptr);
  local_ref<JThrowable> previous;
  auto func = [&previous](std::exception_ptr ptr) {
    auto current = convertCppExceptionToJavaException(ptr);
    if (previous) {
      current->initCause(previous);
    }
    previous = current;
  };
  denest(func, ptr);
  return previous;
}
#endif

void translatePendingCppExceptionToJavaException() {
  try {
    // Manually managing the local ref's lifetime to avoid
    // any additional JNI calls (eg from asserts) after the
    // exception has been set
#ifndef FBJNI_NO_EXCEPTION_PTR
    auto exc =
        getJavaExceptionForCppException(std::current_exception()).release();
#else
    auto exc = JUnknownCppException::create().release();
#endif
    auto env = Environment::current();
    setJavaExceptionAndAbortOnFailure(env, exc);
    env->DeleteLocalRef(exc);
  } catch (...) {
#ifndef FBJNI_NO_EXCEPTION_PTR
    FBJNI_LOGE(
        "Unexpected error in translatePendingCppExceptionToJavaException(): %s",
        lyra::toString(std::current_exception()).c_str());
#else
    FBJNI_LOGE(
        "Unexpected error in translatePendingCppExceptionToJavaException()");
#endif
    std::terminate();
  }
}

// JniException
// ////////////////////////////////////////////////////////////////////////////////////

namespace {
constexpr const char* kExceptionMessageFailure =
    "Unable to get exception message.";
}

JniException::JniException() : JniException(JRuntimeException::create()) {}

JniException::JniException(alias_ref<jthrowable> throwable)
    : isMessageExtracted_(false) {
  throwable_ = make_global(throwable);
}

JniException::JniException(JniException&& rhs)
    : throwable_(std::move(rhs.throwable_)),
      what_(std::move(rhs.what_)),
      isMessageExtracted_(rhs.isMessageExtracted_) {}

JniException::JniException(const JniException& rhs)
    : what_(rhs.what_), isMessageExtracted_(rhs.isMessageExtracted_) {
  throwable_ = make_global(rhs.throwable_);
}

JniException::~JniException() {
  try {
    ThreadScope ts;
    throwable_.reset();
  } catch (...) {
    FBJNI_LOGE("Exception in ~JniException()");
    std::terminate();
  }
}

local_ref<JThrowable> JniException::getThrowable() const noexcept {
  return make_local(throwable_);
}

local_ref<JThrowable> JniException::releaseThrowable() noexcept {
  return make_local(throwable_.releaseAlias());
}

namespace {

// NOTE: This uses "raw" JNI calls to avoid the possibility of a recursive
//       issue.
std::optional<std::string> jni_message(alias_ref<JThrowable> throwable) {
  auto* const env = Environment::current();
  if (env == nullptr) {
    return std::nullopt;
  }

  // Make sure there isn't an exception pending on the way out.
  struct EnvClear final {
    JNIEnv& e;
    explicit EnvClear(JNIEnv& e) : e(e) {}
    ~EnvClear() {
      if (e.ExceptionCheck()) {
        e.ExceptionClear();
      }
    }
  };
  EnvClear ec{*env};

#define NULL_CHECK(entity)   \
  if ((entity) == nullptr) { \
    return std::nullopt;     \
  }

  // Find StringWriter class and constructor
  auto stringWriterClass = adopt_local(env->FindClass("java/io/StringWriter"));
  NULL_CHECK(stringWriterClass)
  auto stringWriter = [&]() -> local_ref<jobject> {
    jmethodID stringWriterCtor =
        env->GetMethodID(stringWriterClass.get(), "<init>", "()V");
    if (stringWriterCtor == nullptr) {
      return nullptr;
    }
    return adopt_local(
        env->NewObject(stringWriterClass.get(), stringWriterCtor));
  }();
  NULL_CHECK(stringWriter)

  {
    // Find PrintWriter class and constructor that takes a Writer
    auto printWriter = [&]() -> local_ref<jobject> {
      auto printWriterClass =
          adopt_local(env->FindClass("java/io/PrintWriter"));
      if (printWriterClass == nullptr) {
        return nullptr;
      }
      jmethodID printWriterCtor = env->GetMethodID(
          printWriterClass.get(), "<init>", "(Ljava/io/Writer;)V");
      if (printWriterCtor == nullptr) {
        return nullptr;
      }
      return adopt_local(env->NewObject(
          printWriterClass.get(), printWriterCtor, stringWriter.get()));
    }();
    NULL_CHECK(printWriter)

    // Call throwable.printStackTrace(printWriter)
    {
      auto throwableClass = adopt_local(env->FindClass("java/lang/Throwable"));
      NULL_CHECK(throwableClass)
      jmethodID printStackTraceMethod = env->GetMethodID(
          throwableClass.get(), "printStackTrace", "(Ljava/io/PrintWriter;)V");
      NULL_CHECK(printStackTraceMethod)
      env->CallVoidMethod(
          throwable.get(), printStackTraceMethod, printWriter.get());
      if (env->ExceptionCheck()) {
        return std::nullopt;
      }
    }
  }

  std::string resultStr;
  {
    // Call stringWriter.toString()
    auto result = [&]() -> local_ref<jstring> {
      jmethodID toStringMethod = env->GetMethodID(
          stringWriterClass.get(), "toString", "()Ljava/lang/String;");
      if (toStringMethod == nullptr) {
        return nullptr;
      }
      return adopt_local<jstring>(reinterpret_cast<jstring>(
          env->CallObjectMethod(stringWriter.get(), toStringMethod)));
    }();
    NULL_CHECK(result)

    // Convert jstring to std::string
    const auto* chars = env->GetStringUTFChars(result.get(), nullptr);
    NULL_CHECK(chars)
    resultStr = chars;
    env->ReleaseStringUTFChars(result.get(), chars);
  }

  return resultStr;
#undef NULL_CHECK
}

constexpr bool kUseJniMessageCode =
#if defined(JNI_EXCEPTION_POPULATE_INTERNAL_EXPERIMENTING_JNI)
    true;
#else
    false;
#endif

constexpr bool kExceptionMessageWithClassLoader =
#if defined(JNI_EXCEPTION_POPULATE_INTERNAL_EXPERIMENTING)
    true;
#else
    false;
#endif

} // namespace

// TODO 6900503: consider making this thread-safe.
void JniException::populateWhat() const noexcept {
  ThreadScope ts;

  if (kUseJniMessageCode) {
    auto msg_opt = jni_message(throwable_.get());
    if (msg_opt) {
      what_ = std::move(*msg_opt);
      isMessageExtracted_ = true;
    } else {
      // NOTE: This does not look recursion-safe.
      try {
        what_ = throwable_->toString();
        what_ += " (stack trace extraction failure)";
        isMessageExtracted_ = true;
      } catch (...) {
        what_ = kExceptionMessageFailure;
      }
    }
    return;
  }

  if (kExceptionMessageWithClassLoader) {
    try {
      // If it is a pending JNI exception, then we need the class loader to
      // ensure that the java class of the `throwable_` is available
      facebook::jni::ThreadScope::WithClassLoader([&] {
        static auto exceptionHelperClass =
            findClassStatic("com/facebook/jni/ExceptionHelper");
        static auto getErrorDescriptionMethod =
            exceptionHelperClass->getStaticMethod<std::string(jthrowable)>(
                "getErrorDescription");
        what_ = "Experimenting: " +
            getErrorDescriptionMethod(exceptionHelperClass, throwable_.get())
                ->toStdString();
        isMessageExtracted_ = true;
      });
      return;
    } catch (...) {
    }
    // Fallthrough intended
  }

  try {
    static auto exceptionHelperClass =
        findClassStatic("com/facebook/jni/ExceptionHelper");
    static auto getErrorDescriptionMethod =
        exceptionHelperClass->getStaticMethod<std::string(jthrowable)>(
            "getErrorDescription");
    what_ = getErrorDescriptionMethod(exceptionHelperClass, throwable_.get())
                ->toStdString();
    isMessageExtracted_ = true;
  } catch (const std::exception& e) {
    try {
      what_ = throwable_->toString();

      if (auto message = e.what()) {
        what_ += " (stack trace extraction failure: ";
        what_ += message;
        what_ += ")";
      }

      isMessageExtracted_ = true;
    } catch (...) {
      what_ = kExceptionMessageFailure;
    }
  } catch (...) {
    try {
      what_ = throwable_->toString();
      isMessageExtracted_ = true;
    } catch (...) {
      what_ = kExceptionMessageFailure;
    }
  }
}

const char* JniException::what() const noexcept {
  if (!isMessageExtracted_) {
    populateWhat();
  }
  return what_.c_str();
}

void JniException::setJavaException() const noexcept {
  auto env = Environment::current();
  setJavaExceptionAndAbortOnFailure(env, throwable_.get());
}

} // namespace jni
} // namespace facebook
