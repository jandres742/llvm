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
};
