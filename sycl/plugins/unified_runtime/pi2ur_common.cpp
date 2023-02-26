//===---------------- pi2ur.hpp - PI API to UR API  --------------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//
#include "pi2ur_common.hpp"

// // Map of UR error codes to PI error codes
// pi_result ur2piResult(ur_result_t urResult) {
//   std::unordered_map<ur_result_t, pi_result> ErrorMapping = {
//       {UR_RESULT_SUCCESS, PI_SUCCESS},
//       {UR_RESULT_ERROR_UNKNOWN, PI_ERROR_UNKNOWN},
//       {UR_RESULT_ERROR_DEVICE_LOST, PI_ERROR_DEVICE_NOT_FOUND},
//       {UR_RESULT_ERROR_INVALID_OPERATION, PI_ERROR_INVALID_OPERATION},
//       {UR_RESULT_ERROR_INVALID_PLATFORM, PI_ERROR_INVALID_PLATFORM},
//       {UR_RESULT_ERROR_INVALID_ARGUMENT, PI_ERROR_INVALID_ARG_VALUE},
//       {UR_RESULT_ERROR_INVALID_VALUE, PI_ERROR_INVALID_VALUE},
//       {UR_RESULT_ERROR_INVALID_EVENT, PI_ERROR_INVALID_EVENT},
//       {UR_RESULT_ERROR_INVALID_BINARY, PI_ERROR_INVALID_BINARY},
//       {UR_RESULT_ERROR_INVALID_KERNEL_NAME, PI_ERROR_INVALID_KERNEL_NAME},
//       {UR_RESULT_ERROR_INVALID_FUNCTION_NAME, PI_ERROR_BUILD_PROGRAM_FAILURE},
//       {UR_RESULT_ERROR_INVALID_WORK_GROUP_SIZE,
//        PI_ERROR_INVALID_WORK_GROUP_SIZE},
//       {UR_RESULT_ERROR_MODULE_BUILD_FAILURE, PI_ERROR_BUILD_PROGRAM_FAILURE},
//       {UR_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, PI_ERROR_OUT_OF_RESOURCES},
//       {UR_RESULT_ERROR_OUT_OF_HOST_MEMORY, PI_ERROR_OUT_OF_HOST_MEMORY}};

//   auto It = ErrorMapping.find(urResult);
//   if (It == ErrorMapping.end()) {
//     return PI_ERROR_UNKNOWN;
//   }
//   return It->second;
// }