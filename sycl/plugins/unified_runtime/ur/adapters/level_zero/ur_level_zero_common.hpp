//===--------- ur_level_zero.hpp - Level Zero Adapter -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//
#pragma once

#include <cassert>
#include <list>
#include <map>
#include <stdarg.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

#include <sycl/detail/pi.h>
#include <ur/ur.hpp>
#include <ur_api.h>
#include <ze_api.h>
#include <zes_api.h>

#include "ur/usm_allocator_config.hpp"

template <class To, class From> To ur_cast(From Value) {
  // TODO: see if more sanity checks are possible.
  assert(sizeof(From) == sizeof(To));
  return (To)(Value);
}

// Trace an internal PI call; returns in case of an error.
#define UR_CALL(Call)                                                          \
  {                                                                            \
    if (PrintTrace)                                                            \
      fprintf(stderr, "UR ---> %s\n", #Call);                                  \
    ur_result_t Result = (Call);                                                 \
    if (Result != UR_RESULT_SUCCESS)                                                  \
      return Result;                                                           \
  }

// Controls Level Zero calls tracing.
enum UrDebugLevel {
  UR_L0_DEBUG_NONE = 0x0,
  UR_L0_DEBUG_BASIC = 0x1,
  UR_L0_DEBUG_VALIDATION = 0x2,
  UR_L0_DEBUG_CALL_COUNT = 0x4,
  UR_L0_DEBUG_ALL = -1
};

const int UrL0Debug = [] {
  const char *DebugMode = std::getenv("UR_L0_DEBUG");
  return DebugMode ? std::atoi(DebugMode) : UR_L0_DEBUG_NONE;
}();

// Prints to stderr if UR_L0_DEBUG allows it
void urPrint(const char *Format, ...);

// Helper for one-liner validation
#define UR_ASSERT(condition, error)                                            \
  if (!(condition))                                                            \
    return error;

// Returns the ze_structure_type_t to use in .stype of a structured descriptor.
// Intentionally not defined; will give an error if no proper specialization
template <class T> ze_structure_type_t getZeStructureType();
template <class T> zes_structure_type_t getZesStructureType();

// The helpers to properly default initialize Level-Zero descriptor and
// properties structures.
template <class T> struct ZeStruct : public T {
  ZeStruct() : T{} { // zero initializes base struct
    this->stype = getZeStructureType<T>();
    this->pNext = nullptr;
  }
};

template <class T> struct ZesStruct : public T {
  ZesStruct() : T{} { // zero initializes base struct
    this->stype = getZesStructureType<T>();
    this->pNext = nullptr;
  }
};

// Map Level Zero runtime error code to UR error code.
ur_result_t ze2urResult(ze_result_t ZeResult);

// Trace a call to Level-Zero RT
#define ZE2UR_CALL(ZeName, ZeArgs)                                             \
  {                                                                            \
    ze_result_t ZeResult = ZeName ZeArgs;                                      \
    if (auto Result = ZeCall().doCall(ZeResult, #ZeName, #ZeArgs, true))       \
      return ze2urResult(Result);                                              \
  }

// Perform traced call to L0 without checking for errors
#define ZE_CALL_NOCHECK(ZeName, ZeArgs)                                        \
  ZeCall().doCall(ZeName ZeArgs, #ZeName, #ZeArgs, false)


// Record for a memory allocation. This structure is used to keep information
// for each memory allocation.
struct MemAllocRecord : _pi_object {
  MemAllocRecord(ur_context_handle_t Context, bool OwnZeMemHandle = true)
      : Context(Context), OwnZeMemHandle(OwnZeMemHandle) {}
  // Currently kernel can reference memory allocations from different contexts
  // and we need to know the context of a memory allocation when we release it
  // in piKernelRelease.
  // TODO: this should go away when memory isolation issue is fixed in the Level
  // Zero runtime.
  ur_context_handle_t Context;

  // Indicates if we own the native memory handle or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  bool OwnZeMemHandle;
};

extern usm_settings::USMAllocatorConfig USMAllocatorConfigInstance;

// Controls support of the indirect access kernels and deferred memory release.
const bool IndirectAccessTrackingEnabled = [] {
  return std::getenv("SYCL_PI_LEVEL_ZERO_TRACK_INDIRECT_ACCESS_MEMORY") !=
         nullptr;
}();

extern const bool UseUSMAllocator;

// The getInfo*/ReturnHelper facilities provide shortcut way of
// writing return bytes for the various getInfo APIs.
template <typename T, typename Assign>
ur_result_t urL0getInfoImpl(size_t param_value_size, void *param_value,
                        size_t *param_value_size_ret, T value,
                        size_t value_size, Assign &&assign_func) {

  if (param_value != nullptr) {

    if (param_value_size < value_size) {
      return UR_RESULT_ERROR_INVALID_VALUE;
    }

    assign_func(param_value, value, value_size);
  }

  if (param_value_size_ret != nullptr) {
    *param_value_size_ret = value_size;
  }

  return UR_RESULT_SUCCESS;
}

template <typename T>
ur_result_t urL0getInfo(size_t param_value_size, void *param_value,
                    size_t *param_value_size_ret, T value) {

  auto assignment = [](void *param_value, T value, size_t value_size) {
    (void)value_size;
    *static_cast<T *>(param_value) = value;
  };

  return urL0getInfoImpl(param_value_size, param_value, param_value_size_ret, value,
                     sizeof(T), assignment);
}

template <typename T>
ur_result_t urL0getInfoArray(size_t array_length, size_t param_value_size,
                         void *param_value, size_t *param_value_size_ret,
                         const T *value) {
  return urL0getInfoImpl(param_value_size, param_value, param_value_size_ret, value,
                     array_length * sizeof(T), memcpy);
}

template <typename T, typename RetType>
ur_result_t urL0getInfoArray(size_t array_length, size_t param_value_size,
                         void *param_value, size_t *param_value_size_ret,
                         const T *value) {
  if (param_value) {
    memset(param_value, 0, param_value_size);
    for (uint32_t I = 0; I < array_length; I++)
      ((RetType *)param_value)[I] = (RetType)value[I];
  }
  if (param_value_size_ret)
    *param_value_size_ret = array_length * sizeof(RetType);
  return UR_RESULT_SUCCESS;
}

template <>
inline ur_result_t
urL0getInfo<const char *>(size_t param_value_size, void *param_value,
                      size_t *param_value_size_ret, const char *value) {
  return urL0getInfoArray(strlen(value) + 1, param_value_size, param_value,
                      param_value_size_ret, value);
}

class UrL0ReturnHelperBase {
public:
  UrL0ReturnHelperBase(size_t param_value_size, void *param_value,
                 size_t *param_value_size_ret)
      : param_value_size(param_value_size), param_value(param_value),
        param_value_size_ret(param_value_size_ret) {}

  // A version where in/out info size is represented by a single pointer
  // to a value which is updated on return
  UrL0ReturnHelperBase(size_t *param_value_size, void *param_value)
      : param_value_size(*param_value_size), param_value(param_value),
        param_value_size_ret(param_value_size) {}

  // Scalar return value
  template <class T> ur_result_t operator()(const T &t) {
    return getInfo(param_value_size, param_value, param_value_size_ret, t);
  }

  // Array return value
  template <class T> ur_result_t operator()(const T *t, size_t s) {
    return urL0getInfoArray(s, param_value_size, param_value, param_value_size_ret,
                        t);
  }

  // Array return value where element type is differrent from T
  template <class RetType, class T>
  ur_result_t operator()(const T *t, size_t s) {
    return urL0getInfoArray<T, RetType>(s, param_value_size, param_value,
                                    param_value_size_ret, t);
  }

protected:
  size_t param_value_size;
  void *param_value;
  size_t *param_value_size_ret;
};

// A version of return helper that returns pi_result and not ur_result_t
class UrL0ReturnHelper : public UrL0ReturnHelperBase {
public:
  using UrL0ReturnHelperBase::UrL0ReturnHelperBase;

  template <class T> ur_result_t operator()(const T &t) {
    return UrL0ReturnHelperBase::operator()(t);
  }
  // Array return value
  template <class T> ur_result_t operator()(const T *t, size_t s) {
    return UrL0ReturnHelperBase::operator()(t, s);
  }
  // Array return value where element type is differrent from T
  template <class RetType, class T> ur_result_t operator()(const T *t, size_t s) {
    return UrL0ReturnHelperBase::operator()<RetType>(t, s);
  }
};
