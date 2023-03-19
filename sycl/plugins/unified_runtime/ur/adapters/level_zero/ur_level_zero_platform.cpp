//===--------- ur_level_zero.hpp - Level Zero Adapter -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#include <algorithm>
#include <climits>
#include <string.h>

#include "ur_level_zero.hpp"
#include "ur_level_zero_platform.hpp"
#include <ur_bindings.hpp>

UR_APIEXPORT ur_result_t UR_APICALL urInit(
    ur_device_init_flags_t
        device_flags ///< [in] device initialization flags.
                     ///< must be 0 (default) or a combination of
                     ///< ::ur_device_init_flag_t.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urTearDown(
    void *pParams ///< [in] pointer to tear down parameters
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urPlatformGetApiVersion(
    ur_platform_handle_t hDriver, ///< [in] handle of the platform
    ur_api_version_t *pVersion    ///< [out] api version
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urPlatformGetNativeHandle(
    ur_platform_handle_t Platform,      ///< [in] handle of the platform.
    ur_native_handle_t *NativePlatform ///< [out] a pointer to the native
                                         ///< handle of the platform.
) {
  // Extract the Level Zero driver handle from the given PI platform
  *NativePlatform = reinterpret_cast<ur_native_handle_t>(Platform->ZeDriver);
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urPlatformCreateWithNativeHandle(
    ur_native_handle_t NativePlatform, ///< [in] the native handle of the platform.
    ur_platform_handle_t *Platform ///< [out] pointer to the handle of the
                                     ///< platform object created.
) {
  auto ZeDriver = ur_cast<ze_driver_handle_t>(NativePlatform);

  uint32_t NumPlatforms = 0;
  UR_CALL(urPlatformGet(0, nullptr, &NumPlatforms));

  if (NumPlatforms) {
    std::vector<ur_platform_handle_t> Platforms(NumPlatforms);
    UR_CALL(urPlatformGet(NumPlatforms, Platforms.data(), nullptr));

    // The SYCL spec requires that the set of platforms must remain fixed for
    // the duration of the application's execution. We assume that we found all
    // of the Level Zero drivers when we initialized the platform cache, so the
    // "NativeHandle" must already be in the cache. If it is not, this must not
    // be a valid Level Zero driver.
    for (const ur_platform_handle_t &CachedPlatform : Platforms) {
      if (CachedPlatform->ZeDriver == ZeDriver) {
        *Platform = CachedPlatform;
        return UR_RESULT_SUCCESS;
      }
    }
  }

  return UR_RESULT_ERROR_INVALID_VALUE;
}

UR_APIEXPORT ur_result_t UR_APICALL urGetLastResult(
    ur_platform_handle_t hPlatform, ///< [in] handle of the platform instance
    const char **ppMessage ///< [out] pointer to a string containing adapter
                           ///< specific result in string representation.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
