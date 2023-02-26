//===---------------- pi2ur.hpp - PI API to UR API  --------------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//
#pragma once

// #include <unordered_map>

// #include "ur_api.h"
// #include <sycl/detail/pi.h>
// #include <ur/ur.hpp>
// #include "ur/adapters/level_zero/ur_level_zero.hpp"
#include "pi2ur_common.hpp"


namespace pi2ur {

struct _pi_context : _pi_object {
  _pi_context(ur_context_handle_t UrContext): UrContext{UrContext}  {}

  ur_context_handle_t UrContext;
};


// inline pi_result piContextCreate(const pi_context_properties *Properties,
//                           pi_uint32 NumDevices, const pi_device *Devices,
//                           void (*PFnNotify)(const char *ErrInfo,
//                                             const void *PrivateInfo, size_t CB,
//                                             void *UserData),
//                           void *UserData, pi_context *RetContext) {
//   printf("%s %d\n", __FILE__, __LINE__);
//   uint32_t DeviceCount = reinterpret_cast<uint32_t>(NumDevices);
//   ur_device_handle_t *phDevices = reinterpret_cast<ur_device_handle_t *>(const_cast<pi_device *>(Devices));
//   ur_context_handle_t *phContext = reinterpret_cast<ur_context_handle_t *>(RetContext);

//   HANDLE_ERRORS(urContextCreate(DeviceCount, phDevices, phContext));

//   return PI_SUCCESS;
// }

// inline pi_result piContextGetInfo(pi_context Context, pi_context_info ParamName,
//                            size_t ParamValueSize, void *ParamValue,
//                            size_t *ParamValueSizeRet) {

//   ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);
//   ur_context_info_t ContextInfoType{};
//   (urContextGetInfo(hContext, ContextInfoType, ParamValueSize, ParamValue, ParamValueSizeRet));

//   return PI_SUCCESS;
// }

// inline pi_result piContextRetain(pi_context Context) {
//   ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);

//   (urContextRetain(hContext));

//   return PI_SUCCESS;
// }


// inline pi_result piContextRelease(pi_context Context) {
//   ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);

//   (urContextRelease(hContext));

//   return PI_SUCCESS;
// }

} // namespace pi2ur
