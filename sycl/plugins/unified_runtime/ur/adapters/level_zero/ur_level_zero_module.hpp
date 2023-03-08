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

#include "ur_level_zero_common.hpp"

struct _ur_module_handle_t : _pi_object {
  _ur_module_handle_t(ur_context_handle_t hContext, const void *pIL, size_t length):
    hContext(hContext), pIL(pIL), length(length) {}

  // handle of the context instance.
  ur_context_handle_t hContext; 
  
  // pointer to IL string.
  const void *pIL;              

  // length of IL in bytes.
  size_t length;
};

struct _ur_kernel_handle_t : _pi_object {
  _ur_kernel_handle_t(ze_kernel_handle_t Kernel, bool OwnZeKernel, ur_program_handle_t Program):
    Program{Program}, ZeKernel{Kernel}, OwnZeKernel{OwnZeKernel}, SubmissionsCount{0}, MemAllocs{} {}

  _ur_kernel_handle_t(ze_kernel_handle_t Kernel, bool OwnZeKernel, ur_context_handle_t Context):
    Context{Context}, ZeKernel{Kernel}, OwnZeKernel{OwnZeKernel}, SubmissionsCount{0}, MemAllocs{} {}

  // Keep the program of the kernel.
  ur_context_handle_t Context;

  // Keep the program of the kernel.
  ur_program_handle_t Program;

    // Level Zero function handle.
  ze_kernel_handle_t ZeKernel;

  // Indicates if we own the ZeKernel or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  bool OwnZeKernel;

  // Counter to track the number of submissions of the kernel.
  // When this value is zero, it means that kernel is not submitted for an
  // execution - at this time we can release memory allocations referenced by
  // this kernel. We can do this when RefCount turns to 0 but it is too late
  // because kernels are cached in the context by SYCL RT and they are released
  // only during context object destruction. Regular RefCount is not usable to
  // track submissions because user/SYCL RT can retain kernel object any number
  // of times. And that's why there is no value of RefCount which can mean zero
  // submissions.
  std::atomic<uint32_t> SubmissionsCount;

  // Returns true if kernel has indirect access, false otherwise.
  bool hasIndirectAccess() {
    // Currently indirect access flag is set for all kernels and there is no API
    // to check if kernel actually indirectly access smth.
    return true;
  }

    // Hash function object for the unordered_set below.
  struct Hash {
    size_t operator()(const std::pair<void *const, MemAllocRecord> *P) const {
      return std::hash<void *>()(P->first);
    }
  };

  // If kernel has indirect access we need to make a snapshot of all existing
  // memory allocations to defer deletion of these memory allocations to the
  // moment when kernel execution has finished.
  // We store pointers to the elements because pointers are not invalidated by
  // insert/delete for std::unordered_map (iterators are invalidated). We need
  // to take a snapshot instead of just reference-counting the allocations,
  // because picture of active allocations can change during kernel execution
  // (new allocations can be added) and we need to know which memory allocations
  // were retained by this kernel to release them (and don't touch new
  // allocations) at kernel completion. Same kernel may be submitted several
  // times and retained allocations may be different at each submission. That's
  // why we have a set of memory allocations here and increase ref count only
  // once even if kernel is submitted many times. We don't want to know how many
  // times and which allocations were retained by each submission. We release
  // all allocations in the set only when SubmissionsCount == 0.
  std::unordered_set<std::pair<void *const, MemAllocRecord> *, Hash> MemAllocs;

  // Completed initialization of PI kernel. Must be called after construction.
  ur_result_t initialize();

  // Keeps info about an argument to the kernel enough to set it with
  // zeKernelSetArgumentValue.
  struct ArgumentInfo {
    uint32_t Index;
    size_t Size;
    // const _ur_mem_handle_t *Value;
    _ur_mem_handle_t *Value;
    _ur_mem_handle_t::access_mode_t AccessMode{_ur_mem_handle_t::unknown};
  };
  // Arguments that still need to be set (with zeKernelSetArgumentValue)
  // before kernel is enqueued.
  std::vector<ArgumentInfo> PendingArguments;

  // Cache of the kernel properties.
  ZeCache<ZeStruct<ze_kernel_properties_t>> ZeKernelProperties;
  ZeCache<std::string> ZeKernelName;
};
