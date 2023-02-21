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

// #include "ur_level_zero_common.hpp"
#include "ur_level_zero.hpp"

struct _ur_mem_handle_t : _pi_object {
  _ur_mem_handle_t() {}
};

struct ur_device_handle_t_;

ur_result_t USMFreeImpl(ur_context_handle_t Context, void *Ptr,
                             bool OwnZeMemHandle);

// Exception type to pass allocation errors
class UsmAllocationException {
  const ur_result_t Error;

public:
  UsmAllocationException(ur_result_t Err) : Error{Err} {}
  ur_result_t getError() const { return Error; }
};

// Implements memory allocation via L0 RT for USM allocator interface.
class USMMemoryAllocBase : public SystemMemory {
protected:
  ur_context_handle_t Context;
  ur_device_handle_t Device;
  // Internal allocation routine which must be implemented for each allocation
  // type
  virtual ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                                 uint32_t Alignment) = 0;

public:
  USMMemoryAllocBase(ur_context_handle_t Ctx, ur_device_handle_t Dev)
      : Context{Ctx}, Device{Dev} {}
  void *allocate(size_t Size) override final;
  void *allocate(size_t Size, size_t Alignment) override final;
  void deallocate(void *Ptr, bool OwnZeMemHandle) override final;
};

// Allocation routines for shared memory type
class USMSharedMemoryAlloc : public USMMemoryAllocBase {
protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                         uint32_t Alignment) override;

public:
  USMSharedMemoryAlloc(ur_context_handle_t Ctx, ur_device_handle_t Dev)
      : USMMemoryAllocBase(Ctx, Dev) {}
};

// Allocation routines for shared memory type that is only modified from host.
class USMSharedReadOnlyMemoryAlloc : public USMMemoryAllocBase {
protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                         uint32_t Alignment) override;

public:
  USMSharedReadOnlyMemoryAlloc(ur_context_handle_t Ctx, ur_device_handle_t Dev)
      : USMMemoryAllocBase(Ctx, Dev) {}
};

// Allocation routines for device memory type
class USMDeviceMemoryAlloc : public USMMemoryAllocBase {
protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                         uint32_t Alignment) override;

public:
  USMDeviceMemoryAlloc(ur_context_handle_t Ctx, ur_device_handle_t Dev)
      : USMMemoryAllocBase(Ctx, Dev) {}
};

// Allocation routines for host memory type
class USMHostMemoryAlloc : public USMMemoryAllocBase {
protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                         uint32_t Alignment) override;

public:
  USMHostMemoryAlloc(ur_context_handle_t Ctx) : USMMemoryAllocBase(Ctx, nullptr) {}
};

ur_result_t USMDeviceAllocImpl(void **ResultPtr, ur_context_handle_t Context,
                                    ur_device_handle_t Device,
                                    pi_usm_mem_properties *Properties,
                                    size_t Size, uint32_t Alignment);

ur_result_t USMSharedAllocImpl(void **ResultPtr, ur_context_handle_t Context,
                                    ur_device_handle_t Device, pi_usm_mem_properties *,
                                    size_t Size, uint32_t Alignment);

ur_result_t USMHostAllocImpl(void **ResultPtr, ur_context_handle_t Context,
                                  pi_usm_mem_properties *Properties,
                                  size_t Size, uint32_t Alignment);