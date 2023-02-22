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

#include <ur/ur.hpp>
#include <ur_api.h>
#include <ze_api.h>
#include <zes_api.h>

#include "ur_level_zero_common.hpp"
#include "ur_level_zero_context.hpp"
#include "ur_level_zero_device.hpp"
#include "ur_level_zero_event.hpp"
#include "ur_level_zero_mem.hpp"
#include "ur_level_zero_module.hpp"
#include "ur_level_zero_native.hpp"
#include "ur_level_zero_platform.hpp"
#include "ur_level_zero_program.hpp"
#include "ur_level_zero_queue.hpp"
#include "ur_level_zero_sampler.hpp"

// Controls Level Zero calls serialization to w/a Level Zero driver being not MT
// ready. Recognized values (can be used as a bit mask):
enum {
  ZeSerializeNone =
      0, // no locking or blocking (except when SYCL RT requested blocking)
  ZeSerializeLock = 1, // locking around each ZE_CALL
  ZeSerializeBlock =
      2, // blocking ZE calls, where supported (usually in enqueue commands)
};
static const uint32_t ZeSerialize = [] {
  const char *SerializeMode = std::getenv("ZE_SERIALIZE");
  const uint32_t SerializeModeValue =
      SerializeMode ? std::atoi(SerializeMode) : 0;
  return SerializeModeValue;
}();

// This class encapsulates actions taken along with a call to Level Zero API.
class ZeCall {
private:
  // The global mutex that is used for total serialization of Level Zero calls.
  static std::mutex GlobalLock;

public:
  ZeCall() {
    if ((ZeSerialize & ZeSerializeLock) != 0) {
      GlobalLock.lock();
    }
  }
  ~ZeCall() {
    if ((ZeSerialize & ZeSerializeLock) != 0) {
      GlobalLock.unlock();
    }
  }

  // The non-static version just calls static one.
  ze_result_t doCall(ze_result_t ZeResult, const char *ZeName,
                     const char *ZeArgs, bool TraceError = true);
};

// Controls Level Zero calls tracing.
enum DebugLevel {
  ZE_DEBUG_NONE = 0x0,
  ZE_DEBUG_BASIC = 0x1,
  ZE_DEBUG_VALIDATION = 0x2,
  ZE_DEBUG_CALL_COUNT = 0x4,
  ZE_DEBUG_ALL = -1
};

const int ZeDebug = [] {
  const char *DebugMode = std::getenv("ZE_DEBUG");
  return DebugMode ? std::atoi(DebugMode) : ZE_DEBUG_NONE;
}();

// Prints to stderr if ZE_DEBUG allows it
void zePrint(const char *Format, ...);

// This function will ensure compatibility with both Linux and Windows for
// setting environment variables.
bool setEnvVar(const char *name, const char *value);

// Perform traced call to L0 without checking for errors
#define ZE_CALL_NOCHECK(ZeName, ZeArgs)                                        \
  ZeCall().doCall(ZeName ZeArgs, #ZeName, #ZeArgs, false)

struct _ur_platform_handle_t;
// using ur_platform_handle_t = _ur_platform_handle_t *;
struct _ur_device_handle_t;
// using ur_device_handle_t = _ur_device_handle_t *;

struct _ur_platform_handle_t : public _ur_platform {
  _ur_platform_handle_t(ze_driver_handle_t Driver) : ZeDriver{Driver} {}
  // Performs initialization of a newly constructed PI platform.
  ur_result_t initialize();

  // Level Zero lacks the notion of a platform, but there is a driver, which is
  // a pretty good fit to keep here.
  ze_driver_handle_t ZeDriver;

  // Cache versions info from zeDriverGetProperties.
  std::string ZeDriverVersion;
  std::string ZeDriverApiVersion;
  ze_api_version_t ZeApiVersion;

  // Cache driver extensions
  std::unordered_map<std::string, uint32_t> zeDriverExtensionMap;

  // Flags to tell whether various Level Zero platform extensions are available.
  bool ZeDriverGlobalOffsetExtensionFound{false};
  bool ZeDriverModuleProgramExtensionFound{false};

  // Cache UR devices for reuse
  std::vector<std::unique_ptr<ur_device_handle_t_>> PiDevicesCache;
  pi_shared_mutex PiDevicesCacheMutex;
  bool DeviceCachePopulated = false;

  // Check the device cache and load it if necessary.
  ur_result_t populateDeviceCacheIfNeeded();

  // Return the PI device from cache that represents given native device.
  // If not found, then nullptr is returned.
  ur_device_handle_t getDeviceFromNativeHandle(ze_device_handle_t);
};

// TODO: make it into a ur_device_handle_t class member
const std::pair<int, int>
getRangeOfAllowedCopyEngines(const ur_device_handle_t &Device);

class ZeUSMImportExtension {
  // Pointers to functions that import/release host memory into USM
  ze_result_t (*zexDriverImportExternalPointer)(ze_driver_handle_t hDriver,
                                                void *, size_t) = nullptr;
  ze_result_t (*zexDriverReleaseImportedPointer)(ze_driver_handle_t,
                                                 void *) = nullptr;

public:
  // Whether user has requested Import/Release, and platform supports it.
  bool Enabled;

  ZeUSMImportExtension() : Enabled{false} {}

  void setZeUSMImport(_ur_platform_handle_t *Platform);
  void doZeUSMImport(ze_driver_handle_t DriverHandle, void *HostPtr,
                     size_t Size);
  void doZeUSMRelease(ze_driver_handle_t DriverHandle, void *HostPtr);
};

// Helper wrapper for working with USM import extension in Level Zero.
extern ZeUSMImportExtension ZeUSMImport;

// This will count the calls to Level-Zero
extern std::map<const char *, int> *ZeCallCount;

// Some opencl extensions we know are supported by all Level Zero devices.
constexpr char ZE_SUPPORTED_EXTENSIONS[] =
    "cl_khr_il_program cl_khr_subgroups cl_intel_subgroups "
    "cl_intel_subgroups_short cl_intel_required_subgroup_size ";
