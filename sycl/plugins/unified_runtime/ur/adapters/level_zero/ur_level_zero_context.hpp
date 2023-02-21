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

struct _ur_context_handle_t : _pi_object {
  _ur_context_handle_t(ze_context_handle_t ZeContext,
                       uint32_t NumDevices,
                       const pi_device *Devs,
                       bool OwnZeContext) :
    ZeContext{ZeContext}, Devices{Devs, Devs + NumDevices}, OwnZeContext{OwnZeContext} {}

  // A L0 context handle is primarily used during creation and management of
  // resources that may be used by multiple devices.
  // This field is only set at _pi_context creation time, and cannot change.
  // Therefore it can be accessed without holding a lock on this _pi_context.
  const ze_context_handle_t ZeContext {};

    // Keep the PI devices this PI context was created for.
  // This field is only set at _pi_context creation time, and cannot change.
  // Therefore it can be accessed without holding a lock on this _pi_context.
  const std::vector<pi_device> Devices;

    // Indicates if we own the ZeContext or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  bool OwnZeContext;

  // Immediate Level Zero command list for the device in this context, to be
  // used for initializations. To be created as:
  // - Immediate command list: So any command appended to it is immediately
  //   offloaded to the device.
  // - Synchronous: So implicit synchronization is made inside the level-zero
  //   driver.
  // There will be a list of immediate command lists (for each device) when
  // support of the multiple devices per context will be added.
  ze_command_list_handle_t ZeCommandListInit {};

  // Mutex for the immediate command list. Per the Level Zero spec memory copy
  // operations submitted to an immediate command list are not allowed to be
  // called from simultaneous threads.
  pi_mutex ImmediateCommandListMutex;

  // Mutex Lock for the Command List Cache. This lock is used to control both
  // compute and copy command list caches.
  pi_mutex ZeCommandListCacheMutex;

  // If context contains one device or sub-devices of the same device, we want
  // to save this device.
  // This field is only set at _pi_context creation time, and cannot change.
  // Therefore it can be accessed without holding a lock on this _pi_context.
  pi_device SingleRootDevice = nullptr;

  // Cache of all currently available/completed command/copy lists.
  // Note that command-list can only be re-used on the same device.
  //
  // TODO: explore if we should use root-device for creating command-lists
  // as spec says that in that case any sub-device can re-use it: "The
  // application must only use the command list for the device, or its
  // sub-devices, which was provided during creation."
  //
  std::unordered_map<ze_device_handle_t, std::list<ze_command_list_handle_t>>
      ZeComputeCommandListCache;
  std::unordered_map<ze_device_handle_t, std::list<ze_command_list_handle_t>>
      ZeCopyCommandListCache;
};
