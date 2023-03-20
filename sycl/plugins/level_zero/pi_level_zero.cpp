//===-------- pi_level_zero.cpp - Level Zero Plugin --------------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//

/// \file pi_level_zero.cpp
/// Implementation of Level Zero Plugin.
///
/// \ingroup sycl_pi_level_zero

#include "pi_level_zero.hpp"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <sycl/detail/spinlock.hpp>
#include <thread>
#include <utility>

#include <zet_api.h>

#include "../unified_runtime/ur/adapters/level_zero/ur_level_zero_common.hpp"
#include "ur/usm_allocator_config.hpp"
#include "ur_bindings.hpp"

extern "C" {
// Forward declarartions.
// static pi_result piQueueReleaseInternal(pi_queue Queue);
// static pi_result piEventReleaseInternal(pi_event Event);
// static pi_result EventCreate(pi_context Context, pi_queue Queue,
//                              bool HostVisible, pi_event *RetEvent);
}

// Defined in tracing.cpp
void enableZeTracing();
void disableZeTracing();

namespace {
  
// Map from L0 to PI result.
static inline pi_result mapError(ze_result_t Result) {
  return ur2piResult(ze2urResult(Result));
}

// Trace a call to Level-Zero RT
#define ZE_CALL(ZeName, ZeArgs)                                                \
  {                                                                            \
    ze_result_t ZeResult = ZeName ZeArgs;                                      \
    if (auto Result = ZeCall().doCall(ZeResult, #ZeName, #ZeArgs, true))       \
      return mapError(Result);                                                 \
  }

// Trace an internal PI call; returns in case of an error.
#define PI_CALL(Call)                                                          \
  {                                                                            \
    if (PrintTrace)                                                            \
      fprintf(stderr, "PI ---> %s\n", #Call);                                  \
    pi_result Result = (Call);                                                 \
    if (Result != PI_SUCCESS)                                                  \
      return Result;                                                           \
  }

} // anonymous namespace

// Forward declarations

_pi_queue::_pi_queue(std::vector<ze_command_queue_handle_t> &ComputeQueues,
                     std::vector<ze_command_queue_handle_t> &CopyQueues,
                     pi_context Context, pi_device Device,
                     bool OwnZeCommandQueue,
                     pi_queue_properties Properties,
                     int ForceComputeIndex)
    : _ur_queue_handle_t(ComputeQueues,
                     CopyQueues,
                     reinterpret_cast<ur_context_handle_t>(Context),
                     reinterpret_cast<ur_device_handle_t>(Device),
                     OwnZeCommandQueue,
                     Properties,
                     ForceComputeIndex) {
}

extern "C" {

// Forward declarations
decltype(piEventCreate) piEventCreate;

#if 0
static ze_result_t
checkUnresolvedSymbols(ze_module_handle_t ZeModule,
                       ze_module_build_log_handle_t *ZeBuildLog);
#endif

pi_result piPlatformsGet(pi_uint32 NumEntries, pi_platform *Platforms,
                         pi_uint32 *NumPlatforms) {
  return pi2ur::piPlatformsGet(NumEntries, Platforms, NumPlatforms);
}

pi_result piPlatformGetInfo(pi_platform Platform, pi_platform_info ParamName,
                            size_t ParamValueSize, void *ParamValue,
                            size_t *ParamValueSizeRet) {
  zePrint("==========================\n");
  zePrint("SYCL over Level-Zero %s\n", Platform->ZeDriverVersion.c_str());
  zePrint("==========================\n");

  // To distinguish this L0 platform from Unified Runtime one.
  if (ParamName == PI_PLATFORM_INFO_NAME) {
    ReturnHelper ReturnValue(ParamValueSize, ParamValue, ParamValueSizeRet);
    return ReturnValue("Intel(R) Level-Zero");
  }
  return pi2ur::piPlatformGetInfo(Platform, ParamName, ParamValueSize,
                                  ParamValue, ParamValueSizeRet);
}

pi_result piextPlatformGetNativeHandle(pi_platform Platform,
                                       pi_native_handle *NativeHandle) {

  return pi2ur::piextPlatformGetNativeHandle(Platform,
                                             NativeHandle);

#if 0
  PI_ASSERT(Platform, PI_ERROR_INVALID_PLATFORM);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  auto ZeDriver = pi_cast<ze_driver_handle_t *>(NativeHandle);
  // Extract the Level Zero driver handle from the given PI platform
  *ZeDriver = Platform->ZeDriver;
  return PI_SUCCESS;
#endif
}

pi_result piextPlatformCreateWithNativeHandle(pi_native_handle NativeHandle,
                                              pi_platform *Platform) {

  return pi2ur::piextPlatformCreateWithNativeHandle(NativeHandle,
                                                    Platform);
#if 0
  PI_ASSERT(Platform, PI_ERROR_INVALID_PLATFORM);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  auto ZeDriver = pi_cast<ze_driver_handle_t>(NativeHandle);

  pi_uint32 NumPlatforms = 0;
  pi_result Res = piPlatformsGet(0, nullptr, &NumPlatforms);
  if (Res != PI_SUCCESS) {
    return Res;
  }

  if (NumPlatforms) {
    std::vector<pi_platform> Platforms(NumPlatforms);
    PI_CALL(piPlatformsGet(NumPlatforms, Platforms.data(), nullptr));

    // The SYCL spec requires that the set of platforms must remain fixed for
    // the duration of the application's execution. We assume that we found all
    // of the Level Zero drivers when we initialized the platform cache, so the
    // "NativeHandle" must already be in the cache. If it is not, this must not
    // be a valid Level Zero driver.
    for (const pi_platform &CachedPlatform : Platforms) {
      if (CachedPlatform->ZeDriver == ZeDriver) {
        *Platform = CachedPlatform;
        return PI_SUCCESS;
      }
    }
  }

  return PI_ERROR_INVALID_VALUE;
#endif
}

pi_result piPluginGetLastError(char **message) {
  return pi2ur::piPluginGetLastError(message);
}

pi_result piDevicesGet(pi_platform Platform, pi_device_type DeviceType,
                       pi_uint32 NumEntries, pi_device *Devices,
                       pi_uint32 *NumDevices) {
  return pi2ur::piDevicesGet(Platform, DeviceType, NumEntries, Devices,
                             NumDevices);
}

pi_result piDeviceRetain(pi_device Device) {
  return pi2ur::piDeviceRetain(Device);
}

pi_result piDeviceRelease(pi_device Device) {
  return pi2ur::piDeviceRelease(Device);
}

pi_result piDeviceGetInfo(pi_device Device, pi_device_info ParamName,
                          size_t ParamValueSize, void *ParamValue,
                          size_t *ParamValueSizeRet) {
  return pi2ur::piDeviceGetInfo(Device, ParamName, ParamValueSize, ParamValue,
                                ParamValueSizeRet);
}

pi_result piDevicePartition(pi_device Device,
                            const pi_device_partition_property *Properties,
                            pi_uint32 NumDevices, pi_device *OutDevices,
                            pi_uint32 *OutNumDevices) {
  return pi2ur::piDevicePartition(Device, Properties, NumDevices, OutDevices,
                                  OutNumDevices);
}

pi_result
piextDeviceSelectBinary(pi_device Device, // TODO: does this need to be context?
                        pi_device_binary *Binaries, pi_uint32 NumBinaries,
                        pi_uint32 *SelectedBinaryInd) {
  return pi2ur::piextDeviceSelectBinary(Device, Binaries,
                                        NumBinaries, SelectedBinaryInd);
}

pi_result piextDeviceGetNativeHandle(pi_device Device,
                                     pi_native_handle *NativeHandle) {

  return pi2ur::piextDeviceGetNativeHandle(Device,
                                           NativeHandle);
}

pi_result piextDeviceCreateWithNativeHandle(pi_native_handle NativeHandle,
                                            pi_platform Platform,
                                            pi_device *Device) {

  return pi2ur::piextDeviceCreateWithNativeHandle(NativeHandle,
                                                  Platform,
                                                  Device);

#if 0
  PI_ASSERT(Device, PI_ERROR_INVALID_DEVICE);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  auto ZeDevice = pi_cast<ze_device_handle_t>(NativeHandle);

  // The SYCL spec requires that the set of devices must remain fixed for the
  // duration of the application's execution. We assume that we found all of the
  // Level Zero devices when we initialized the platforms/devices cache, so the
  // "NativeHandle" must already be in the cache. If it is not, this must not be
  // a valid Level Zero device.
  //
  // TODO: maybe we should populate cache of platforms if it wasn't already.
  // For now assert that is was populated.
  PI_ASSERT(PiPlatformCachePopulated, PI_ERROR_INVALID_VALUE);
  const std::lock_guard<SpinLock> Lock{*PiPlatformsCacheMutex};

  pi_device Dev = nullptr;
  for (pi_platform ThePlatform : *PiPlatformsCache) {
    Dev = ThePlatform->getDeviceFromNativeHandle(ZeDevice);
    if (Dev) {
      // Check that the input Platform, if was given, matches the found one.
      PI_ASSERT(!Platform || Platform == ThePlatform,
                PI_ERROR_INVALID_PLATFORM);
      break;
    }
  }

  if (Dev == nullptr)
    return PI_ERROR_INVALID_VALUE;

  *Device = Dev;
  return PI_SUCCESS;
#endif
}

pi_result piContextCreate(const pi_context_properties *Properties,
                          pi_uint32 NumDevices, const pi_device *Devices,
                          void (*PFnNotify)(const char *ErrInfo,
                                            const void *PrivateInfo, size_t CB,
                                            void *UserData),
                          void *UserData, pi_context *RetContext) {
  return pi2ur::piContextCreate(Properties,
                                NumDevices,
                                Devices,
                                PFnNotify,
                                UserData,
                                RetContext);
#if 0
  (void)Properties;
  (void)PFnNotify;
  (void)UserData;

  PI_ASSERT(NumDevices, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Devices, PI_ERROR_INVALID_DEVICE);
  PI_ASSERT(RetContext, PI_ERROR_INVALID_VALUE);

  pi_platform Platform = (*Devices)->Platform;
  ZeStruct<ze_context_desc_t> ContextDesc;
  ContextDesc.flags = 0;

  ze_context_handle_t ZeContext;
  ZE_CALL(zeContextCreate, (Platform->ZeDriver, &ContextDesc, &ZeContext));
  try {
    *RetContext = new _pi_context(ZeContext, NumDevices, Devices, true);
    (*RetContext)->initialize();
    if (IndirectAccessTrackingEnabled) {
      std::scoped_lock<pi_shared_mutex> Lock(Platform->ContextsMutex);
      Platform->Contexts.push_back(reinterpret_cast<ur_context_handle_t>(*RetContext));
    }
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
#endif
}

pi_result piContextGetInfo(pi_context Context, pi_context_info ParamName,
                           size_t ParamValueSize, void *ParamValue,
                           size_t *ParamValueSizeRet) {

  return pi2ur::piContextGetInfo(Context,
                                 ParamName,
                                 ParamValueSize,
                                 ParamValue,
                                 ParamValueSizeRet);
#if 0
  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);

  std::shared_lock<pi_shared_mutex> Lock(Context->Mutex);
  ReturnHelper ReturnValue(ParamValueSize, ParamValue, ParamValueSizeRet);
  switch (ParamName) {
  case PI_CONTEXT_INFO_DEVICES:
    return ReturnValue(&Context->Devices[0], Context->Devices.size());
  case PI_CONTEXT_INFO_NUM_DEVICES:
    return ReturnValue(pi_uint32(Context->Devices.size()));
  case PI_CONTEXT_INFO_REFERENCE_COUNT:
    return ReturnValue(pi_uint32{Context->RefCount.load()});
  case PI_EXT_ONEAPI_CONTEXT_INFO_USM_MEMCPY2D_SUPPORT:
    // 2D USM memcpy is supported.
    return ReturnValue(pi_bool{true});
  case PI_EXT_ONEAPI_CONTEXT_INFO_USM_FILL2D_SUPPORT:
  case PI_EXT_ONEAPI_CONTEXT_INFO_USM_MEMSET2D_SUPPORT:
    // 2D USM fill and memset is not supported.
    return ReturnValue(pi_bool{false});
  case PI_CONTEXT_INFO_ATOMIC_MEMORY_SCOPE_CAPABILITIES:
  default:
    // TODO: implement other parameters
    die("piGetContextInfo: unsuppported ParamName.");
  }

  return PI_SUCCESS;
#endif
}

// FIXME: Dummy implementation to prevent link fail
pi_result piextContextSetExtendedDeleter(pi_context Context,
                                         pi_context_extended_deleter Function,
                                         void *UserData) {
  return pi2ur::piextContextSetExtendedDeleter(Context,
                                               Function,
                                               UserData);
}

pi_result piextContextGetNativeHandle(pi_context Context,
                                      pi_native_handle *NativeHandle) {
  return pi2ur::piextContextGetNativeHandle(Context,
                                            NativeHandle);
}

pi_result piextContextCreateWithNativeHandle(pi_native_handle NativeHandle,
                                             pi_uint32 NumDevices,
                                             const pi_device *Devices,
                                             bool OwnNativeHandle,
                                             pi_context *RetContext) {
  return pi2ur::piextContextCreateWithNativeHandle(NativeHandle,
                                                   NumDevices,
                                                   Devices,
                                                   OwnNativeHandle,
                                                   RetContext);
}

pi_result piContextRetain(pi_context Context) {

  return pi2ur::piContextRetain(Context);
}

pi_result piContextRelease(pi_context Context) {
  return pi2ur::piContextRelease(Context);
}

pi_result piQueueCreate(pi_context Context, pi_device Device,
                        pi_queue_properties Flags, pi_queue *Queue) {
  pi_queue_properties Properties[] = {PI_QUEUE_FLAGS, Flags, 0};
  return piextQueueCreate(Context, Device, Properties, Queue);
}

pi_result piextQueueCreate(pi_context Context, pi_device Device,
                           pi_queue_properties *Properties, pi_queue *Queue) {
  return pi2ur::piextQueueCreate(Context, Device, Properties, Queue);
}

pi_result piQueueGetInfo(pi_queue Queue, pi_queue_info ParamName,
                         size_t ParamValueSize, void *ParamValue,
                         size_t *ParamValueSizeRet) {

  return pi2ur::piQueueGetInfo(Queue,
                               ParamName,
                               ParamValueSize,
                               ParamValue,
                               ParamValueSizeRet);
}

pi_result piQueueRetain(pi_queue Queue) {

  return pi2ur::piQueueRetain(Queue);
}

pi_result piQueueRelease(pi_queue Queue) {
  return pi2ur::piQueueRelease(Queue);
}

pi_result piQueueFinish(pi_queue Queue) {
  return pi2ur::piQueueFinish(Queue);
}

pi_result piQueueFlush(pi_queue Queue) {
  return pi2ur::piQueueFlush(Queue);
}

pi_result piextQueueGetNativeHandle(pi_queue Queue,
                                    pi_native_handle *NativeHandle) {

  return pi2ur::piextQueueGetNativeHandle(Queue,
                                          NativeHandle);
}

pi_result piextQueueCreateWithNativeHandle(pi_native_handle NativeHandle,
                                           pi_context Context, pi_device Device,
                                           bool OwnNativeHandle,
                                           pi_queue *Queue) {

  return pi2ur::piextQueueCreateWithNativeHandle(NativeHandle,
                                                 Context,
                                                 Device,
                                                 OwnNativeHandle,
                                                 Queue);
}

pi_result piMemBufferCreate(pi_context Context, pi_mem_flags Flags, size_t Size,
                            void *HostPtr, pi_mem *RetMem,
                            const pi_mem_properties *properties) {
  return pi2ur::piMemBufferCreate(Context,
                                  Flags,
                                  Size,
                                  HostPtr,
                                  RetMem,
                                  properties);
}

pi_result piMemGetInfo(pi_mem Mem, pi_mem_info ParamName, size_t ParamValueSize,
                       void *ParamValue, size_t *ParamValueSizeRet) {
  return pi2ur::piMemGetInfo(Mem,
                             ParamName,
                             ParamValueSize,
                             ParamValue,
                             ParamValueSizeRet);
}

pi_result piMemRetain(pi_mem Mem) {
  return pi2ur::piMemRetain(Mem);
}

pi_result piMemRelease(pi_mem Mem) {
  return pi2ur::piMemRelease(Mem);
}

pi_result piMemImageCreate(pi_context Context, pi_mem_flags Flags,
                           const pi_image_format *ImageFormat,
                           const pi_image_desc *ImageDesc, void *HostPtr,
                           pi_mem *RetImage) {

  return pi2ur::piMemImageCreate(Context,
                                 Flags,
                                 ImageFormat,
                                 ImageDesc,
                                 HostPtr,
                                 RetImage);
}

pi_result piextMemGetNativeHandle(pi_mem Mem, pi_native_handle *NativeHandle) {
  return pi2ur::piextMemGetNativeHandle(Mem,
                                        NativeHandle);
}

pi_result piextMemCreateWithNativeHandle(pi_native_handle NativeHandle,
                                         pi_context Context,
                                         bool ownNativeHandle, pi_mem *Mem) {
  return pi2ur::piextMemCreateWithNativeHandle(NativeHandle,
                                               Context,
                                               ownNativeHandle,
                                               Mem);
}

pi_result piProgramCreate(pi_context Context, const void *ILBytes,
                          size_t Length, pi_program *Program) {
  return pi2ur::piProgramCreate(Context, ILBytes,
                                Length, Program);
}

pi_result piProgramCreateWithBinary(
    pi_context Context, pi_uint32 NumDevices, const pi_device *DeviceList,
    const size_t *Lengths, const unsigned char **Binaries,
    size_t NumMetadataEntries, const pi_device_binary_property *Metadata,
    pi_int32 *BinaryStatus, pi_program *Program) {

  return pi2ur::piProgramCreateWithBinary(Context,
                                          NumDevices,
                                          DeviceList,
                                          Lengths,
                                          Binaries,
                                          NumMetadataEntries,
                                          Metadata,
                                          BinaryStatus,
                                          Program);
}

pi_result piclProgramCreateWithSource(pi_context Context, pi_uint32 Count,
                                      const char **Strings,
                                      const size_t *Lengths,
                                      pi_program *RetProgram) {
  return pi2ur::piclProgramCreateWithSource(Context,
                                            Count,
                                            Strings,
                                            Lengths,
                                            RetProgram);
}

pi_result piProgramGetInfo(pi_program Program, pi_program_info ParamName,
                           size_t ParamValueSize, void *ParamValue,
                           size_t *ParamValueSizeRet) {

  return pi2ur::piProgramGetInfo(Program,
                                 ParamName,
                                 ParamValueSize,
                                 ParamValue,
                                 ParamValueSizeRet);
}

pi_result piProgramLink(pi_context Context, pi_uint32 NumDevices,
                        const pi_device *DeviceList, const char *Options,
                        pi_uint32 NumInputPrograms,
                        const pi_program *InputPrograms,
                        void (*PFnNotify)(pi_program Program, void *UserData),
                        void *UserData, pi_program *RetProgram) {
    return pi2ur::piProgramLink(Context,
                              NumDevices,
                              DeviceList,
                              Options,
                              NumInputPrograms,
                              InputPrograms,
                              PFnNotify,
                              UserData,
                              RetProgram);
}

pi_result piProgramCompile(
    pi_program Program, pi_uint32 NumDevices, const pi_device *DeviceList,
    const char *Options, pi_uint32 NumInputHeaders,
    const pi_program *InputHeaders, const char **HeaderIncludeNames,
    void (*PFnNotify)(pi_program Program, void *UserData), void *UserData) {

  return pi2ur::piProgramCompile(Program,
                                 NumDevices,
                                 DeviceList,
                                 Options,
                                 NumInputHeaders,
                                 InputHeaders,
                                 HeaderIncludeNames,
                                 PFnNotify, UserData);
}

pi_result piProgramBuild(pi_program Program, pi_uint32 NumDevices,
                         const pi_device *DeviceList, const char *Options,
                         void (*PFnNotify)(pi_program Program, void *UserData),
                         void *UserData) {
  return pi2ur::piProgramBuild(Program,
                              NumDevices,
                              DeviceList,
                              Options,
                              PFnNotify,
                              UserData);
}

pi_result piProgramGetBuildInfo(pi_program Program, pi_device Device,
                                pi_program_build_info ParamName,
                                size_t ParamValueSize, void *ParamValue,
                                size_t *ParamValueSizeRet) {
  
  return pi2ur::piProgramGetBuildInfo(Program,
                                      Device,
                                      ParamName,
                                      ParamValueSize,
                                      ParamValue,
                                      ParamValueSizeRet);
}

pi_result piProgramRetain(pi_program Program) {
  return pi2ur::piProgramRetain(Program);
}

pi_result piProgramRelease(pi_program Program) {
  return pi2ur::piProgramRelease(Program);
}

pi_result piextProgramGetNativeHandle(pi_program Program,
                                      pi_native_handle *NativeHandle) {
  return pi2ur::piextProgramGetNativeHandle(Program,
                                            NativeHandle);
}

pi_result piextProgramCreateWithNativeHandle(pi_native_handle NativeHandle,
                                             pi_context Context,
                                             bool ownNativeHandle,
                                             pi_program *Program) {
  return pi2ur::piextProgramCreateWithNativeHandle(NativeHandle,
                                                   Context,
                                                   ownNativeHandle,
                                                   Program);
}

pi_result piKernelCreate(pi_program Program, const char *KernelName,
                         pi_kernel *RetKernel) {

  return pi2ur::piKernelCreate(Program,
                               KernelName,
                               RetKernel);
}

pi_result piKernelSetArg(pi_kernel Kernel, pi_uint32 ArgIndex, size_t ArgSize,
                         const void *ArgValue) {

  return pi2ur::piKernelSetArg(Kernel,
                               ArgIndex,
                               ArgSize,
                               ArgValue);
}

// Special version of piKernelSetArg to accept pi_mem.
pi_result piextKernelSetArgMemObj(pi_kernel Kernel, pi_uint32 ArgIndex,
                                  const pi_mem *ArgValue) {

  return pi2ur::piextKernelSetArgMemObj(Kernel,
                                        ArgIndex,
                                        ArgValue);
}

// Special version of piKernelSetArg to accept pi_sampler.
pi_result piextKernelSetArgSampler(pi_kernel Kernel, pi_uint32 ArgIndex,
                                   const pi_sampler *ArgValue) {

  return pi2ur::piextKernelSetArgSampler(Kernel,
                                         ArgIndex,
                                         ArgValue);
}

pi_result piKernelGetInfo(pi_kernel Kernel, pi_kernel_info ParamName,
                          size_t ParamValueSize, void *ParamValue,
                          size_t *ParamValueSizeRet) {

  return pi2ur::piKernelGetInfo(Kernel,
                                ParamName,
                                ParamValueSize,
                                ParamValue,
                                ParamValueSizeRet);
}

pi_result piKernelGetGroupInfo(pi_kernel Kernel, pi_device Device,
                               pi_kernel_group_info ParamName,
                               size_t ParamValueSize, void *ParamValue,
                               size_t *ParamValueSizeRet) {

  return pi2ur::piKernelGetGroupInfo(Kernel,
                                     Device,
                                     ParamName,
                                     ParamValueSize,
                                     ParamValue,
                                     ParamValueSizeRet);
}

pi_result piKernelGetSubGroupInfo(pi_kernel Kernel, pi_device Device,
                                  pi_kernel_sub_group_info ParamName,
                                  size_t InputValueSize, const void *InputValue,
                                  size_t ParamValueSize, void *ParamValue,
                                  size_t *ParamValueSizeRet) {

  return pi2ur::piKernelGetSubGroupInfo(Kernel,
                                        Device,
                                        ParamName,
                                        InputValueSize,
                                        InputValue,
                                        ParamValueSize,
                                        ParamValue,
                                        ParamValueSizeRet);
}

pi_result piKernelRetain(pi_kernel Kernel) {

  return pi2ur::piKernelRetain(Kernel);
}

pi_result piKernelRelease(pi_kernel Kernel) {

  return pi2ur::piKernelRelease(Kernel);
}

pi_result
piEnqueueKernelLaunch(pi_queue Queue, pi_kernel Kernel, pi_uint32 WorkDim,
                      const size_t *GlobalWorkOffset,
                      const size_t *GlobalWorkSize, const size_t *LocalWorkSize,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventWaitList, pi_event *OutEvent) {
  return pi2ur::piEnqueueKernelLaunch(Queue,
                                      Kernel,
                                      WorkDim,
                                      GlobalWorkOffset,
                                      GlobalWorkSize,
                                      LocalWorkSize,
                                      NumEventsInWaitList,
                                      EventWaitList,
                                      OutEvent);
}

pi_result piextKernelCreateWithNativeHandle(pi_native_handle NativeHandle,
                                            pi_context Context,
                                            pi_program Program,
                                            bool OwnNativeHandle,
                                            pi_kernel *Kernel) {

  return pi2ur::piextKernelCreateWithNativeHandle(NativeHandle,
                                                  Context,
                                                  Program,
                                                  OwnNativeHandle,
                                                  Kernel);
}

pi_result piextKernelGetNativeHandle(pi_kernel Kernel,
                                     pi_native_handle *NativeHandle) {
  return pi2ur::piextKernelGetNativeHandle(Kernel,
                                           NativeHandle);
}

//
// Events
//

// External PI API entry
pi_result piEventCreate(pi_context Context, pi_event *RetEvent) {
  return pi2ur::piEventCreate(Context,
                              RetEvent);
}

pi_result piEventGetInfo(pi_event Event, pi_event_info ParamName,
                         size_t ParamValueSize, void *ParamValue,
                         size_t *ParamValueSizeRet) {
  return pi2ur::piEventGetInfo(Event,
                               ParamName,
                               ParamValueSize,
                               ParamValue,
                               ParamValueSizeRet);
}

pi_result piEventGetProfilingInfo(pi_event Event, pi_profiling_info ParamName,
                                  size_t ParamValueSize, void *ParamValue,
                                  size_t *ParamValueSizeRet) {

  return pi2ur::piEventGetProfilingInfo(Event,
                                        ParamName,
                                        ParamValueSize,
                                        ParamValue,
                                        ParamValueSizeRet);
}

} // extern "C"


extern "C" {

pi_result piEventsWait(pi_uint32 NumEvents, const pi_event *EventList) {
  return pi2ur::piEventsWait(NumEvents,
                             EventList);
}

pi_result piEventSetCallback(pi_event Event, pi_int32 CommandExecCallbackType,
                             void (*PFnNotify)(pi_event Event,
                                               pi_int32 EventCommandStatus,
                                               void *UserData),
                             void *UserData) {
  return pi2ur::piEventSetCallback(Event,
                                   CommandExecCallbackType,
                                   PFnNotify,
                                   UserData);
}

pi_result piEventSetStatus(pi_event Event, pi_int32 ExecutionStatus) {
  return pi2ur::piEventSetStatus(Event,
                                 ExecutionStatus);
}

pi_result piEventRetain(pi_event Event) {
  return pi2ur::piEventRetain(Event);
}

pi_result piEventRelease(pi_event Event) {
  return pi2ur::piEventRelease(Event);
}

pi_result piextEventGetNativeHandle(pi_event Event,
                                    pi_native_handle *NativeHandle) {

  return pi2ur::piextEventGetNativeHandle(Event,
                                          NativeHandle);
}

pi_result piextEventCreateWithNativeHandle(pi_native_handle NativeHandle,
                                           pi_context Context,
                                           bool OwnNativeHandle,
                                           pi_event *Event) {
  return pi2ur::piextEventCreateWithNativeHandle(NativeHandle,
                                                 Context,
                                                 OwnNativeHandle,
                                                 Event);
}

//
// Sampler
//
pi_result piSamplerCreate(pi_context Context,
                          const pi_sampler_properties *SamplerProperties,
                          pi_sampler *RetSampler) {
  return pi2ur::piSamplerCreate(Context,
                                SamplerProperties,
                                RetSampler);
}

pi_result piSamplerGetInfo(pi_sampler Sampler, pi_sampler_info ParamName,
                           size_t ParamValueSize, void *ParamValue,
                           size_t *ParamValueSizeRet) {

  return pi2ur::piSamplerGetInfo(Sampler,
                                 ParamName,
                                 ParamValueSize,
                                 ParamValue,
                                 ParamValueSizeRet);
}

pi_result piSamplerRetain(pi_sampler Sampler) {
  return pi2ur::piSamplerRetain(Sampler);
}

pi_result piSamplerRelease(pi_sampler Sampler) {
  return pi2ur::piSamplerRelease(Sampler);
}

//
// Queue Commands
//
pi_result piEnqueueEventsWait(pi_queue Queue, pi_uint32 NumEventsInWaitList,
                              const pi_event *EventWaitList,
                              pi_event *OutEvent) {

  return pi2ur::piEnqueueEventsWait(Queue,
                                    NumEventsInWaitList,
                                    EventWaitList,
                                    OutEvent);
}

pi_result piEnqueueEventsWaitWithBarrier(pi_queue Queue,
                                         pi_uint32 NumEventsInWaitList,
                                         const pi_event *EventWaitList,
                                         pi_event *OutEvent) {
  
  return pi2ur::piEnqueueEventsWaitWithBarrier(Queue,
                                               NumEventsInWaitList,
                                               EventWaitList,
                                               OutEvent);
#if 0
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  // Lock automatically releases when this goes out of scope.
  std::scoped_lock<pi_shared_mutex> lock(Queue->Mutex);

  // Helper function for appending a barrier to a command list.
  auto insertBarrierIntoCmdList =
      [&Queue](pi_command_list_ptr_t CmdList,
               const _pi_ze_event_list_t &EventWaitList, pi_event &Event,
               bool IsInternal) {
        if (auto Res = ur2piResult(createEventAndAssociateQueue(reinterpret_cast<ur_queue_handle_t>(Queue),
                                                                reinterpret_cast<ur_event_handle_t *>(&Event),
                                                                PI_COMMAND_TYPE_USER,
                                                                CmdList,
                                                                IsInternal)))
          return Res;

        Event->WaitList = EventWaitList;
        ZE_CALL(zeCommandListAppendBarrier,
                (CmdList->first, Event->ZeEvent, EventWaitList.Length,
                 EventWaitList.ZeEventList));
        return PI_SUCCESS;
      };

  pi_event InternalEvent;
  bool IsInternal = OutEvent == nullptr;
  pi_event *Event = OutEvent ? OutEvent : &InternalEvent;

  // Indicator for whether batching is allowed. This may be changed later in
  // this function, but allow it by default.
  bool OkToBatch = true;

  // If we have a list of events to make the barrier from, then we can create a
  // barrier on these and use the resulting event as our future barrier.
  // We use the same approach if
  // SYCL_PI_LEVEL_ZERO_USE_MULTIPLE_COMMANDLIST_BARRIERS is not set to a
  // positive value.
  // We use the same approach if we have in-order queue because every command
  // depends on previous one, so we don't need to insert barrier to multiple
  // command lists.
  if (NumEventsInWaitList || !UseMultipleCmdlistBarriers ||
      Queue->isInOrderQueue()) {
    // Retain the events as they will be owned by the result event.
    _pi_ze_event_list_t TmpWaitList;
    if (auto Res = ur2piResult(TmpWaitList.createAndRetainPiZeEventList(
            NumEventsInWaitList, reinterpret_cast<const ur_event_handle_t *>(EventWaitList), reinterpret_cast<ur_queue_handle_t>(Queue),
            /*UseCopyEngine=*/false)))
      return Res;

    // Get an arbitrary command-list in the queue.
    pi_command_list_ptr_t CmdList;
    if (auto Res = ur2piResult(Queue->Context->getAvailableCommandList(
            reinterpret_cast<ur_queue_handle_t>(Queue), CmdList,
            /*UseCopyEngine=*/false, OkToBatch)))
      return Res;

    // Insert the barrier into the command-list and execute.
    if (auto Res =
            insertBarrierIntoCmdList(CmdList, TmpWaitList, *Event, IsInternal))
      return Res;

    if (auto Res = ur2piResult(Queue->executeCommandList(CmdList, false, OkToBatch)))
      return Res;

    // Because of the dependency between commands in the in-order queue we don't
    // need to keep track of any active barriers if we have in-order queue.
    if (UseMultipleCmdlistBarriers && !Queue->isInOrderQueue()) {
      auto UREvent = reinterpret_cast<ur_event_handle_t>(*Event);
      Queue->ActiveBarriers.add(UREvent);
    }
    return PI_SUCCESS;
  }

  // Since there are no events to explicitly create a barrier for, we are
  // inserting a queue-wide barrier.

  // Command list(s) for putting barriers.
  std::vector<pi_command_list_ptr_t> CmdLists;

  // There must be at least one L0 queue.
  auto &InitialComputeGroup = Queue->ComputeQueueGroupsByTID.begin()->second;
  auto &InitialCopyGroup = Queue->CopyQueueGroupsByTID.begin()->second;
  PI_ASSERT(!InitialComputeGroup.ZeQueues.empty() ||
                !InitialCopyGroup.ZeQueues.empty(),
            PI_ERROR_INVALID_QUEUE);

  size_t NumQueues = 0;
  for (auto &QueueMap :
       {Queue->ComputeQueueGroupsByTID, Queue->CopyQueueGroupsByTID})
    for (auto &QueueGroup : QueueMap)
      NumQueues += QueueGroup.second.ZeQueues.size();

  OkToBatch = true;
  // Get an available command list tied to each command queue. We need
  // these so a queue-wide barrier can be inserted into each command
  // queue.
  CmdLists.reserve(NumQueues);
  for (auto &QueueMap :
       {Queue->ComputeQueueGroupsByTID, Queue->CopyQueueGroupsByTID})
    for (auto &QueueGroup : QueueMap) {
      bool UseCopyEngine =
          QueueGroup.second.Type != _pi_queue::queue_type::Compute;
      if (Queue->Device->useImmediateCommandLists()) {
        // If immediate command lists are being used, each will act as their own
        // queue, so we must insert a barrier into each.
        for (auto ImmCmdList : QueueGroup.second.ImmCmdLists)
          if (ImmCmdList != Queue->CommandListMap.end())
            CmdLists.push_back(ImmCmdList);
      } else {
        for (auto ZeQueue : QueueGroup.second.ZeQueues) {
          if (ZeQueue) {
            pi_command_list_ptr_t CmdList;
            if (auto Res = ur2piResult(Queue->Context->getAvailableCommandList(
                    reinterpret_cast<ur_queue_handle_t>(Queue), CmdList, UseCopyEngine, OkToBatch, &ZeQueue)))
              return Res;
            CmdLists.push_back(CmdList);
          }
        }
      }
    }

  // If no activity has occurred on the queue then there will be no cmdlists.
  // We need one for generating an Event, so create one.
  if (CmdLists.size() == 0) {
    // Get any available command list.
    pi_command_list_ptr_t CmdList;
    if (auto Res = ur2piResult(Queue->Context->getAvailableCommandList(
            reinterpret_cast<ur_queue_handle_t>(Queue), CmdList,
            /*UseCopyEngine=*/false, OkToBatch)))
      return Res;
    CmdLists.push_back(CmdList);
  }

  if (CmdLists.size() > 1) {
    // Insert a barrier into each unique command queue using the available
    // command-lists.
    std::vector<pi_event> EventWaitVector(CmdLists.size());
    for (size_t I = 0; I < CmdLists.size(); ++I) {
      if (auto Res =
              insertBarrierIntoCmdList(CmdLists[I], _pi_ze_event_list_t{},
                                       EventWaitVector[I], /*IsInternal*/ true))
        return Res;
    }
    // If there were multiple queues we need to create a "convergence" event to
    // be our active barrier. This convergence event is signalled by a barrier
    // on all the events from the barriers we have inserted into each queue.
    // Use the first command list as our convergence command list.
    pi_command_list_ptr_t &ConvergenceCmdList = CmdLists[0];

    // Create an event list. It will take ownership over all relevant events so
    // we relinquish ownership and let it keep all events it needs.
    _pi_ze_event_list_t BaseWaitList;
    if (auto Res = ur2piResult(BaseWaitList.createAndRetainPiZeEventList(
            EventWaitVector.size(), reinterpret_cast<const ur_event_handle_t *>(EventWaitVector.data()), reinterpret_cast<ur_queue_handle_t>(Queue),
            ConvergenceCmdList->second.isCopy(reinterpret_cast<ur_queue_handle_t>(Queue)))))
      return Res;

    // Insert a barrier with the events from each command-queue into the
    // convergence command list. The resulting event signals the convergence of
    // all barriers.
    if (auto Res = insertBarrierIntoCmdList(ConvergenceCmdList, BaseWaitList,
                                            *Event, IsInternal))
      return Res;
  } else {
    // If there is only a single queue then insert a barrier and the single
    // result event can be used as our active barrier and used as the return
    // event. Take into account whether output event is discarded or not.
    if (auto Res = insertBarrierIntoCmdList(CmdLists[0], _pi_ze_event_list_t{},
                                            *Event, IsInternal))
      return Res;
  }

  // Execute each command list so the barriers can be encountered.
  for (pi_command_list_ptr_t &CmdList : CmdLists)
    if (auto Res = ur2piResult(Queue->executeCommandList(CmdList, false, OkToBatch)))
      return Res;

  if (auto Res = ur2piResult(Queue->ActiveBarriers.clear()))
    return Res;
  auto UREvent = reinterpret_cast<ur_event_handle_t>(*Event);
  Queue->ActiveBarriers.add(UREvent);
  return PI_SUCCESS;
#endif
}

pi_result piEnqueueMemBufferRead(pi_queue Queue, pi_mem Src,
                                 pi_bool BlockingRead, size_t Offset,
                                 size_t Size, void *Dst,
                                 pi_uint32 NumEventsInWaitList,
                                 const pi_event *EventWaitList,
                                 pi_event *Event) {
  
  return pi2ur::piEnqueueMemBufferRead(Queue,
                                       Src,
                                       BlockingRead,
                                       Offset,
                                       Size,
                                       Dst,
                                       NumEventsInWaitList,
                                       EventWaitList,
                                       Event);

#if 0
  PI_ASSERT(Src, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  std::shared_lock<pi_shared_mutex> SrcLock(Src->Mutex, std::defer_lock);
  std::scoped_lock<std::shared_lock<pi_shared_mutex>, pi_shared_mutex> LockAll(
      SrcLock, Queue->Mutex);

  char *ZeHandleSrc;
  PI_CALL(Src->getZeHandle(ZeHandleSrc, _pi_mem::read_only, Queue->Device));
  return enqueueMemCopyHelper(PI_COMMAND_TYPE_MEM_BUFFER_READ, Queue, Dst,
                              BlockingRead, Size, ZeHandleSrc + Offset,
                              NumEventsInWaitList, EventWaitList, Event,
                              /* PreferCopyEngine */ true);
#endif
}

pi_result piEnqueueMemBufferReadRect(
    pi_queue Queue, pi_mem Buffer, pi_bool BlockingRead,
    pi_buff_rect_offset BufferOffset, pi_buff_rect_offset HostOffset,
    pi_buff_rect_region Region, size_t BufferRowPitch, size_t BufferSlicePitch,
    size_t HostRowPitch, size_t HostSlicePitch, void *Ptr,
    pi_uint32 NumEventsInWaitList, const pi_event *EventWaitList,
    pi_event *Event) {

  return pi2ur::piEnqueueMemBufferReadRect(Queue,
                                           Buffer,
                                           BlockingRead,
                                           BufferOffset,
                                           HostOffset,
                                           Region,
                                           BufferRowPitch,
                                           BufferSlicePitch,
                                           HostRowPitch,
                                           HostSlicePitch,
                                           Ptr,
                                           NumEventsInWaitList,
                                           EventWaitList,
                                           Event);

#if 0
  PI_ASSERT(Buffer, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  std::shared_lock<pi_shared_mutex> SrcLock(Buffer->Mutex, std::defer_lock);
  std::scoped_lock<std::shared_lock<pi_shared_mutex>, pi_shared_mutex> LockAll(
      SrcLock, Queue->Mutex);

  char *ZeHandleSrc;
  PI_CALL(Buffer->getZeHandle(ZeHandleSrc, _pi_mem::read_only, Queue->Device));
  return enqueueMemCopyRectHelper(
      PI_COMMAND_TYPE_MEM_BUFFER_READ_RECT, Queue, ZeHandleSrc,
      static_cast<char *>(Ptr), BufferOffset, HostOffset, Region,
      BufferRowPitch, HostRowPitch, BufferSlicePitch, HostSlicePitch,
      BlockingRead, NumEventsInWaitList, EventWaitList, Event);
#endif
}

} // extern "C"

extern "C" {

pi_result piEnqueueMemBufferWrite(pi_queue Queue, pi_mem Buffer,
                                  pi_bool BlockingWrite, size_t Offset,
                                  size_t Size, const void *Ptr,
                                  pi_uint32 NumEventsInWaitList,
                                  const pi_event *EventWaitList,
                                  pi_event *Event) {

  return pi2ur::piEnqueueMemBufferWrite(Queue,
                                        Buffer,
                                        BlockingWrite,
                                        Offset,
                                        Size,
                                        Ptr,
                                        NumEventsInWaitList,
                                        EventWaitList,
                                        Event);
#if 0 
  PI_ASSERT(Buffer, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(Queue->Mutex,
                                                          Buffer->Mutex);

  char *ZeHandleDst;
  PI_CALL(Buffer->getZeHandle(ZeHandleDst, _pi_mem::write_only, Queue->Device));
  return enqueueMemCopyHelper(PI_COMMAND_TYPE_MEM_BUFFER_WRITE, Queue,
                              ZeHandleDst + Offset, // dst
                              BlockingWrite, Size,
                              Ptr, // src
                              NumEventsInWaitList, EventWaitList, Event,
                              /* PreferCopyEngine */ true);
#endif
}

pi_result piEnqueueMemBufferWriteRect(
    pi_queue Queue, pi_mem Buffer, pi_bool BlockingWrite,
    pi_buff_rect_offset BufferOffset, pi_buff_rect_offset HostOffset,
    pi_buff_rect_region Region, size_t BufferRowPitch, size_t BufferSlicePitch,
    size_t HostRowPitch, size_t HostSlicePitch, const void *Ptr,
    pi_uint32 NumEventsInWaitList, const pi_event *EventWaitList,
    pi_event *Event) {

  return pi2ur::piEnqueueMemBufferWriteRect(Queue,
                                            Buffer,
                                            BlockingWrite,
                                            BufferOffset,
                                            HostOffset,
                                            Region,
                                            BufferRowPitch,
                                            BufferSlicePitch,
                                            HostRowPitch,
                                            HostSlicePitch,
                                            Ptr,
                                            NumEventsInWaitList,
                                            EventWaitList,
                                            Event);
#if 0
  PI_ASSERT(Buffer, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(Queue->Mutex,
                                                          Buffer->Mutex);

  char *ZeHandleDst;
  PI_CALL(Buffer->getZeHandle(ZeHandleDst, _pi_mem::write_only, Queue->Device));
  return enqueueMemCopyRectHelper(
      PI_COMMAND_TYPE_MEM_BUFFER_WRITE_RECT, Queue,
      const_cast<char *>(static_cast<const char *>(Ptr)), ZeHandleDst,
      HostOffset, BufferOffset, Region, HostRowPitch, BufferRowPitch,
      HostSlicePitch, BufferSlicePitch, BlockingWrite, NumEventsInWaitList,
      EventWaitList, Event);
#endif
}

pi_result piEnqueueMemBufferCopy(pi_queue Queue, pi_mem SrcMem, pi_mem DstMem,
                                 size_t SrcOffset, size_t DstOffset,
                                 size_t Size, pi_uint32 NumEventsInWaitList,
                                 const pi_event *EventWaitList,
                                 pi_event *Event) {

  return pi2ur::piEnqueueMemBufferCopy(Queue,
                                       SrcMem,
                                       DstMem,
                                       SrcOffset,
                                       DstOffset,
                                       Size,
                                       NumEventsInWaitList,
                                       EventWaitList,
                                       Event);

#if 0
  PI_ASSERT(SrcMem && DstMem, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  PI_ASSERT(!SrcMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(!DstMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
  auto SrcBuffer = pi_cast<pi_buffer>(SrcMem);
  auto DstBuffer = pi_cast<pi_buffer>(DstMem);

  std::shared_lock<pi_shared_mutex> SrcLock(SrcBuffer->Mutex, std::defer_lock);
  std::scoped_lock<std::shared_lock<pi_shared_mutex>, pi_shared_mutex,
                   pi_shared_mutex>
      LockAll(SrcLock, DstBuffer->Mutex, Queue->Mutex);

  // Copy engine is preferred only for host to device transfer.
  // Device to device transfers run faster on compute engines.
  bool PreferCopyEngine = (SrcBuffer->OnHost || DstBuffer->OnHost);

  // Temporary option added to use copy engine for D2D copy
  PreferCopyEngine |= UseCopyEngineForD2DCopy;

  char *ZeHandleSrc;
  PI_CALL(
      SrcBuffer->getZeHandle(ZeHandleSrc, _pi_mem::read_only, Queue->Device));
  char *ZeHandleDst;
  PI_CALL(
      DstBuffer->getZeHandle(ZeHandleDst, _pi_mem::write_only, Queue->Device));

  return enqueueMemCopyHelper(
      PI_COMMAND_TYPE_MEM_BUFFER_COPY, Queue, ZeHandleDst + DstOffset,
      false, // blocking
      Size, ZeHandleSrc + SrcOffset, NumEventsInWaitList, EventWaitList, Event,
      PreferCopyEngine);
#endif
}

pi_result piEnqueueMemBufferCopyRect(
    pi_queue Queue, pi_mem SrcMem, pi_mem DstMem, pi_buff_rect_offset SrcOrigin,
    pi_buff_rect_offset DstOrigin, pi_buff_rect_region Region,
    size_t SrcRowPitch, size_t SrcSlicePitch, size_t DstRowPitch,
    size_t DstSlicePitch, pi_uint32 NumEventsInWaitList,
    const pi_event *EventWaitList, pi_event *Event) {

  return pi2ur::piEnqueueMemBufferCopyRect(Queue,
                                           SrcMem,
                                           DstMem,
                                           SrcOrigin,
                                           DstOrigin,
                                           Region,
                                           SrcRowPitch,
                                           SrcSlicePitch,
                                           DstRowPitch,
                                           DstSlicePitch,
                                           NumEventsInWaitList,
                                           EventWaitList,
                                           Event);

#if 0
  PI_ASSERT(SrcMem && DstMem, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  PI_ASSERT(!SrcMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(!DstMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
  auto SrcBuffer = pi_cast<pi_buffer>(SrcMem);
  auto DstBuffer = pi_cast<pi_buffer>(DstMem);

  std::shared_lock<pi_shared_mutex> SrcLock(SrcBuffer->Mutex, std::defer_lock);
  std::scoped_lock<std::shared_lock<pi_shared_mutex>, pi_shared_mutex,
                   pi_shared_mutex>
      LockAll(SrcLock, DstBuffer->Mutex, Queue->Mutex);

  // Copy engine is preferred only for host to device transfer.
  // Device to device transfers run faster on compute engines.
  bool PreferCopyEngine = (SrcBuffer->OnHost || DstBuffer->OnHost);

  char *ZeHandleSrc;
  PI_CALL(
      SrcBuffer->getZeHandle(ZeHandleSrc, _pi_mem::read_only, Queue->Device));
  char *ZeHandleDst;
  PI_CALL(
      DstBuffer->getZeHandle(ZeHandleDst, _pi_mem::write_only, Queue->Device));

  return enqueueMemCopyRectHelper(
      PI_COMMAND_TYPE_MEM_BUFFER_COPY_RECT, Queue, ZeHandleSrc, ZeHandleDst,
      SrcOrigin, DstOrigin, Region, SrcRowPitch, DstRowPitch, SrcSlicePitch,
      DstSlicePitch,
      false, // blocking
      NumEventsInWaitList, EventWaitList, Event, PreferCopyEngine);
#endif
}

} // extern "C"

extern "C" {

pi_result piEnqueueMemBufferFill(pi_queue Queue, pi_mem Buffer,
                                 const void *Pattern, size_t PatternSize,
                                 size_t Offset, size_t Size,
                                 pi_uint32 NumEventsInWaitList,
                                 const pi_event *EventWaitList,
                                 pi_event *Event) {

  return pi2ur::piEnqueueMemBufferFill(Queue,
                                       Buffer,
                                       Pattern,
                                       PatternSize,
                                       Offset,
                                       Size,
                                       NumEventsInWaitList,
                                       EventWaitList,
                                       Event);
#if 0
  PI_ASSERT(Buffer, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(Queue->Mutex,
                                                          Buffer->Mutex);

  char *ZeHandleDst;
  PI_CALL(Buffer->getZeHandle(ZeHandleDst, _pi_mem::write_only, Queue->Device));
  return enqueueMemFillHelper(PI_COMMAND_TYPE_MEM_BUFFER_FILL, Queue,
                              ZeHandleDst + Offset, Pattern, PatternSize, Size,
                              NumEventsInWaitList, EventWaitList, Event);
#endif
}

pi_result piEnqueueMemBufferMap(pi_queue Queue, pi_mem Mem, pi_bool BlockingMap,
                                pi_map_flags MapFlags, size_t Offset,
                                size_t Size, pi_uint32 NumEventsInWaitList,
                                const pi_event *EventWaitList,
                                pi_event *OutEvent, void **RetMap) {

  return pi2ur::piEnqueueMemBufferMap(Queue,
                                      Mem,
                                      BlockingMap,
                                      MapFlags,
                                      Offset,
                                      Size,
                                      NumEventsInWaitList,
                                      EventWaitList,
                                      OutEvent,
                                      RetMap);
#if 0
  // TODO: we don't implement read-only or write-only, always read-write.
  // assert((map_flags & PI_MAP_READ) != 0);
  // assert((map_flags & PI_MAP_WRITE) != 0);
  PI_ASSERT(Mem, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  PI_ASSERT(!Mem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
  auto Buffer = pi_cast<pi_buffer>(Mem);

  pi_event InternalEvent;
  bool IsInternal = OutEvent == nullptr;
  pi_event *Event = OutEvent ? OutEvent : &InternalEvent;
  ze_event_handle_t ZeEvent = nullptr;

  bool UseCopyEngine = false;
  {
    // Lock automatically releases when this goes out of scope.
    std::scoped_lock<pi_shared_mutex> lock(Queue->Mutex);

    _pi_ze_event_list_t TmpWaitList;
    if (auto Res = ur2piResult(TmpWaitList.createAndRetainPiZeEventList(
            NumEventsInWaitList, reinterpret_cast<const ur_event_handle_t *>(EventWaitList), reinterpret_cast<ur_queue_handle_t>(Queue), UseCopyEngine)))
      return Res;

    auto Res = ur2piResult(createEventAndAssociateQueue(reinterpret_cast<ur_queue_handle_t>(Queue),
                                                        reinterpret_cast<ur_event_handle_t *>(Event),
                                                        PI_COMMAND_TYPE_MEM_BUFFER_MAP,
                                                        Queue->CommandListMap.end(),
                                                        IsInternal));
    if (Res != PI_SUCCESS)
      return Res;

    ZeEvent = (*Event)->ZeEvent;
    (*Event)->WaitList = TmpWaitList;
  }

  // Translate the host access mode info.
  _pi_mem::access_mode_t AccessMode = _pi_mem::unknown;
  if (MapFlags & PI_MAP_WRITE_INVALIDATE_REGION)
    AccessMode = _pi_mem::write_only;
  else {
    if (MapFlags & PI_MAP_READ) {
      AccessMode = _pi_mem::read_only;
      if (MapFlags & PI_MAP_WRITE)
        AccessMode = _pi_mem::read_write;
    } else if (MapFlags & PI_MAP_WRITE)
      AccessMode = _pi_mem::write_only;
  }
  PI_ASSERT(AccessMode != _pi_mem::unknown, PI_ERROR_INVALID_VALUE);

  // TODO: Level Zero is missing the memory "mapping" capabilities, so we are
  // left to doing new memory allocation and a copy (read) on discrete devices.
  // For integrated devices, we have allocated the buffer in host memory so no
  // actions are needed here except for synchronizing on incoming events.
  // A host-to-host copy is done if a host pointer had been supplied during
  // buffer creation on integrated devices.
  //
  // TODO: for discrete, check if the input buffer is already allocated
  // in shared memory and thus is accessible from the host as is.
  // Can we get SYCL RT to predict/allocate in shared memory
  // from the beginning?

  // For integrated devices the buffer has been allocated in host memory.
  if (Buffer->OnHost) {
    // Wait on incoming events before doing the copy
    if (NumEventsInWaitList > 0)
      PI_CALL(piEventsWait(NumEventsInWaitList, EventWaitList));

    if (Queue->isInOrderQueue())
      PI_CALL(piQueueFinish(Queue));

    // Lock automatically releases when this goes out of scope.
    std::scoped_lock<pi_shared_mutex> Guard(Buffer->Mutex);

    char *ZeHandleSrc;
    PI_CALL(Buffer->getZeHandle(ZeHandleSrc, AccessMode, Queue->Device));

    if (Buffer->MapHostPtr) {
      *RetMap = Buffer->MapHostPtr + Offset;
      if (ZeHandleSrc != Buffer->MapHostPtr &&
          AccessMode != _pi_mem::write_only) {
        memcpy(*RetMap, ZeHandleSrc + Offset, Size);
      }
    } else {
      *RetMap = ZeHandleSrc + Offset;
    }

    auto Res = Buffer->Mappings.insert({*RetMap, {Offset, Size}});
    // False as the second value in pair means that mapping was not inserted
    // because mapping already exists.
    if (!Res.second) {
      zePrint("piEnqueueMemBufferMap: duplicate mapping detected\n");
      return PI_ERROR_INVALID_VALUE;
    }

    // Signal this event
    ZE_CALL(zeEventHostSignal, (ZeEvent));
    (*Event)->Completed = true;
    return PI_SUCCESS;
  }

  // Lock automatically releases when this goes out of scope.
  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(Queue->Mutex,
                                                          Buffer->Mutex);

  if (Buffer->MapHostPtr) {
    *RetMap = Buffer->MapHostPtr + Offset;
  } else {
    // TODO: use USM host allocator here
    // TODO: Do we even need every map to allocate new host memory?
    //       In the case when the buffer is "OnHost" we use single allocation.
    if (auto Res = ZeHostMemAllocHelper(RetMap, Queue->Context, Size))
      return Res;
  }

  // Take a shortcut if the host is not going to read buffer's data.
  if (AccessMode == _pi_mem::write_only) {
    (*Event)->Completed = true;
  } else {
    // For discrete devices we need a command list
    pi_command_list_ptr_t CommandList{};
    if (auto Res = ur2piResult(Queue->Context->getAvailableCommandList(reinterpret_cast<ur_queue_handle_t>(Queue), CommandList,
                                                           UseCopyEngine)))
      return Res;

    // Add the event to the command list.
    CommandList->second.append(reinterpret_cast<ur_event_handle_t>(*Event));
    (*Event)->RefCount.increment();

    const auto &ZeCommandList = CommandList->first;
    const auto &WaitList = (*Event)->WaitList;

    char *ZeHandleSrc;
    PI_CALL(Buffer->getZeHandle(ZeHandleSrc, AccessMode, Queue->Device));

    ZE_CALL(zeCommandListAppendMemoryCopy,
            (ZeCommandList, *RetMap, ZeHandleSrc + Offset, Size, ZeEvent,
             WaitList.Length, WaitList.ZeEventList));

    if (auto Res = ur2piResult(Queue->executeCommandList(CommandList, BlockingMap)))
      return Res;
  }

  auto Res = Buffer->Mappings.insert({*RetMap, {Offset, Size}});
  // False as the second value in pair means that mapping was not inserted
  // because mapping already exists.
  if (!Res.second) {
    zePrint("piEnqueueMemBufferMap: duplicate mapping detected\n");
    return PI_ERROR_INVALID_VALUE;
  }
  return PI_SUCCESS;
#endif
}

pi_result piEnqueueMemUnmap(pi_queue Queue, pi_mem Mem, void *MappedPtr,
                            pi_uint32 NumEventsInWaitList,
                            const pi_event *EventWaitList, pi_event *OutEvent) {
  
  return pi2ur::piEnqueueMemUnmap(Queue,
                                  Mem,
                                  MappedPtr,
                                  NumEventsInWaitList,
                                  EventWaitList,
                                  OutEvent);

#if 0
  PI_ASSERT(Mem, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  PI_ASSERT(!Mem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
  auto Buffer = pi_cast<pi_buffer>(Mem);

  bool UseCopyEngine = false;

  ze_event_handle_t ZeEvent = nullptr;
  pi_event InternalEvent;
  bool IsInternal = OutEvent == nullptr;
  pi_event *Event = OutEvent ? OutEvent : &InternalEvent;
  {
    // Lock automatically releases when this goes out of scope.
    std::scoped_lock<pi_shared_mutex> lock(Queue->Mutex);

    _pi_ze_event_list_t TmpWaitList;
    if (auto Res = ur2piResult(TmpWaitList.createAndRetainPiZeEventList(
            NumEventsInWaitList, reinterpret_cast<const ur_event_handle_t *>(EventWaitList), reinterpret_cast<ur_queue_handle_t>(Queue), UseCopyEngine)))
      return Res;

    auto Res = ur2piResult(createEventAndAssociateQueue(reinterpret_cast<ur_queue_handle_t>(Queue),
                                                        reinterpret_cast<ur_event_handle_t *>(Event),
                                                        PI_COMMAND_TYPE_MEM_BUFFER_UNMAP,
                                                        Queue->CommandListMap.end(),
                                                        IsInternal));
    if (Res != PI_SUCCESS)
      return Res;
    ZeEvent = (*Event)->ZeEvent;
    (*Event)->WaitList = TmpWaitList;
  }

  _pi_buffer::Mapping MapInfo = {};
  {
    // Lock automatically releases when this goes out of scope.
    std::scoped_lock<pi_shared_mutex> Guard(Buffer->Mutex);
    auto It = Buffer->Mappings.find(MappedPtr);
    if (It == Buffer->Mappings.end()) {
      zePrint("piEnqueueMemUnmap: unknown memory mapping\n");
      return PI_ERROR_INVALID_VALUE;
    }
    MapInfo = It->second;
    Buffer->Mappings.erase(It);

    // NOTE: we still have to free the host memory allocated/returned by
    // piEnqueueMemBufferMap, but can only do so after the above copy
    // is completed. Instead of waiting for It here (blocking), we shall
    // do so in piEventRelease called for the pi_event tracking the unmap.
    // In the case of an integrated device, the map operation does not allocate
    // any memory, so there is nothing to free. This is indicated by a nullptr.
    (*Event)->CommandData =
        (Buffer->OnHost ? nullptr : (Buffer->MapHostPtr ? nullptr : MappedPtr));
  }

  // For integrated devices the buffer is allocated in host memory.
  if (Buffer->OnHost) {
    // Wait on incoming events before doing the copy
    if (NumEventsInWaitList > 0)
      PI_CALL(piEventsWait(NumEventsInWaitList, EventWaitList));

    if (Queue->isInOrderQueue())
      PI_CALL(piQueueFinish(Queue));

    char *ZeHandleDst;
    PI_CALL(
        Buffer->getZeHandle(ZeHandleDst, _pi_mem::write_only, Queue->Device));

    std::scoped_lock<pi_shared_mutex> Guard(Buffer->Mutex);
    if (Buffer->MapHostPtr)
      memcpy(ZeHandleDst + MapInfo.Offset, MappedPtr, MapInfo.Size);

    // Signal this event
    ZE_CALL(zeEventHostSignal, (ZeEvent));
    (*Event)->Completed = true;
    return PI_SUCCESS;
  }

  // Lock automatically releases when this goes out of scope.
  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(Queue->Mutex,
                                                          Buffer->Mutex);

  pi_command_list_ptr_t CommandList{};
  if (auto Res = ur2piResult(Queue->Context->getAvailableCommandList(reinterpret_cast<ur_queue_handle_t>(Queue), CommandList,
                                                         UseCopyEngine)))
    return Res;

  CommandList->second.append(reinterpret_cast<ur_event_handle_t>(*Event));
  (*Event)->RefCount.increment();

  const auto &ZeCommandList = CommandList->first;

  // TODO: Level Zero is missing the memory "mapping" capabilities, so we are
  // left to doing copy (write back to the device).
  //
  // NOTE: Keep this in sync with the implementation of
  // piEnqueueMemBufferMap.

  char *ZeHandleDst;
  PI_CALL(Buffer->getZeHandle(ZeHandleDst, _pi_mem::write_only, Queue->Device));

  ZE_CALL(zeCommandListAppendMemoryCopy,
          (ZeCommandList, ZeHandleDst + MapInfo.Offset, MappedPtr, MapInfo.Size,
           ZeEvent, (*Event)->WaitList.Length, (*Event)->WaitList.ZeEventList));

  // Execute command list asynchronously, as the event will be used
  // to track down its completion.
  if (auto Res = ur2piResult(Queue->executeCommandList(CommandList)))
    return Res;

  return PI_SUCCESS;
#endif
}

pi_result piMemImageGetInfo(pi_mem Image, pi_image_info ParamName,
                            size_t ParamValueSize, void *ParamValue,
                            size_t *ParamValueSizeRet) {
  
  return pi2ur::piMemImageGetInfo(Image,
                                  ParamName,
                                  ParamValueSize,
                                  ParamValue,
                                  ParamValueSizeRet);
}

} // extern "C"

#if 0
static pi_result getImageRegionHelper(pi_mem Mem, pi_image_offset Origin,
                                      pi_image_region Region,
                                      ze_image_region_t &ZeRegion) {

  PI_ASSERT(Mem, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Origin, PI_ERROR_INVALID_VALUE);

#ifndef NDEBUG
  PI_ASSERT(Mem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
  auto Image = static_cast<_pi_image *>(Mem);
  ze_image_desc_t &ZeImageDesc = Image->ZeImageDesc;

  PI_ASSERT((ZeImageDesc.type == ZE_IMAGE_TYPE_1D && Origin->y == 0 &&
             Origin->z == 0) ||
                (ZeImageDesc.type == ZE_IMAGE_TYPE_1DARRAY && Origin->z == 0) ||
                (ZeImageDesc.type == ZE_IMAGE_TYPE_2D && Origin->z == 0) ||
                (ZeImageDesc.type == ZE_IMAGE_TYPE_3D),
            PI_ERROR_INVALID_VALUE);

  PI_ASSERT(Region->width && Region->height && Region->depth,
            PI_ERROR_INVALID_VALUE);
  PI_ASSERT(
      (ZeImageDesc.type == ZE_IMAGE_TYPE_1D && Region->height == 1 &&
       Region->depth == 1) ||
          (ZeImageDesc.type == ZE_IMAGE_TYPE_1DARRAY && Region->depth == 1) ||
          (ZeImageDesc.type == ZE_IMAGE_TYPE_2D && Region->depth == 1) ||
          (ZeImageDesc.type == ZE_IMAGE_TYPE_3D),
      PI_ERROR_INVALID_VALUE);
#endif // !NDEBUG

  uint32_t OriginX = pi_cast<uint32_t>(Origin->x);
  uint32_t OriginY = pi_cast<uint32_t>(Origin->y);
  uint32_t OriginZ = pi_cast<uint32_t>(Origin->z);

  uint32_t Width = pi_cast<uint32_t>(Region->width);
  uint32_t Height = pi_cast<uint32_t>(Region->height);
  uint32_t Depth = pi_cast<uint32_t>(Region->depth);

  ZeRegion = {OriginX, OriginY, OriginZ, Width, Height, Depth};

  return PI_SUCCESS;
}
#endif

#if 0
// Helper function to implement image read/write/copy.
// PI interfaces must have queue's and destination image's mutexes locked for
// exclusive use and source image's mutex locked for shared use on entry.
static pi_result enqueueMemImageCommandHelper(
    pi_command_type CommandType, pi_queue Queue,
    const void *Src, // image or ptr
    void *Dst,       // image or ptr
    pi_bool IsBlocking, pi_image_offset SrcOrigin, pi_image_offset DstOrigin,
    pi_image_region Region, size_t RowPitch, size_t SlicePitch,
    pi_uint32 NumEventsInWaitList, const pi_event *EventWaitList,
    pi_event *OutEvent, bool PreferCopyEngine = false) {
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  bool UseCopyEngine = Queue->useCopyEngine(PreferCopyEngine);

  _pi_ze_event_list_t TmpWaitList;
  if (auto Res = ur2piResult(TmpWaitList.createAndRetainPiZeEventList(
          NumEventsInWaitList, reinterpret_cast<const ur_event_handle_t *>(EventWaitList), reinterpret_cast<ur_queue_handle_t>(Queue), UseCopyEngine)))
    return Res;

  // We want to batch these commands to avoid extra submissions (costly)
  bool OkToBatch = true;

  // Get a new command list to be used on this call
  pi_command_list_ptr_t CommandList{};
  if (auto Res = ur2piResult(Queue->Context->getAvailableCommandList(
          reinterpret_cast<ur_queue_handle_t>(Queue), CommandList, UseCopyEngine, OkToBatch)))
    return Res;

  ze_event_handle_t ZeEvent = nullptr;
  pi_event InternalEvent;
  bool IsInternal = OutEvent == nullptr;
  pi_event *Event = OutEvent ? OutEvent : &InternalEvent;
  auto Res = ur2piResult(createEventAndAssociateQueue(reinterpret_cast<ur_queue_handle_t>(Queue),
                                                      reinterpret_cast<ur_event_handle_t *>(Event),
                                                      CommandType,
                                                      CommandList,
                                                      IsInternal));
  if (Res != PI_SUCCESS)
    return Res;
  ZeEvent = (*Event)->ZeEvent;
  (*Event)->WaitList = TmpWaitList;

  const auto &ZeCommandList = CommandList->first;
  const auto &WaitList = (*Event)->WaitList;

  if (CommandType == PI_COMMAND_TYPE_IMAGE_READ) {
    pi_mem SrcMem = pi_cast<pi_mem>(const_cast<void *>(Src));

    ze_image_region_t ZeSrcRegion;
    auto Result = getImageRegionHelper(SrcMem, SrcOrigin, Region, ZeSrcRegion);
    if (Result != PI_SUCCESS)
      return Result;

    // TODO: Level Zero does not support row_pitch/slice_pitch for images yet.
    // Check that SYCL RT did not want pitch larger than default.
    (void)RowPitch;
    (void)SlicePitch;
#ifndef NDEBUG
    PI_ASSERT(SrcMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);

    auto SrcImage = static_cast<_pi_image *>(SrcMem);
    const ze_image_desc_t &ZeImageDesc = SrcImage->ZeImageDesc;
    PI_ASSERT(
        RowPitch == 0 ||
            // special case RGBA image pitch equal to region's width
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32 &&
             RowPitch == 4 * 4 * ZeSrcRegion.width) ||
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16 &&
             RowPitch == 4 * 2 * ZeSrcRegion.width) ||
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8 &&
             RowPitch == 4 * ZeSrcRegion.width),
        PI_ERROR_INVALID_IMAGE_SIZE);
    PI_ASSERT(SlicePitch == 0 || SlicePitch == RowPitch * ZeSrcRegion.height,
              PI_ERROR_INVALID_IMAGE_SIZE);
#endif // !NDEBUG

    char *ZeHandleSrc;
    PI_CALL(
        SrcMem->getZeHandle(ZeHandleSrc, _pi_mem::read_only, Queue->Device));
    ZE_CALL(zeCommandListAppendImageCopyToMemory,
            (ZeCommandList, Dst, pi_cast<ze_image_handle_t>(ZeHandleSrc),
             &ZeSrcRegion, ZeEvent, WaitList.Length, WaitList.ZeEventList));
  } else if (CommandType == PI_COMMAND_TYPE_IMAGE_WRITE) {
    pi_mem DstMem = pi_cast<pi_mem>(Dst);
    ze_image_region_t ZeDstRegion;
    auto Result = getImageRegionHelper(DstMem, DstOrigin, Region, ZeDstRegion);
    if (Result != PI_SUCCESS)
      return Result;

      // TODO: Level Zero does not support row_pitch/slice_pitch for images yet.
      // Check that SYCL RT did not want pitch larger than default.
#ifndef NDEBUG
    PI_ASSERT(DstMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);

    auto DstImage = static_cast<_pi_image *>(DstMem);
    const ze_image_desc_t &ZeImageDesc = DstImage->ZeImageDesc;
    PI_ASSERT(
        RowPitch == 0 ||
            // special case RGBA image pitch equal to region's width
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32 &&
             RowPitch == 4 * 4 * ZeDstRegion.width) ||
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16 &&
             RowPitch == 4 * 2 * ZeDstRegion.width) ||
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8 &&
             RowPitch == 4 * ZeDstRegion.width),
        PI_ERROR_INVALID_IMAGE_SIZE);
    PI_ASSERT(SlicePitch == 0 || SlicePitch == RowPitch * ZeDstRegion.height,
              PI_ERROR_INVALID_IMAGE_SIZE);
#endif // !NDEBUG

    char *ZeHandleDst;
    PI_CALL(
        DstMem->getZeHandle(ZeHandleDst, _pi_mem::write_only, Queue->Device));
    ZE_CALL(zeCommandListAppendImageCopyFromMemory,
            (ZeCommandList, pi_cast<ze_image_handle_t>(ZeHandleDst), Src,
             &ZeDstRegion, ZeEvent, WaitList.Length, WaitList.ZeEventList));
  } else if (CommandType == PI_COMMAND_TYPE_IMAGE_COPY) {
    pi_mem SrcImage = pi_cast<pi_mem>(const_cast<void *>(Src));
    pi_mem DstImage = pi_cast<pi_mem>(Dst);

    ze_image_region_t ZeSrcRegion;
    auto Result =
        getImageRegionHelper(SrcImage, SrcOrigin, Region, ZeSrcRegion);
    if (Result != PI_SUCCESS)
      return Result;
    ze_image_region_t ZeDstRegion;
    Result = getImageRegionHelper(DstImage, DstOrigin, Region, ZeDstRegion);
    if (Result != PI_SUCCESS)
      return Result;

    char *ZeHandleSrc;
    char *ZeHandleDst;
    PI_CALL(
        SrcImage->getZeHandle(ZeHandleSrc, _pi_mem::read_only, Queue->Device));
    PI_CALL(
        DstImage->getZeHandle(ZeHandleDst, _pi_mem::write_only, Queue->Device));
    ZE_CALL(zeCommandListAppendImageCopyRegion,
            (ZeCommandList, pi_cast<ze_image_handle_t>(ZeHandleDst),
             pi_cast<ze_image_handle_t>(ZeHandleSrc), &ZeDstRegion,
             &ZeSrcRegion, ZeEvent, 0, nullptr));
  } else {
    zePrint("enqueueMemImageUpdate: unsupported image command type\n");
    return PI_ERROR_INVALID_OPERATION;
  }

  if (auto Res = ur2piResult(Queue->executeCommandList(CommandList, IsBlocking, OkToBatch)))
    return Res;

  return PI_SUCCESS;
}
#endif

extern "C" {

pi_result piEnqueueMemImageRead(pi_queue Queue, pi_mem Image,
                                pi_bool BlockingRead, pi_image_offset Origin,
                                pi_image_region Region, size_t RowPitch,
                                size_t SlicePitch, void *Ptr,
                                pi_uint32 NumEventsInWaitList,
                                const pi_event *EventWaitList,
                                pi_event *Event) {
  return pi2ur::piEnqueueMemImageRead(Queue,
                                      Image,
                                      BlockingRead,
                                      Origin,
                                      Region,
                                      RowPitch,
                                      SlicePitch,
                                      Ptr,
                                      NumEventsInWaitList,
                                      EventWaitList,
                                      Event);
#if 0
 PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  std::shared_lock<pi_shared_mutex> SrcLock(Image->Mutex, std::defer_lock);
  std::scoped_lock<std::shared_lock<pi_shared_mutex>, pi_shared_mutex> LockAll(
      SrcLock, Queue->Mutex);
  return enqueueMemImageCommandHelper(
      PI_COMMAND_TYPE_IMAGE_READ, Queue,
      Image, // src
      Ptr,   // dst
      BlockingRead,
      Origin,  // SrcOrigin
      nullptr, // DstOrigin
      Region, RowPitch, SlicePitch, NumEventsInWaitList, EventWaitList, Event);
#endif
}

pi_result piEnqueueMemImageWrite(pi_queue Queue, pi_mem Image,
                                 pi_bool BlockingWrite, pi_image_offset Origin,
                                 pi_image_region Region, size_t InputRowPitch,
                                 size_t InputSlicePitch, const void *Ptr,
                                 pi_uint32 NumEventsInWaitList,
                                 const pi_event *EventWaitList,
                                 pi_event *Event) {

  return pi2ur::piEnqueueMemImageWrite(Queue,
                                       Image,
                                       BlockingWrite,
                                       Origin,
                                       Region,
                                       InputRowPitch,
                                       InputSlicePitch,
                                       Ptr,
                                       NumEventsInWaitList,
                                       EventWaitList,
                                       Event);
}

pi_result
piEnqueueMemImageCopy(pi_queue Queue, pi_mem SrcImage, pi_mem DstImage,
                      pi_image_offset SrcOrigin, pi_image_offset DstOrigin,
                      pi_image_region Region, pi_uint32 NumEventsInWaitList,
                      const pi_event *EventWaitList, pi_event *Event) {
  return pi2ur::piEnqueueMemImageCopy(Queue,
                                      SrcImage,
                                      DstImage,
                                      SrcOrigin,
                                      DstOrigin,
                                      Region,
                                      NumEventsInWaitList,
                                      EventWaitList,
                                      Event);
#if 0
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  std::shared_lock<pi_shared_mutex> SrcLock(SrcImage->Mutex, std::defer_lock);
  std::scoped_lock<std::shared_lock<pi_shared_mutex>, pi_shared_mutex,
                   pi_shared_mutex>
      LockAll(SrcLock, DstImage->Mutex, Queue->Mutex);
  // Copy engine is preferred only for host to device transfer.
  // Device to device transfers run faster on compute engines.
  // Images are always allocated on device.
  bool PreferCopyEngine = false;
  return enqueueMemImageCommandHelper(
      PI_COMMAND_TYPE_IMAGE_COPY, Queue, SrcImage, DstImage,
      false, // is_blocking
      SrcOrigin, DstOrigin, Region,
      0, // row pitch
      0, // slice pitch
      NumEventsInWaitList, EventWaitList, Event, PreferCopyEngine);
#endif
}

pi_result piEnqueueMemImageFill(pi_queue Queue, pi_mem Image,
                                const void *FillColor, const size_t *Origin,
                                const size_t *Region,
                                pi_uint32 NumEventsInWaitList,
                                const pi_event *EventWaitList,
                                pi_event *Event) {

  return pi2ur::piEnqueueMemImageFill(Queue,
                                      Image,
                                      FillColor,
                                      Origin,
                                      Region,
                                      NumEventsInWaitList,
                                      EventWaitList,
                                      Event);
#if 0
  (void)Image;
  (void)FillColor;
  (void)Origin;
  (void)Region;
  (void)NumEventsInWaitList;
  (void)EventWaitList;
  (void)Event;

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  // Lock automatically releases when this goes out of scope.
  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(Queue->Mutex,
                                                          Image->Mutex);

  die("piEnqueueMemImageFill: not implemented");
  return {};
#endif
}

pi_result piMemBufferPartition(pi_mem Buffer, pi_mem_flags Flags,
                               pi_buffer_create_type BufferCreateType,
                               void *BufferCreateInfo, pi_mem *RetMem) {

  return pi2ur::piMemBufferPartition(Buffer,
                                     Flags,
                                     BufferCreateType,
                                     BufferCreateInfo,
                                     RetMem);
#if 0
  PI_ASSERT(Buffer && !Buffer->isImage() &&
                !(static_cast<pi_buffer>(Buffer))->isSubBuffer(),
            PI_ERROR_INVALID_MEM_OBJECT);

  PI_ASSERT(BufferCreateType == PI_BUFFER_CREATE_TYPE_REGION &&
                BufferCreateInfo && RetMem,
            PI_ERROR_INVALID_VALUE);

  std::shared_lock<pi_shared_mutex> Guard(Buffer->Mutex);

  if (Flags != PI_MEM_FLAGS_ACCESS_RW) {
    die("piMemBufferPartition: Level-Zero implements only read-write buffer,"
        "no read-only or write-only yet.");
  }

  auto Region = (pi_buffer_region)BufferCreateInfo;

  PI_ASSERT(Region->size != 0u, PI_ERROR_INVALID_BUFFER_SIZE);
  PI_ASSERT(Region->origin <= (Region->origin + Region->size),
            PI_ERROR_INVALID_VALUE);

  try {
    *RetMem = new _pi_buffer(static_cast<pi_buffer>(Buffer), Region->origin,
                             Region->size);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
#endif
}

pi_result piEnqueueNativeKernel(pi_queue Queue, void (*UserFunc)(void *),
                                void *Args, size_t CbArgs,
                                pi_uint32 NumMemObjects, const pi_mem *MemList,
                                const void **ArgsMemLoc,
                                pi_uint32 NumEventsInWaitList,
                                const pi_event *EventWaitList,
                                pi_event *Event) {
  return pi2ur::piEnqueueNativeKernel(Queue,
                                      UserFunc,
                                      Args,
                                      CbArgs,
                                      NumMemObjects,
                                      MemList,
                                      ArgsMemLoc,
                                      NumEventsInWaitList,
                                      EventWaitList,
                                      Event);
#if 0
  (void)UserFunc;
  (void)Args;
  (void)CbArgs;
  (void)NumMemObjects;
  (void)MemList;
  (void)ArgsMemLoc;
  (void)NumEventsInWaitList;
  (void)EventWaitList;
  (void)Event;

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  // Lock automatically releases when this goes out of scope.
  std::scoped_lock<pi_shared_mutex> lock(Queue->Mutex);

  die("piEnqueueNativeKernel: not implemented");
  return {};
#endif
}

#if 0
// Function gets characters between delimeter's in str
// then checks if they are equal to the sub_str.
// returns true if there is at least one instance
// returns false if there are no instances of the name
static bool is_in_separated_string(const std::string &str, char delimiter,
                                   const std::string &sub_str) {
  size_t beg = 0;
  size_t length = 0;
  for (const auto &x : str) {
    if (x == delimiter) {
      if (str.substr(beg, length) == sub_str)
        return true;

      beg += length + 1;
      length = 0;
      continue;
    }
    length++;
  }
  if (length != 0)
    if (str.substr(beg, length) == sub_str)
      return true;

  return false;
}
#endif

// TODO: Check if the function_pointer_ret type can be converted to void**.
pi_result piextGetDeviceFunctionPointer(pi_device Device, pi_program Program,
                                        const char *FunctionName,
                                        pi_uint64 *FunctionPointerRet) {

  return pi2ur::piextGetDeviceFunctionPointer(Device,
                                              Program,
                                              FunctionName,
                                              FunctionPointerRet);
#if 0
  (void)Device;
  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);

  std::shared_lock<pi_shared_mutex> Guard(Program->Mutex);
  if (Program->State != _pi_program::Exe) {
    return PI_ERROR_INVALID_PROGRAM_EXECUTABLE;
  }

  ze_result_t ZeResult =
      ZE_CALL_NOCHECK(zeModuleGetFunctionPointer,
                      (Program->ZeModule, FunctionName,
                       reinterpret_cast<void **>(FunctionPointerRet)));

  // zeModuleGetFunctionPointer currently fails for all
  // kernels regardless of if the kernel exist or not
  // with ZE_RESULT_ERROR_INVALID_ARGUMENT
  // TODO: remove when this is no longer the case
  // If zeModuleGetFunctionPointer returns invalid argument,
  // fallback to searching through kernel list and return
  // PI_ERROR_FUNCTION_ADDRESS_IS_NOT_AVAILABLE if the function exists
  // or PI_ERROR_INVALID_KERNEL_NAME if the function does not exist.
  // FunctionPointerRet should always be 0
  if (ZeResult == ZE_RESULT_ERROR_INVALID_ARGUMENT) {
    size_t Size;
    *FunctionPointerRet = 0;
    PI_CALL(piProgramGetInfo(Program, PI_PROGRAM_INFO_KERNEL_NAMES, 0, nullptr,
                             &Size));

    std::string ClResult(Size, ' ');
    PI_CALL(piProgramGetInfo(Program, PI_PROGRAM_INFO_KERNEL_NAMES,
                             ClResult.size(), &ClResult[0], nullptr));

    // Get rid of the null terminator and search for kernel_name
    // If function can be found return error code to indicate it
    // exists
    ClResult.pop_back();
    if (is_in_separated_string(ClResult, ';', std::string(FunctionName)))
      return PI_ERROR_FUNCTION_ADDRESS_IS_NOT_AVAILABLE;

    return PI_ERROR_INVALID_KERNEL_NAME;
  }

  if (ZeResult == ZE_RESULT_ERROR_INVALID_FUNCTION_NAME) {
    *FunctionPointerRet = 0;
    return PI_ERROR_INVALID_KERNEL_NAME;
  }

  return mapError(ZeResult);
#endif
}

pi_result piextUSMDeviceAlloc(void **ResultPtr, pi_context Context,
                              pi_device Device,
                              pi_usm_mem_properties *Properties, size_t Size,
                              pi_uint32 Alignment) {

  return pi2ur::piextUSMDeviceAlloc(ResultPtr,
                                    Context,
                                    Device,
                                    Properties,
                                    Size,
                                    Alignment);
#if 0
  // L0 supports alignment up to 64KB and silently ignores higher values.
  // We flag alignment > 64KB as an invalid value.
  if (Alignment > 65536)
    return PI_ERROR_INVALID_VALUE;

  pi_platform Plt = Device->Platform;

  // If indirect access tracking is enabled then lock the mutex which is
  // guarding contexts container in the platform. This prevents new kernels from
  // being submitted in any context while we are in the process of allocating a
  // memory, this is needed to properly capture allocations by kernels with
  // indirect access. This lock also protects access to the context's data
  // structures. If indirect access tracking is not enabled then lock context
  // mutex to protect access to context's data structures.
  std::shared_lock<pi_shared_mutex> ContextLock(Context->Mutex,
                                                std::defer_lock);
  std::unique_lock<pi_shared_mutex> IndirectAccessTrackingLock(
      Plt->ContextsMutex, std::defer_lock);
  if (IndirectAccessTrackingEnabled) {
    IndirectAccessTrackingLock.lock();
    // We are going to defer memory release if there are kernels with indirect
    // access, that is why explicitly retain context to be sure that it is
    // released after all memory allocations in this context are released.
    PI_CALL(piContextRetain(Context));
  } else {
    ContextLock.lock();
  }

  if (!UseUSMAllocator ||
      // L0 spec says that allocation fails if Alignment != 2^n, in order to
      // keep the same behavior for the allocator, just call L0 API directly and
      // return the error code.
      ((Alignment & (Alignment - 1)) != 0)) {
    pi_result Res = ur2piResult(USMDeviceAllocImpl(ResultPtr,
                                       reinterpret_cast<ur_context_handle_t>(Context),
                                       reinterpret_cast<ur_device_handle_t>(Device),
                                       Properties,
                                       Size,
                                       Alignment));
    if (IndirectAccessTrackingEnabled) {
      // Keep track of all memory allocations in the context
      Context->MemAllocs.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(*ResultPtr),
                                 std::forward_as_tuple(reinterpret_cast<ur_context_handle_t>(Context)));
    }
    return Res;
  }

  try {
    auto It = Context->DeviceMemAllocContexts.find(Device->ZeDevice);
    if (It == Context->DeviceMemAllocContexts.end())
      return PI_ERROR_INVALID_VALUE;

    *ResultPtr = It->second.allocate(Size, Alignment);
    if (IndirectAccessTrackingEnabled) {
      // Keep track of all memory allocations in the context
      Context->MemAllocs.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(*ResultPtr),
                                 std::forward_as_tuple(reinterpret_cast<ur_context_handle_t>(Context)));
    }

  } catch (const UsmAllocationException &Ex) {
    *ResultPtr = nullptr;
    return ur2piResult(Ex.getError());
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
#endif
}

pi_result piextUSMSharedAlloc(void **ResultPtr, pi_context Context,
                              pi_device Device,
                              pi_usm_mem_properties *Properties, size_t Size,
                              pi_uint32 Alignment) {

  return pi2ur::piextUSMSharedAlloc(ResultPtr,
                                    Context,
                                    Device,
                                    Properties,
                                    Size,
                                    Alignment);
#if 0  
  // See if the memory is going to be read-only on the device.
  bool DeviceReadOnly = false;
  // Check that incorrect bits are not set in the properties.
  if (Properties && *Properties != 0) {
    PI_ASSERT(*(Properties) == PI_MEM_ALLOC_FLAGS && *(Properties + 2) == 0,
              PI_ERROR_INVALID_VALUE);
    DeviceReadOnly = *(Properties + 1) & PI_MEM_ALLOC_DEVICE_READ_ONLY;
  }

  // L0 supports alignment up to 64KB and silently ignores higher values.
  // We flag alignment > 64KB as an invalid value.
  if (Alignment > 65536)
    return PI_ERROR_INVALID_VALUE;

  pi_platform Plt = Device->Platform;

  // If indirect access tracking is enabled then lock the mutex which is
  // guarding contexts container in the platform. This prevents new kernels from
  // being submitted in any context while we are in the process of allocating a
  // memory, this is needed to properly capture allocations by kernels with
  // indirect access. This lock also protects access to the context's data
  // structures. If indirect access tracking is not enabled then lock context
  // mutex to protect access to context's data structures.
  std::scoped_lock<pi_shared_mutex> Lock(
      IndirectAccessTrackingEnabled ? Plt->ContextsMutex : Context->Mutex);

  if (IndirectAccessTrackingEnabled) {
    // We are going to defer memory release if there are kernels with indirect
    // access, that is why explicitly retain context to be sure that it is
    // released after all memory allocations in this context are released.
    PI_CALL(piContextRetain(Context));
  }

  if (!UseUSMAllocator ||
      // L0 spec says that allocation fails if Alignment != 2^n, in order to
      // keep the same behavior for the allocator, just call L0 API directly and
      // return the error code.
      ((Alignment & (Alignment - 1)) != 0)) {
    pi_result Res = ur2piResult(USMSharedAllocImpl(ResultPtr,
                                                   reinterpret_cast<ur_context_handle_t>(Context),
                                                   reinterpret_cast<ur_device_handle_t>(Device),
                                                   Properties,
                                                   Size, Alignment));
    if (IndirectAccessTrackingEnabled) {
      // Keep track of all memory allocations in the context
      Context->MemAllocs.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(*ResultPtr),
                                 std::forward_as_tuple(reinterpret_cast<ur_context_handle_t>(Context)));
    }
    return Res;
  }

  try {
    auto &Allocator = (DeviceReadOnly ? Context->SharedReadOnlyMemAllocContexts
                                      : Context->SharedMemAllocContexts);
    auto It = Allocator.find(Device->ZeDevice);
    if (It == Allocator.end())
      return PI_ERROR_INVALID_VALUE;

    *ResultPtr = It->second.allocate(Size, Alignment);
    if (DeviceReadOnly) {
      Context->SharedReadOnlyAllocs.insert(*ResultPtr);
    }
    if (IndirectAccessTrackingEnabled) {
      // Keep track of all memory allocations in the context
      Context->MemAllocs.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(*ResultPtr),
                                 std::forward_as_tuple(reinterpret_cast<ur_context_handle_t>(Context)));
    }
  } catch (const UsmAllocationException &Ex) {
    *ResultPtr = nullptr;
    return ur2piResult(Ex.getError());
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
#endif
}

pi_result piextUSMHostAlloc(void **ResultPtr, pi_context Context,
                            pi_usm_mem_properties *Properties, size_t Size,
                            pi_uint32 Alignment) {

  return pi2ur::piextUSMHostAlloc(ResultPtr,
                                  Context,
                                  Properties,
                                  Size,
                                  Alignment);
#if 0
  // L0 supports alignment up to 64KB and silently ignores higher values.
  // We flag alignment > 64KB as an invalid value.
  if (Alignment > 65536)
    return PI_ERROR_INVALID_VALUE;

  pi_platform Plt = Context->getPlatform();
  // If indirect access tracking is enabled then lock the mutex which is
  // guarding contexts container in the platform. This prevents new kernels from
  // being submitted in any context while we are in the process of allocating a
  // memory, this is needed to properly capture allocations by kernels with
  // indirect access. This lock also protects access to the context's data
  // structures. If indirect access tracking is not enabled then lock context
  // mutex to protect access to context's data structures.
  std::shared_lock<pi_shared_mutex> ContextLock(Context->Mutex,
                                                std::defer_lock);
  std::unique_lock<pi_shared_mutex> IndirectAccessTrackingLock(
      Plt->ContextsMutex, std::defer_lock);
  if (IndirectAccessTrackingEnabled) {
    IndirectAccessTrackingLock.lock();
    // We are going to defer memory release if there are kernels with indirect
    // access, that is why explicitly retain context to be sure that it is
    // released after all memory allocations in this context are released.
    PI_CALL(piContextRetain(Context));
  } else {
    ContextLock.lock();
  }

  if (!UseUSMAllocator ||
      // L0 spec says that allocation fails if Alignment != 2^n, in order to
      // keep the same behavior for the allocator, just call L0 API directly and
      // return the error code.
      ((Alignment & (Alignment - 1)) != 0)) {
    pi_result Res =
        ur2piResult(USMHostAllocImpl(ResultPtr,
                                     reinterpret_cast<ur_context_handle_t>(Context),                                     Properties,
                                     Size,
                                     Alignment));
    if (IndirectAccessTrackingEnabled) {
      // Keep track of all memory allocations in the context
      Context->MemAllocs.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(*ResultPtr),
                                 std::forward_as_tuple(reinterpret_cast<ur_context_handle_t>(Context)));
    }
    return Res;
  }

  // There is a single allocator for Host USM allocations, so we don't need to
  // find the allocator depending on context as we do for Shared and Device
  // allocations.
  try {
    *ResultPtr = Context->HostMemAllocContext->allocate(Size, Alignment);
    if (IndirectAccessTrackingEnabled) {
      // Keep track of all memory allocations in the context
      Context->MemAllocs.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(*ResultPtr),
                                 std::forward_as_tuple(reinterpret_cast<ur_context_handle_t>(Context)));
    }
  } catch (const UsmAllocationException &Ex) {
    *ResultPtr = nullptr;
    return ur2piResult(Ex.getError());
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
#endif
}

pi_result piextUSMFree(pi_context Context, void *Ptr) {

  return pi2ur::piextUSMFree(Context,
                             Ptr);
#if 0
  pi_platform Plt = Cntext->getPlatform();

  std::scoped_lock<pi_shared_mutex> Lock(
      IndirectAccessTrackingEnabled ? Plt->ContextsMutex : Context->Mutex);

  return ur2piResult(USMFreeHelper(reinterpret_cast<ur_context_handle_t>(Context),
                                   Ptr,
                                   true /* OwnZeMemHandle */));
#endif
}

pi_result piextKernelSetArgPointer(pi_kernel Kernel, pi_uint32 ArgIndex,
                                   size_t ArgSize, const void *ArgValue) {
  return pi2ur::piextKernelSetArgPointer(Kernel,
                                         ArgIndex,
                                         ArgSize,
                                         ArgValue);
#if 0
  PI_CALL(piKernelSetArg(Kernel, ArgIndex, ArgSize, ArgValue));
  return PI_SUCCESS;
#endif
}

/// USM Memset API
///
/// @param Queue is the queue to submit to
/// @param Ptr is the ptr to memset
/// @param Value is value to set.  It is interpreted as an 8-bit value and the
/// upper
///        24 bits are ignored
/// @param Count is the size in bytes to memset
/// @param NumEventsInWaitlist is the number of events to wait on
/// @param EventsWaitlist is an array of events to wait on
/// @param Event is the event that represents this operation
pi_result piextUSMEnqueueMemset(pi_queue Queue, void *Ptr, pi_int32 Value,
                                size_t Count, pi_uint32 NumEventsInWaitlist,
                                const pi_event *EventsWaitlist,
                                pi_event *Event) {
  return pi2ur::piextUSMEnqueueMemset(Queue,
                                      Ptr,
                                      Value,
                                      Count,
                                      NumEventsInWaitlist,
                                      EventsWaitlist,
                                      Event);
#if 0
  if (!Ptr) {
    return PI_ERROR_INVALID_VALUE;
  }

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  std::scoped_lock<pi_shared_mutex> Lock(Queue->Mutex);
  return enqueueMemFillHelper(
      // TODO: do we need a new command type for USM memset?
      PI_COMMAND_TYPE_MEM_BUFFER_FILL, Queue, Ptr,
      &Value, // It will be interpreted as an 8-bit value,
      1,      // which is indicated with this pattern_size==1
      Count, NumEventsInWaitlist, EventsWaitlist, Event);
#endif
}

pi_result piextUSMEnqueueMemcpy(pi_queue Queue, pi_bool Blocking, void *DstPtr,
                                const void *SrcPtr, size_t Size,
                                pi_uint32 NumEventsInWaitlist,
                                const pi_event *EventsWaitlist,
                                pi_event *Event) {

  return pi2ur::piextUSMEnqueueMemcpy(Queue,
                                      Blocking,
                                      DstPtr,
                                      SrcPtr,
                                      Size,
                                      NumEventsInWaitlist,
                                      EventsWaitlist,
                                      Event);
#if 0
  if (!DstPtr) {
    return PI_ERROR_INVALID_VALUE;
  }

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  std::scoped_lock<pi_shared_mutex> lock(Queue->Mutex);

  // Device to Device copies are found to execute slower on copy engine
  // (versus compute engine).
  bool PreferCopyEngine = !IsDevicePointer(Queue->Context, SrcPtr) ||
                          !IsDevicePointer(Queue->Context, DstPtr);

  // Temporary option added to use copy engine for D2D copy
  PreferCopyEngine |= UseCopyEngineForD2DCopy;

  return enqueueMemCopyHelper(
      // TODO: do we need a new command type for this?
      PI_COMMAND_TYPE_MEM_BUFFER_COPY, Queue, DstPtr, Blocking, Size, SrcPtr,
      NumEventsInWaitlist, EventsWaitlist, Event, PreferCopyEngine);
#endif
}

/// Hint to migrate memory to the device
///
/// @param Queue is the queue to submit to
/// @param Ptr points to the memory to migrate
/// @param Size is the number of bytes to migrate
/// @param Flags is a bitfield used to specify memory migration options
/// @param NumEventsInWaitlist is the number of events to wait on
/// @param EventsWaitlist is an array of events to wait on
/// @param Event is the event that represents this operation
pi_result piextUSMEnqueuePrefetch(pi_queue Queue, const void *Ptr, size_t Size,
                                  pi_usm_migration_flags Flags,
                                  pi_uint32 NumEventsInWaitList,
                                  const pi_event *EventWaitList,
                                  pi_event *OutEvent) {

  return pi2ur::piextUSMEnqueuePrefetch(Queue,
                                        Ptr,
                                        Size,
                                        Flags,
                                        NumEventsInWaitList,
                                        EventWaitList,
                                        OutEvent);

#if 0
  // flags is currently unused so fail if set
  PI_ASSERT(Flags == 0, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  // Lock automatically releases when this goes out of scope.
  std::scoped_lock<pi_shared_mutex> lock(Queue->Mutex);

  bool UseCopyEngine = false;

  // Please note that the following code should be run before the
  // subsequent getAvailableCommandList() call so that there is no
  // dead-lock from waiting unsubmitted events in an open batch.
  // The createAndRetainPiZeEventList() has the proper side-effect
  // of submitting batches with dependent events.
  //
  _pi_ze_event_list_t TmpWaitList;
  if (auto Res = ur2piResult(TmpWaitList.createAndRetainPiZeEventList(NumEventsInWaitList,
                                                                      reinterpret_cast<const ur_event_handle_t *>(EventWaitList),
                                                                      reinterpret_cast<ur_queue_handle_t>(Queue),
                                                                      UseCopyEngine)))
    return Res;

  // Get a new command list to be used on this call
  pi_command_list_ptr_t CommandList{};
  // TODO: Change UseCopyEngine argument to 'true' once L0 backend
  // support is added
  if (auto Res = ur2piResult(Queue->Context->getAvailableCommandList(reinterpret_cast<ur_queue_handle_t>(Queue), CommandList,
                                                         UseCopyEngine)))
    return Res;

  // TODO: do we need to create a unique command type for this?
  ze_event_handle_t ZeEvent = nullptr;
  pi_event InternalEvent;
  bool IsInternal = OutEvent == nullptr;
  pi_event *Event = OutEvent ? OutEvent : &InternalEvent;
  auto Res = ur2piResult(createEventAndAssociateQueue(reinterpret_cast<ur_queue_handle_t>(Queue),
                                                      reinterpret_cast<ur_event_handle_t *>(Event),
                                                      PI_COMMAND_TYPE_USER,
                                                      CommandList,
                                                      IsInternal));
  if (Res != PI_SUCCESS)
    return Res;
  ZeEvent = (*Event)->ZeEvent;
  (*Event)->WaitList = TmpWaitList;

  const auto &WaitList = (*Event)->WaitList;
  const auto &ZeCommandList = CommandList->first;
  if (WaitList.Length) {
    ZE_CALL(zeCommandListAppendWaitOnEvents,
            (ZeCommandList, WaitList.Length, WaitList.ZeEventList));
  }
  // TODO: figure out how to translate "flags"
  ZE_CALL(zeCommandListAppendMemoryPrefetch, (ZeCommandList, Ptr, Size));

  // TODO: Level Zero does not have a completion "event" with the prefetch API,
  // so manually add command to signal our event.
  ZE_CALL(zeCommandListAppendSignalEvent, (ZeCommandList, ZeEvent));

  if (auto Res = ur2piResult(Queue->executeCommandList(CommandList, false)))
    return Res;

  return PI_SUCCESS;
#endif
}

/// USM memadvise API to govern behavior of automatic migration mechanisms
///
/// @param Queue is the queue to submit to
/// @param Ptr is the data to be advised
/// @param Length is the size in bytes of the meory to advise
/// @param Advice is device specific advice
/// @param Event is the event that represents this operation
///
pi_result piextUSMEnqueueMemAdvise(pi_queue Queue, const void *Ptr,
                                   size_t Length, pi_mem_advice Advice,
                                   pi_event *OutEvent) {
  
  return pi2ur::piextUSMEnqueueMemAdvise(Queue,
                                         Ptr,
                                         Length,
                                         Advice,
                                         OutEvent);
#if 0
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  // Lock automatically releases when this goes out of scope.
  std::scoped_lock<pi_shared_mutex> lock(Queue->Mutex);

  auto ZeAdvice = pi_cast<ze_memory_advice_t>(Advice);

  bool UseCopyEngine = false;

  _pi_ze_event_list_t TmpWaitList;
  if (auto Res = ur2piResult(TmpWaitList.createAndRetainPiZeEventList(0,
                                                                      nullptr,
                                                                      reinterpret_cast<ur_queue_handle_t>(Queue),
                                                                      UseCopyEngine)))
    return Res;

  // Get a new command list to be used on this call
  pi_command_list_ptr_t CommandList{};
  // UseCopyEngine is set to 'false' here.
  // TODO: Additional analysis is required to check if this operation will
  // run faster on copy engines.
  if (auto Res = ur2piResult(Queue->Context->getAvailableCommandList(reinterpret_cast<ur_queue_handle_t>(Queue), CommandList,
                                                         UseCopyEngine)))
    return Res;

  // TODO: do we need to create a unique command type for this?
  ze_event_handle_t ZeEvent = nullptr;
  pi_event InternalEvent;
  bool IsInternal = OutEvent == nullptr;
  pi_event *Event = OutEvent ? OutEvent : &InternalEvent;
  auto Res = ur2piResult(createEventAndAssociateQueue(reinterpret_cast<ur_queue_handle_t>(Queue),
                                                      reinterpret_cast<ur_event_handle_t *>(Event),
                                                      PI_COMMAND_TYPE_USER,
                                                      CommandList, IsInternal));
  if (Res != PI_SUCCESS)
    return Res;
  ZeEvent = (*Event)->ZeEvent;
  (*Event)->WaitList = TmpWaitList;

  const auto &ZeCommandList = CommandList->first;
  const auto &WaitList = (*Event)->WaitList;

  if (WaitList.Length) {
    ZE_CALL(zeCommandListAppendWaitOnEvents,
            (ZeCommandList, WaitList.Length, WaitList.ZeEventList));
  }

  ZE_CALL(zeCommandListAppendMemAdvise,
          (ZeCommandList, Queue->Device->ZeDevice, Ptr, Length, ZeAdvice));

  // TODO: Level Zero does not have a completion "event" with the advise API,
  // so manually add command to signal our event.
  ZE_CALL(zeCommandListAppendSignalEvent, (ZeCommandList, ZeEvent));

  Queue->executeCommandList(CommandList, false);

  return PI_SUCCESS;
#endif
}

/// USM 2D Fill API
///
/// \param queue is the queue to submit to
/// \param ptr is the ptr to fill
/// \param pitch is the total width of the destination memory including padding
/// \param pattern is a pointer with the bytes of the pattern to set
/// \param pattern_size is the size in bytes of the pattern
/// \param width is width in bytes of each row to fill
/// \param height is height the columns to fill
/// \param num_events_in_waitlist is the number of events to wait on
/// \param events_waitlist is an array of events to wait on
/// \param event is the event that represents this operation
__SYCL_EXPORT pi_result piextUSMEnqueueFill2D(pi_queue Queue, void *Ptr,
                                              size_t Pitch, size_t PatternSize,
                                              const void *Pattern, size_t Width,
                                              size_t Height,
                                              pi_uint32 NumEventsWaitList,
                                              const pi_event *EventsWaitList,
                                              pi_event *Event) {

  return pi2ur::piextUSMEnqueueFill2D(Queue,
                                      Ptr,
                                      Pitch,
                                      PatternSize,
                                      Pattern,
                                      Width,
                                      Height,
                                      NumEventsWaitList,
                                      EventsWaitList,
                                      Event);                                                

#if 0
  std::ignore = Queue;
  std::ignore = Ptr;
  std::ignore = Pitch;
  std::ignore = PatternSize;
  std::ignore = Pattern;
  std::ignore = Width;
  std::ignore = Height;
  std::ignore = NumEventsWaitList;
  std::ignore = EventsWaitList;
  std::ignore = Event;
  die("piextUSMEnqueueFill2D: not implemented");
  return {};
#endif
}

/// USM 2D Memset API
///
/// \param queue is the queue to submit to
/// \param ptr is the ptr to fill
/// \param pitch is the total width of the destination memory including padding
/// \param pattern is a pointer with the bytes of the pattern to set
/// \param pattern_size is the size in bytes of the pattern
/// \param width is width in bytes of each row to fill
/// \param height is height the columns to fill
/// \param num_events_in_waitlist is the number of events to wait on
/// \param events_waitlist is an array of events to wait on
/// \param event is the event that represents this operation
__SYCL_EXPORT pi_result piextUSMEnqueueMemset2D(
    pi_queue Queue, void *Ptr, size_t Pitch, int Value, size_t Width,
    size_t Height, pi_uint32 NumEventsWaitList,
    const pi_event *EventsWaitlist, pi_event *Event) {

    return pi2ur::piextUSMEnqueueMemset2D(Queue,
                                          Ptr,
                                          Pitch,
                                          Value,
                                          Width,
                                          Height,
                                          NumEventsWaitList,
                                          EventsWaitlist,
                                          Event);
#if 0
  std::ignore = queue;
  std::ignore = ptr;
  std::ignore = pitch;
  std::ignore = value;
  std::ignore = width;
  std::ignore = height;
  std::ignore = num_events_in_waitlist;
  std::ignore = events_waitlist;
  std::ignore = event;
  die("piextUSMEnqueueMemset2D: not implemented");
  return {};
#endif
}

/// USM 2D Memcpy API
///
/// \param queue is the queue to submit to
/// \param blocking is whether this operation should block the host
/// \param dst_ptr is the location the data will be copied
/// \param dst_pitch is the total width of the destination memory including
/// padding
/// \param src_ptr is the data to be copied
/// \param dst_pitch is the total width of the source memory including padding
/// \param width is width in bytes of each row to be copied
/// \param height is height the columns to be copied
/// \param num_events_in_waitlist is the number of events to wait on
/// \param events_waitlist is an array of events to wait on
/// \param event is the event that represents this operation
__SYCL_EXPORT pi_result piextUSMEnqueueMemcpy2D(
    pi_queue Queue, pi_bool Blocking, void *DstPtr, size_t DstPitch,
    const void *SrcPtr, size_t SrcPitch, size_t Width, size_t Height,
    pi_uint32 NumEventsInWaitlist, const pi_event *EventWaitlist,
    pi_event *Event) {

  return pi2ur::piextUSMEnqueueMemcpy2D(Queue,
                                        Blocking,
                                        DstPtr,
                                        DstPitch,
                                        SrcPtr,
                                        SrcPitch,
                                        Width,
                                        Height,
                                        NumEventsInWaitlist,
                                        EventWaitlist,
                                        Event);
}

/// API to query information about USM allocated pointers.
/// Valid Queries:
///   PI_MEM_ALLOC_TYPE returns host/device/shared pi_usm_type value
///   PI_MEM_ALLOC_BASE_PTR returns the base ptr of an allocation if
///                         the queried pointer fell inside an allocation.
///                         Result must fit in void *
///   PI_MEM_ALLOC_SIZE returns how big the queried pointer's
///                     allocation is in bytes. Result is a size_t.
///   PI_MEM_ALLOC_DEVICE returns the pi_device this was allocated against
///
/// @param Context is the pi_context
/// @param Ptr is the pointer to query
/// @param ParamName is the type of query to perform
/// @param ParamValueSize is the size of the result in bytes
/// @param ParamValue is the result
/// @param ParamValueRet is how many bytes were written
pi_result piextUSMGetMemAllocInfo(pi_context Context, const void *Ptr,
                                  pi_mem_alloc_info ParamName,
                                  size_t ParamValueSize, void *ParamValue,
                                  size_t *ParamValueSizeRet) {
  return pi2ur::piextUSMGetMemAllocInfo(Context,
                                        Ptr,
                                        ParamName,
                                        ParamValueSize,
                                        ParamValue,
                                        ParamValueSizeRet);
#if 0
  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);

  ze_device_handle_t ZeDeviceHandle;
  ZeStruct<ze_memory_allocation_properties_t> ZeMemoryAllocationProperties;

  ZE_CALL(zeMemGetAllocProperties,
          (Context->ZeContext, Ptr, &ZeMemoryAllocationProperties,
           &ZeDeviceHandle));

  ReturnHelper ReturnValue(ParamValueSize, ParamValue, ParamValueSizeRet);
  switch (ParamName) {
  case PI_MEM_ALLOC_TYPE: {
    pi_usm_type MemAllocaType;
    switch (ZeMemoryAllocationProperties.type) {
    case ZE_MEMORY_TYPE_UNKNOWN:
      MemAllocaType = PI_MEM_TYPE_UNKNOWN;
      break;
    case ZE_MEMORY_TYPE_HOST:
      MemAllocaType = PI_MEM_TYPE_HOST;
      break;
    case ZE_MEMORY_TYPE_DEVICE:
      MemAllocaType = PI_MEM_TYPE_DEVICE;
      break;
    case ZE_MEMORY_TYPE_SHARED:
      MemAllocaType = PI_MEM_TYPE_SHARED;
      break;
    default:
      zePrint("piextUSMGetMemAllocInfo: unexpected usm memory type\n");
      return PI_ERROR_INVALID_VALUE;
    }
    return ReturnValue(MemAllocaType);
  }
  case PI_MEM_ALLOC_DEVICE:
    if (ZeDeviceHandle) {
      auto Platform = Context->getPlatform();
      auto Device = Platform->getDeviceFromNativeHandle(ZeDeviceHandle);
      return Device ? ReturnValue(Device) : PI_ERROR_INVALID_VALUE;
    } else {
      return PI_ERROR_INVALID_VALUE;
    }
  case PI_MEM_ALLOC_BASE_PTR: {
    void *Base;
    ZE_CALL(zeMemGetAddressRange, (Context->ZeContext, Ptr, &Base, nullptr));
    return ReturnValue(Base);
  }
  case PI_MEM_ALLOC_SIZE: {
    size_t Size;
    ZE_CALL(zeMemGetAddressRange, (Context->ZeContext, Ptr, nullptr, &Size));
    return ReturnValue(Size);
  }
  default:
    zePrint("piextUSMGetMemAllocInfo: unsupported ParamName\n");
    return PI_ERROR_INVALID_VALUE;
  }
  return PI_SUCCESS;
#endif
}

/// API for writing data from host to a device global variable.
///
/// \param Queue is the queue
/// \param Program is the program containing the device global variable
/// \param Name is the unique identifier for the device global variable
/// \param BlockingWrite is true if the write should block
/// \param Count is the number of bytes to copy
/// \param Offset is the byte offset into the device global variable to start
/// copying
/// \param Src is a pointer to where the data must be copied from
/// \param NumEventsInWaitList is a number of events in the wait list
/// \param EventWaitList is the wait list
/// \param Event is the resulting event
pi_result piextEnqueueDeviceGlobalVariableWrite(
    pi_queue Queue, pi_program Program, const char *Name, pi_bool BlockingWrite,
    size_t Count, size_t Offset, const void *Src, pi_uint32 NumEventsInWaitList,
    const pi_event *EventsWaitList, pi_event *Event) {
  return pi2ur::piextEnqueueDeviceGlobalVariableWrite(Queue,
                                                      Program,
                                                      Name,
                                                      BlockingWrite,
                                                      Count,
                                                      Offset,
                                                      Src,
                                                      NumEventsInWaitList,
                                                      EventsWaitList,Event);
}

/// API reading data from a device global variable to host.
///
/// \param Queue is the queue
/// \param Program is the program containing the device global variable
/// \param Name is the unique identifier for the device global variable
/// \param BlockingRead is true if the read should block
/// \param Count is the number of bytes to copy
/// \param Offset is the byte offset into the device global variable to start
/// copying
/// \param Dst is a pointer to where the data must be copied to
/// \param NumEventsInWaitList is a number of events in the wait list
/// \param EventWaitList is the wait list
/// \param Event is the resulting event
pi_result piextEnqueueDeviceGlobalVariableRead(
    pi_queue Queue, pi_program Program, const char *Name, pi_bool BlockingRead,
    size_t Count, size_t Offset, void *Dst, pi_uint32 NumEventsInWaitList,
    const pi_event *EventsWaitList, pi_event *Event) {

  return pi2ur::piextEnqueueDeviceGlobalVariableRead(Queue,
                                                     Program,
                                                     Name,
                                                     BlockingRead,
                                                     Count,
                                                     Offset,
                                                     Dst,
                                                     NumEventsInWaitList,
                                                     EventsWaitList,
                                                     Event);

  return PI_SUCCESS;
}

pi_result piKernelSetExecInfo(pi_kernel Kernel, pi_kernel_exec_info ParamName,
                              size_t ParamValueSize, const void *ParamValue) {

  return pi2ur::piKernelSetExecInfo(Kernel,
                                    ParamName,
                                    ParamValueSize,
                                    ParamValue);
}

pi_result piextProgramSetSpecializationConstant(pi_program Prog,
                                                pi_uint32 SpecID, size_t Size,
                                                const void *SpecValue) {
  return pi2ur::piextProgramSetSpecializationConstant(Prog,
                                                      SpecID,
                                                      Size,
                                                      SpecValue);
}

const char SupportedVersion[] = _PI_LEVEL_ZERO_PLUGIN_VERSION_STRING;

pi_result piPluginInit(pi_plugin *PluginInit) { // missing
  PI_ASSERT(PluginInit, PI_ERROR_INVALID_VALUE);

  // Check that the major version matches in PiVersion and SupportedVersion
  _PI_PLUGIN_VERSION_CHECK(PluginInit->PiVersion, SupportedVersion);

  // TODO: handle versioning/targets properly.
  size_t PluginVersionSize = sizeof(PluginInit->PluginVersion);

  PI_ASSERT(strlen(_PI_LEVEL_ZERO_PLUGIN_VERSION_STRING) < PluginVersionSize,
            PI_ERROR_INVALID_VALUE);

  strncpy(PluginInit->PluginVersion, SupportedVersion, PluginVersionSize);

#define _PI_API(api)                                                           \
  (PluginInit->PiFunctionTable).api = (decltype(&::api))(&api);
#include <sycl/detail/pi.def>

  enableZeTracing();
  return PI_SUCCESS;
}

pi_result piextPluginGetOpaqueData(void *opaque_data_param,
                                   void **opaque_data_return) {
return pi2ur::piextPluginGetOpaqueData(opaque_data_param,
                                       opaque_data_return);
}

// SYCL RT calls this api to notify the end of plugin lifetime.
// It can include all the jobs to tear down resources before
// the plugin is unloaded from memory.
pi_result piTearDown(void *PluginParameter) {
  (void)PluginParameter;
  bool LeakFound = false;
  // reclaim pi_platform objects here since we don't have piPlatformRelease.
  for (pi_platform Platform : *PiPlatformsCache) {
    delete Platform;
  }
  delete PiPlatformsCache;
  delete PiPlatformsCacheMutex;

  // Print the balance of various create/destroy native calls.
  // The idea is to verify if the number of create(+) and destroy(-) calls are
  // matched.
  if (ZeDebug & ZE_DEBUG_CALL_COUNT) {
    // clang-format off
    //
    // The format of this table is such that each row accounts for a
    // specific type of objects, and all elements in the raw except the last
    // one are allocating objects of that type, while the last element is known
    // to deallocate objects of that type.
    //
    std::vector<std::vector<const char *>> CreateDestroySet = {
      {"zeContextCreate",      "zeContextDestroy"},
      {"zeCommandQueueCreate", "zeCommandQueueDestroy"},
      {"zeModuleCreate",       "zeModuleDestroy"},
      {"zeKernelCreate",       "zeKernelDestroy"},
      {"zeEventPoolCreate",    "zeEventPoolDestroy"},
      {"zeCommandListCreateImmediate", "zeCommandListCreate", "zeCommandListDestroy"},
      {"zeEventCreate",        "zeEventDestroy"},
      {"zeFenceCreate",        "zeFenceDestroy"},
      {"zeImageCreate",        "zeImageDestroy"},
      {"zeSamplerCreate",      "zeSamplerDestroy"},
      {"zeMemAllocDevice", "zeMemAllocHost", "zeMemAllocShared", "zeMemFree"},
    };

    // A sample output aimed below is this:
    // ------------------------------------------------------------------------
    //                zeContextCreate = 1     \--->        zeContextDestroy = 1
    //           zeCommandQueueCreate = 1     \--->   zeCommandQueueDestroy = 1
    //                 zeModuleCreate = 1     \--->         zeModuleDestroy = 1
    //                 zeKernelCreate = 1     \--->         zeKernelDestroy = 1
    //              zeEventPoolCreate = 1     \--->      zeEventPoolDestroy = 1
    //   zeCommandListCreateImmediate = 1     |
    //            zeCommandListCreate = 1     \--->    zeCommandListDestroy = 1  ---> LEAK = 1
    //                  zeEventCreate = 2     \--->          zeEventDestroy = 2
    //                  zeFenceCreate = 1     \--->          zeFenceDestroy = 1
    //                  zeImageCreate = 0     \--->          zeImageDestroy = 0
    //                zeSamplerCreate = 0     \--->        zeSamplerDestroy = 0
    //               zeMemAllocDevice = 0     |
    //                 zeMemAllocHost = 1     |
    //               zeMemAllocShared = 0     \--->               zeMemFree = 1
    //
    // clang-format on

    fprintf(stderr, "ZE_DEBUG=%d: check balance of create/destroy calls\n",
            ZE_DEBUG_CALL_COUNT);
    fprintf(stderr,
            "----------------------------------------------------------\n");
    for (const auto &Row : CreateDestroySet) {
      int diff = 0;
      for (auto I = Row.begin(); I != Row.end();) {
        const char *ZeName = *I;
        const auto &ZeCount = (*ZeCallCount)[*I];

        bool First = (I == Row.begin());
        bool Last = (++I == Row.end());

        if (Last) {
          fprintf(stderr, " \\--->");
          diff -= ZeCount;
        } else {
          diff += ZeCount;
          if (!First) {
            fprintf(stderr, " | \n");
          }
        }

        fprintf(stderr, "%30s = %-5d", ZeName, ZeCount);
      }

      if (diff) {
        LeakFound = true;
        fprintf(stderr, " ---> LEAK = %d", diff);
      }
      fprintf(stderr, "\n");
    }

    ZeCallCount->clear();
    delete ZeCallCount;
    ZeCallCount = nullptr;
  }
  if (LeakFound)
    return PI_ERROR_INVALID_MEM_OBJECT;

  disableZeTracing();
  return PI_SUCCESS;
}

#if 0
// Buffer constructor
_pi_buffer::_pi_buffer(ur_context_handle_t Context, size_t Size, char *HostPtr,
            bool ImportedHostPtr = false)
    : _ur_mem_handle_t(Context), Size(Size), SubBuffer{nullptr, 0} {

  // We treat integrated devices (physical memory shared with the CPU)
  // differently from discrete devices (those with distinct memories).
  // For integrated devices, allocating the buffer in the host memory
  // enables automatic access from the device, and makes copying
  // unnecessary in the map/unmap operations. This improves performance.
  OnHost = Context->Devices.size() == 1 &&
            Context->Devices[0]->ZeDeviceProperties->flags &
                ZE_DEVICE_PROPERTY_FLAG_INTEGRATED;

  // Fill the host allocation data.
  if (HostPtr) {
    MapHostPtr = HostPtr;
    // If this host ptr is imported to USM then use this as a host
    // allocation for this buffer.
    if (ImportedHostPtr) {
      Allocations[nullptr].ZeHandle = HostPtr;
      Allocations[nullptr].Valid = true;
      Allocations[nullptr].ReleaseAction = _pi_buffer::allocation_t::unimport;
    }
  }

  // This initialization does not end up with any valid allocation yet.
  LastDeviceWithValidAllocation = nullptr;
}
#endif

#if 0
// Interop-buffer constructor
_pi_buffer::_pi_buffer(ur_context_handle_t Context,
                       size_t Size,
                       ur_device_handle_t Device,
                       char *ZeMemHandle,
                       bool OwnZeMemHandle)
    : _ur_mem_handle_t(Context), Size(Size), SubBuffer{nullptr, 0} {

  // Device == nullptr means host allocation
  Allocations[Device].ZeHandle = ZeMemHandle;
  Allocations[Device].Valid = true;
  Allocations[Device].ReleaseAction =
      OwnZeMemHandle ? allocation_t::free_native : allocation_t::keep;

  // Check if this buffer can always stay on host
  OnHost = false;
  if (!Device) { // Host allocation
    if (Context->Devices.size() == 1 &&
        Context->Devices[0]->ZeDeviceProperties->flags &
            ZE_DEVICE_PROPERTY_FLAG_INTEGRATED) {
      OnHost = true;
      MapHostPtr = ZeMemHandle; // map to this allocation
    }
  }
  LastDeviceWithValidAllocation = Device;
}

pi_result _pi_buffer::getZeHandlePtr(char **&ZeHandlePtr,
                                     access_mode_t AccessMode,
                                     pi_device Device) {
  char *ZeHandle;
  PI_CALL(getZeHandle(ZeHandle, AccessMode, Device));
  ZeHandlePtr = &Allocations[Device].ZeHandle;
  return PI_SUCCESS;
}

size_t _pi_buffer::getAlignment() const {
  // Choose an alignment that is at most 64 and is the next power of 2
  // for sizes less than 64.
  auto Alignment = Size;
  if (Alignment > 32UL)
    Alignment = 64UL;
  else if (Alignment > 16UL)
    Alignment = 32UL;
  else if (Alignment > 8UL)
    Alignment = 16UL;
  else if (Alignment > 4UL)
    Alignment = 8UL;
  else if (Alignment > 2UL)
    Alignment = 4UL;
  else if (Alignment > 1UL)
    Alignment = 2UL;
  else
    Alignment = 1UL;
  return Alignment;
}
#endif

#if 0
// Buffer constructor
_pi_buffer::_pi_buffer(pi_context Context, size_t Size, char *HostPtr,
            bool ImportedHostPtr = false)
    : _pi_mem(Context), Size(Size), SubBuffer{nullptr, 0} {

  // We treat integrated devices (physical memory shared with the CPU)
  // differently from discrete devices (those with distinct memories).
  // For integrated devices, allocating the buffer in the host memory
  // enables automatic access from the device, and makes copying
  // unnecessary in the map/unmap operations. This improves performance.
  OnHost = Context->Devices.size() == 1 &&
            Context->Devices[0]->ZeDeviceProperties->flags &
                ZE_DEVICE_PROPERTY_FLAG_INTEGRATED;

  // Fill the host allocation data.
  if (HostPtr) {
    MapHostPtr = HostPtr;
    // If this host ptr is imported to USM then use this as a host
    // allocation for this buffer.
    if (ImportedHostPtr) {
      Allocations[nullptr].ZeHandle = HostPtr;
      Allocations[nullptr].Valid = true;
      Allocations[nullptr].ReleaseAction = _pi_buffer::allocation_t::unimport;
    }
  }

  // This initialization does not end up with any valid allocation yet.
  LastDeviceWithValidAllocation = nullptr;
}

// Interop-buffer constructor
_pi_buffer::_pi_buffer(pi_context Context, size_t Size, pi_device Device,
            char *ZeMemHandle, bool OwnZeMemHandle)
    : _pi_mem(Context), Size(Size), SubBuffer{nullptr, 0} {

  // Device == nullptr means host allocation
  Allocations[Device].ZeHandle = ZeMemHandle;
  Allocations[Device].Valid = true;
  Allocations[Device].ReleaseAction =
      OwnZeMemHandle ? allocation_t::free_native : allocation_t::keep;

  // Check if this buffer can always stay on host
  OnHost = false;
  if (!Device) { // Host allocation
    if (Context->Devices.size() == 1 &&
        Context->Devices[0]->ZeDeviceProperties->flags &
            ZE_DEVICE_PROPERTY_FLAG_INTEGRATED) {
      OnHost = true;
      MapHostPtr = ZeMemHandle; // map to this allocation
    }
  }
  LastDeviceWithValidAllocation = Device;
}

pi_result _pi_buffer::getZeHandlePtr(char **&ZeHandlePtr,
                                     access_mode_t AccessMode,
                                     pi_device Device) {
  char *ZeHandle;
  PI_CALL(getZeHandle(ZeHandle, AccessMode, Device));
  ZeHandlePtr = &Allocations[Device].ZeHandle;
  return PI_SUCCESS;
}

size_t _pi_buffer::getAlignment() const {
  // Choose an alignment that is at most 64 and is the next power of 2
  // for sizes less than 64.
  auto Alignment = Size;
  if (Alignment > 32UL)
    Alignment = 64UL;
  else if (Alignment > 16UL)
    Alignment = 32UL;
  else if (Alignment > 8UL)
    Alignment = 16UL;
  else if (Alignment > 4UL)
    Alignment = 8UL;
  else if (Alignment > 2UL)
    Alignment = 4UL;
  else if (Alignment > 1UL)
    Alignment = 2UL;
  else
    Alignment = 1UL;
  return Alignment;
}
#endif

#if 0
pi_result _pi_buffer::getZeHandle(char *&ZeHandle, access_mode_t AccessMode,
                                  pi_device Device) {

  // NOTE: There might be no valid allocation at all yet and we get
  // here from piEnqueueKernelLaunch that would be doing the buffer
  // initialization. In this case the Device is not null as kernel
  // launch is always on a specific device.
  if (!Device)
    Device = LastDeviceWithValidAllocation;
  // If the device is still not selected then use the first one in
  // the context of the buffer.
  if (!Device)
    Device = Context->Devices[0];

  auto &Allocation = Allocations[Device];

  // Sub-buffers don't maintain own allocations but rely on parent buffer.
  if (isSubBuffer()) {
    PI_CALL(SubBuffer.Parent->getZeHandle(ZeHandle, AccessMode, Device));
    ZeHandle += SubBuffer.Origin;
    // Still store the allocation info in the PI sub-buffer for
    // getZeHandlePtr to work. At least zeKernelSetArgumentValue needs to
    // be given a pointer to the allocation handle rather than its value.
    //
    Allocation.ZeHandle = ZeHandle;
    Allocation.ReleaseAction = allocation_t::keep;
    LastDeviceWithValidAllocation = Device;
    return PI_SUCCESS;
  }

  // First handle case where the buffer is represented by only
  // a single host allocation.
  if (OnHost) {
    auto &HostAllocation = Allocations[nullptr];
    // The host allocation may already exists, e.g. with imported
    // host ptr, or in case of interop buffer.
    if (!HostAllocation.ZeHandle) {
      if (USMAllocatorConfigInstance.EnableBuffers) {
        HostAllocation.ReleaseAction = allocation_t::free;
        PI_CALL(piextUSMHostAlloc(pi_cast<void **>(&ZeHandle), Context, nullptr,
                                  Size, getAlignment()));
      } else {
        HostAllocation.ReleaseAction = allocation_t::free_native;
        PI_CALL(
            ZeHostMemAllocHelper(pi_cast<void **>(&ZeHandle), Context, Size));
      }
      HostAllocation.ZeHandle = ZeHandle;
      HostAllocation.Valid = true;
    }
    Allocation = HostAllocation;
    Allocation.ReleaseAction = allocation_t::keep;
    ZeHandle = Allocation.ZeHandle;
    LastDeviceWithValidAllocation = Device;
    return PI_SUCCESS;
  }
  // Reads user setting on how to deal with buffers in contexts where
  // all devices have the same root-device. Returns "true" if the
  // preference is to have allocate on each [sub-]device and migrate
  // normally (copy) to other sub-devices as needed. Returns "false"
  // if the preference is to have single root-device allocations
  // serve the needs of all [sub-]devices, meaning potentially more
  // cross-tile traffic.
  //
  static const bool SingleRootDeviceBufferMigration = [] {
    const char *EnvStr =
        std::getenv("SYCL_PI_LEVEL_ZERO_SINGLE_ROOT_DEVICE_BUFFER_MIGRATION");
    if (EnvStr)
      return (std::stoi(EnvStr) != 0);
    // The default is to migrate normally, which may not always be the
    // best option (depends on buffer access patterns), but is an
    // overall win on the set of the available benchmarks.
    return true;
  }();

  // Peform actual device allocation as needed.
  if (!Allocation.ZeHandle) {
    if (!SingleRootDeviceBufferMigration && Context->SingleRootDevice &&
        Context->SingleRootDevice != Device) {
      // If all devices in the context are sub-devices of the same device
      // then we reuse root-device allocation by all sub-devices in the
      // context.
      // TODO: we can probably generalize this and share root-device
      //       allocations by its own sub-devices even if not all other
      //       devices in the context have the same root.
      PI_CALL(getZeHandle(ZeHandle, AccessMode, Context->SingleRootDevice));
      Allocation.ReleaseAction = allocation_t::keep;
      Allocation.ZeHandle = ZeHandle;
      Allocation.Valid = true;
      return PI_SUCCESS;
    } else { // Create device allocation
      if (USMAllocatorConfigInstance.EnableBuffers) {
        Allocation.ReleaseAction = allocation_t::free;
        PI_CALL(piextUSMDeviceAlloc(pi_cast<void **>(&ZeHandle), Context,
                                    Device, nullptr, Size, getAlignment()));
      } else {
        Allocation.ReleaseAction = allocation_t::free_native;
        PI_CALL(ZeDeviceMemAllocHelper(pi_cast<void **>(&ZeHandle), Context,
                                       Device, Size));
      }
    }
    Allocation.ZeHandle = ZeHandle;
  } else {
    ZeHandle = Allocation.ZeHandle;
  }

  // If some prior access invalidated this allocation then make it valid again.
  if (!Allocation.Valid) {
    // LastDeviceWithValidAllocation should always have valid allocation.
    if (Device == LastDeviceWithValidAllocation)
      die("getZeHandle: last used allocation is not valid");

    // For write-only access the allocation contents is not going to be used.
    // So don't do anything to make it "valid".
    bool NeedCopy = AccessMode != _pi_mem::write_only;
    // It's also possible that the buffer doesn't have a valid allocation
    // yet presumably when it is passed to a kernel that will perform
    // it's intialization.
    if (NeedCopy && !LastDeviceWithValidAllocation) {
      NeedCopy = false;
    }
    char *ZeHandleSrc = nullptr;
    if (NeedCopy) {
      PI_CALL(getZeHandle(ZeHandleSrc, _pi_mem::read_only,
                          LastDeviceWithValidAllocation));
      // It's possible with the single root-device contexts that
      // the buffer is represented by the single root-device
      // allocation and then skip the copy to itself.
      if (ZeHandleSrc == ZeHandle)
        NeedCopy = false;
    }

    if (NeedCopy) {
      // Copy valid buffer data to this allocation.
      // TODO: see if we should better use peer's device allocation used
      // directly, if that capability is reported with zeDeviceCanAccessPeer,
      // instead of maintaining a separate allocation and performing
      // explciit copies.
      //
      // zeCommandListAppendMemoryCopy must not be called from simultaneous
      // threads with the same command list handle, so we need exclusive lock.
      ze_bool_t P2P = false;
      ZE_CALL(
          zeDeviceCanAccessPeer,
          (Device->ZeDevice, LastDeviceWithValidAllocation->ZeDevice, &P2P));
      if (!P2P) {
        // P2P copy is not possible, so copy through the host.
        auto &HostAllocation = Allocations[nullptr];
        // The host allocation may already exists, e.g. with imported
        // host ptr, or in case of interop buffer.
        if (!HostAllocation.ZeHandle) {
          void *ZeHandleHost;
          if (USMAllocatorConfigInstance.EnableBuffers) {
            HostAllocation.ReleaseAction = allocation_t::free;
            PI_CALL(piextUSMHostAlloc(&ZeHandleHost, Context, nullptr, Size,
                                      getAlignment()));
          } else {
            HostAllocation.ReleaseAction = allocation_t::free_native;
            PI_CALL(ZeHostMemAllocHelper(&ZeHandleHost, Context, Size));
          }
          HostAllocation.ZeHandle = pi_cast<char *>(ZeHandleHost);
          HostAllocation.Valid = false;
        }
        std::scoped_lock<pi_mutex> Lock(Context->ImmediateCommandListMutex);
        if (!HostAllocation.Valid) {
          ZE_CALL(zeCommandListAppendMemoryCopy,
                  (Context->ZeCommandListInit,
                   HostAllocation.ZeHandle /* Dst */, ZeHandleSrc, Size,
                   nullptr, 0, nullptr));
          // Mark the host allocation data  as valid so it can be reused.
          // It will be invalidated below if the current access is not
          // read-only.
          HostAllocation.Valid = true;
        }
        ZE_CALL(zeCommandListAppendMemoryCopy,
                (Context->ZeCommandListInit, ZeHandle /* Dst */,
                 HostAllocation.ZeHandle, Size, nullptr, 0, nullptr));
      } else {
        // Perform P2P copy.
        std::scoped_lock<pi_mutex> Lock(Context->ImmediateCommandListMutex);
        ZE_CALL(zeCommandListAppendMemoryCopy,
                (Context->ZeCommandListInit, ZeHandle /* Dst */, ZeHandleSrc,
                 Size, nullptr, 0, nullptr));
      }
    }
    Allocation.Valid = true;
    LastDeviceWithValidAllocation = Device;
  }

  // Invalidate other allocations that would become not valid if
  // this access is not read-only.
  if (AccessMode != _pi_mem::read_only) {
    for (auto &Alloc : Allocations) {
      if (Alloc.first != LastDeviceWithValidAllocation)
        Alloc.second.Valid = false;
    }
  }

  zePrint("getZeHandle(pi_device{%p}) = %p\n", (void *)Device,
          (void *)Allocation.ZeHandle);
  return PI_SUCCESS;
}
#endif

#if 0
pi_result _pi_buffer::free() {
  for (auto &Alloc : Allocations) {
    auto &ZeHandle = Alloc.second.ZeHandle;
    // It is possible that the real allocation wasn't made if the buffer
    // wasn't really used in this location.
    if (!ZeHandle)
      continue;

    switch (Alloc.second.ReleaseAction) {
    case allocation_t::keep:
      break;
    case allocation_t::free: {
      pi_platform Plt = Context->getPlatform();
      std::scoped_lock<pi_shared_mutex> Lock(
          IndirectAccessTrackingEnabled ? Plt->ContextsMutex : Context->Mutex);

      PI_CALL(ur2piResult(USMFreeHelper(reinterpret_cast<ur_context_handle_t>(Context),
                          ZeHandle,
                          true)));
      break;
    }
    case allocation_t::free_native:
      PI_CALL(ur2piResult(ZeMemFreeHelper(reinterpret_cast<ur_context_handle_t>(Context), ZeHandle, true)));
      break;
    case allocation_t::unimport:
      ZeUSMImport.doZeUSMRelease(Context->getPlatform()->ZeDriver, ZeHandle);
      break;
    default:
      die("_pi_buffer::free(): Unhandled release action");
    }
    ZeHandle = nullptr; // don't leave hanging pointers
  }
  return PI_SUCCESS;
}
#endif

pi_result piGetDeviceAndHostTimer(pi_device Device, uint64_t *DeviceTime,
                                  uint64_t *HostTime) {
  return pi2ur::piGetDeviceAndHostTimer(Device, DeviceTime, HostTime);
}
} // extern "C"
