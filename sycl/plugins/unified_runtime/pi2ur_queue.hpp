//===---------------- pi2ur.hpp - PI API to UR API  --------------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//
#pragma once

#include <unordered_map>

#include "ur_api.h"
#include <sycl/detail/pi.h>
#include <ur/ur.hpp>
#include "ur/adapters/level_zero/ur_level_zero.hpp"
#include "pi2ur_common.hpp"

namespace pi2ur {

// inline pi_result piQueueCreate(pi_context Context, pi_device Device,
//                         pi_queue_properties Flags, pi_queue *Queue) {

//   ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);
//   ur_device_handle_t hDevice = reinterpret_cast<ur_device_handle_t>(Device);
//   ur_queue_property_t props {};
//   ur_queue_handle_t *phQueue = reinterpret_cast<ur_queue_handle_t *>(Queue);

//   printf("%s %d\n", __FILE__, __LINE__);

//   HANDLE_ERRORS(urQueueCreate(hContext,hDevice, &props, phQueue));

//   return PI_SUCCESS;
// }

// inline pi_result piQueueGetInfo(pi_queue Queue, pi_queue_info ParamName,
//                          size_t ParamValueSize, void *ParamValue,
//                          size_t *ParamValueSizeRet) {

//   ur_queue_handle_t hQueue = reinterpret_cast<ur_queue_handle_t>(Queue);
//   ur_queue_info_t propName {};

//   HANDLE_ERRORS(urQueueGetInfo(hQueue, propName, ParamValueSize, ParamValue, ParamValueSizeRet));

//   return PI_SUCCESS;
// }

// inline pi_result piQueueRetain(pi_queue Queue) {
//   ur_queue_handle_t hQueue = reinterpret_cast<ur_queue_handle_t>(Queue);
  
//   HANDLE_ERRORS(urQueueRetain(hQueue));

//   return PI_SUCCESS;
// }

// inline pi_result piQueueRelease(pi_queue Queue) {
//   PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

//   ur_queue_handle_t hQueue = reinterpret_cast<ur_queue_handle_t>(Queue);
  
//   HANDLE_ERRORS(urQueueRelease(hQueue));

//   return PI_SUCCESS;
// }

// inline pi_result piQueueFinish(pi_queue Queue) {
//   ur_queue_handle_t hQueue = reinterpret_cast<ur_queue_handle_t>(Queue);
  
//   HANDLE_ERRORS(urQueueFinish(hQueue));

//   return PI_SUCCESS;
// }

// inline pi_result piQueueFlushj(pi_queue Queue) {
//   ur_queue_handle_t hQueue = reinterpret_cast<ur_queue_handle_t>(Queue);
  
//   HANDLE_ERRORS(urQueueFlush(hQueue));

//   return PI_SUCCESS;
// }

} // namespace pi2ur
