//===---------------- pi2ur.cpp - PI API to UR API  --------------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//

// This thin layer performs conversion from PI API to Unified Runtime API
// TODO: remove when SYCL RT is changed to talk in UR directly

// #include "pi2ur_common.hpp"
#include "pi2ur_context.hpp"

namespace pi2ur {
_pi_program::~_pi_program() {
  // According to Level Zero Specification, all kernels and build logs
  // must be destroyed before the Module can be destroyed.  So, be sure
  // to destroy build log before destroying the module.
#if 0
  if (ZeBuildLog) {
    ZE_CALL_NOCHECK(zeModuleBuildLogDestroy, (ZeBuildLog));
  }

  if (ZeModule && OwnZeModule) {
    ZE_CALL_NOCHECK(zeModuleDestroy, (ZeModule));
  }
#endif
}
}