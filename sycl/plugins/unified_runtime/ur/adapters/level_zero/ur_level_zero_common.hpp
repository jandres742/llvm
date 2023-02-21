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

#include <sycl/detail/pi.h>
#include <ur/ur.hpp>
#include <ur_api.h>
#include <ze_api.h>
#include <zes_api.h>

#include "ur/usm_allocator_config.hpp"

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

// Record for a memory allocation. This structure is used to keep information
// for each memory allocation.
struct MemAllocRecord : _pi_object {
  MemAllocRecord(pi_context Context, bool OwnZeMemHandle = true)
      : Context(Context), OwnZeMemHandle(OwnZeMemHandle) {}
  // Currently kernel can reference memory allocations from different contexts
  // and we need to know the context of a memory allocation when we release it
  // in piKernelRelease.
  // TODO: this should go away when memory isolation issue is fixed in the Level
  // Zero runtime.
  pi_context Context;

  // Indicates if we own the native memory handle or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  bool OwnZeMemHandle;
};

extern usm_settings::USMAllocatorConfig USMAllocatorConfigInstance;
