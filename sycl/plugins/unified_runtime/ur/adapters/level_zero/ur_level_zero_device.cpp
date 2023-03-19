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
#include "ur_level_zero_device.hpp"
#include <ur_bindings.hpp>

UR_APIEXPORT ur_result_t UR_APICALL urDeviceSelectBinary(
    ur_device_handle_t
        hDevice, ///< [in] handle of the device to select binary for.
    const uint8_t **ppBinaries, ///< [in] the array of binaries to select from.
    uint32_t NumBinaries, ///< [in] the number of binaries passed in ppBinaries.
                          ///< Must greater than or equal to zero otherwise
                          ///< ::UR_RESULT_ERROR_INVALID_VALUE is returned.
    uint32_t *
        pSelectedBinary ///< [out] the index of the selected binary in the input
                        ///< array of binaries. If a suitable binary was not
                        ///< found the function returns ${X}_INVALID_BINARY.
) {
  // TODO: this is a bare-bones implementation for choosing a device image
  // that would be compatible with the targeted device. An AOT-compiled
  // image is preferred over SPIR-V for known devices (i.e. Intel devices)
  // The implementation makes no effort to differentiate between multiple images
  // for the given device, and simply picks the first one compatible.
  //
  // Real implementation will use the same mechanism OpenCL ICD dispatcher
  // uses. Something like:
  //   PI_VALIDATE_HANDLE_RETURN_HANDLE(ctx, PI_ERROR_INVALID_CONTEXT);
  //     return context->dispatch->piextDeviceSelectIR(
  //       ctx, images, num_images, selected_image);
  // where context->dispatch is set to the dispatch table provided by PI
  // plugin for platform/device the ctx was created for.

  // Look for GEN binary, which we known can only be handled by Level-Zero now.
  const char *BinaryTarget = __SYCL_PI_DEVICE_BINARY_TARGET_SPIRV64_GEN;

  pi_device_binary *Binaries = reinterpret_cast<pi_device_binary *>(const_cast<uint8_t **>(ppBinaries));

  pi_uint32 *SelectedBinaryInd = pSelectedBinary;

  // Find the appropriate device image, fallback to spirv if not found
  constexpr pi_uint32 InvalidInd = std::numeric_limits<pi_uint32>::max();
  pi_uint32 Spirv = InvalidInd;

  for (pi_uint32 i = 0; i < NumBinaries; ++i) {
    if (strcmp(Binaries[i]->DeviceTargetSpec, BinaryTarget) == 0) {
      *SelectedBinaryInd = i;
      return UR_RESULT_SUCCESS;
    }
    if (strcmp(Binaries[i]->DeviceTargetSpec,
               __SYCL_PI_DEVICE_BINARY_TARGET_SPIRV64) == 0)
      Spirv = i;
  }
  // Points to a spirv image, if such indeed was found
  if ((*SelectedBinaryInd = Spirv) != InvalidInd)
    return UR_RESULT_SUCCESS;

  // No image can be loaded for the given device
  return UR_RESULT_ERROR_INVALID_BINARY;
}

UR_APIEXPORT ur_result_t UR_APICALL urDeviceGetNativeHandle(
    ur_device_handle_t Device, ///< [in] handle of the device.
    ur_native_handle_t *NativeDevice ///< [out] a pointer to the native handle of the device.
) {
  *NativeDevice = reinterpret_cast<ur_native_handle_t>(Device->ZeDevice);
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urDeviceCreateWithNativeHandle(
    ur_native_handle_t NativeDevice, ///< [in] the native handle of the device.
    ur_platform_handle_t Platform,   ///< [in] handle of the platform instance
    ur_device_handle_t *Device ///< [out] pointer to the handle of the device object created.
) {
  auto ZeDevice = ur_cast<ze_device_handle_t>(NativeDevice);

  // The SYCL spec requires that the set of devices must remain fixed for the
  // duration of the application's execution. We assume that we found all of the
  // Level Zero devices when we initialized the platforms/devices cache, so the
  // "NativeHandle" must already be in the cache. If it is not, this must not be
  // a valid Level Zero device.
  //
  // TODO: maybe we should populate cache of platforms if it wasn't already.
  // For now assert that is was populated.
  UR_ASSERT(PiPlatformCachePopulated, UR_RESULT_ERROR_INVALID_VALUE);
  const std::lock_guard<SpinLock> Lock{*PiPlatformsCacheMutex};

  ur_device_handle_t Dev = nullptr;
  for (ur_platform_handle_t ThePlatform : *PiPlatformsCache) {
    Dev = ThePlatform->getDeviceFromNativeHandle(ZeDevice);
    if (Dev) {
      // Check that the input Platform, if was given, matches the found one.
      UR_ASSERT(!Platform || Platform == ThePlatform,
                UR_RESULT_ERROR_INVALID_PLATFORM);
      break;
    }
  }

  if (Dev == nullptr)
    return UR_RESULT_ERROR_INVALID_VALUE;

  *Device = Dev;
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urDeviceGetGlobalTimestamps(
    ur_device_handle_t hDevice, ///< [in] handle of the device instance
    uint64_t *pDeviceTimestamp, ///< [out][optional] pointer to the Device's
                                ///< global timestamp that correlates with the
                                ///< Host's global timestamp value
    uint64_t *pHostTimestamp ///< [out][optional] pointer to the Host's global
                             ///< timestamp that correlates with the Device's
                             ///< global timestamp value
) {
    _ur_device_handle_t *Device = reinterpret_cast<_ur_device_handle_t *>(hDevice);
    
    const uint64_t &ZeTimerResolution =
      Device->ZeDeviceProperties->timerResolution;
  const uint64_t TimestampMaxCount =
      ((1ULL << Device->ZeDeviceProperties->kernelTimestampValidBits) - 1ULL);
  uint64_t DeviceClockCount, Dummy;

  ZE2UR_CALL(zeDeviceGetGlobalTimestamps,
          (Device->ZeDevice, pHostTimestamp == nullptr ? &Dummy : pHostTimestamp,
           &DeviceClockCount));

  if (pDeviceTimestamp != nullptr) {
    *pDeviceTimestamp = (DeviceClockCount & TimestampMaxCount) * ZeTimerResolution;
  }

  return UR_RESULT_SUCCESS;
}
