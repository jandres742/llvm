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

// // Map of UR error codes to PI error codes
// pi_result ur2piResult(ur_result_t urResult);

// // Early exits on any error
// #define HANDLE_ERRORS(urCall)                                                  \
//   if (auto Result = urCall)                                                    \
//     return ur2piResult(Result);

