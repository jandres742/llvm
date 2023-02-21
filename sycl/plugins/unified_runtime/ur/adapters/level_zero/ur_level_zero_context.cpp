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
#include "ur_level_zero_context.hpp"
#include <ur_bindings.hpp>

UR_APIEXPORT ur_result_t UR_APICALL urContextCreate(
    uint32_t DeviceCount, ///< [in] the number of devices given in phDevices
    ur_device_handle_t
        *phDevices, ///< [in][range(0, DeviceCount)] array of handle of devices.
    ur_context_handle_t
        *phContext ///< [out] pointer to handle of context object created
) {
  ur_platform_handle_t Platform = phDevices[0]->Platform;
  ZeStruct<ze_context_desc_t> ContextDesc {};

  printf("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

  ze_context_handle_t ZeContext {};
  ZE2UR_CALL(zeContextCreate, (Platform->ZeDriver, &ContextDesc, &ZeContext));
  try {
    _ur_context_handle_t *Context = new _ur_context_handle_t(ZeContext,
                                                             DeviceCount,
                                                             const_cast<const ur_device_handle_t *>(phDevices),
                                                             true);
    *phContext = reinterpret_cast<ur_context_handle_t>(Context);
  } catch (const std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextRetain(
    ur_context_handle_t
        hContext ///< [in] handle of the context to get a reference of.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextRelease(
    ur_context_handle_t hContext ///< [in] handle of the context to release.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextGetInfo(
    ur_context_handle_t hContext,      ///< [in] handle of the context
    ur_context_info_t ContextInfoType, ///< [in] type of the info to retrieve
    size_t propSize,    ///< [in] the number of bytes of memory pointed to by
                        ///< pContextInfo.
    void *pContextInfo, ///< [out][optional] array of bytes holding the info.
                        ///< if propSize is not equal to or greater than the
                        ///< real number of bytes needed to return the info then
                        ///< the ::UR_RESULT_ERROR_INVALID_SIZE error is
                        ///< returned and pContextInfo is not used.
    size_t *pPropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data queried by ContextInfoType.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextGetNativeHandle(
    ur_context_handle_t hContext,       ///< [in] handle of the context.
    ur_native_handle_t *phNativeContext ///< [out] a pointer to the native
                                        ///< handle of the context.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextCreateWithNativeHandle(
    ur_native_handle_t
        hNativeContext,            ///< [in] the native handle of the context.
    ur_context_handle_t *phContext ///< [out] pointer to the handle of the
                                   ///< context object created.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextSetExtendedDeleter(
    ur_context_handle_t hContext, ///< [in] handle of the context.
    ur_context_extended_deleter_t
        pfnDeleter, ///< [in] Function pointer to extended deleter.
    void *pUserData ///< [in][out][optional] pointer to data to be passed to
                    ///< callback.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t _ur_context_handle_t::initialize() {

  // Helper lambda to create various USM allocators for a device.
  // Note that the CCS devices and their respective subdevices share a
  // common ze_device_handle and therefore, also share USM allocators.
  auto createUSMAllocators = [this](ur_device_handle_t Device) {
    SharedMemAllocContexts.emplace(
        std::piecewise_construct, std::make_tuple(Device->ZeDevice),
        std::make_tuple(
            std::unique_ptr<SystemMemory>(
                new USMSharedMemoryAlloc(reinterpret_cast<ur_context_handle_t>(this), reinterpret_cast<ur_device_handle_t>(Device))),
            USMAllocatorConfigInstance.Configs[usm_settings::MemType::Shared]));

    SharedReadOnlyMemAllocContexts.emplace(
        std::piecewise_construct, std::make_tuple(Device->ZeDevice),
        std::make_tuple(std::unique_ptr<SystemMemory>(
                            new USMSharedReadOnlyMemoryAlloc(reinterpret_cast<ur_context_handle_t>(this), reinterpret_cast<ur_device_handle_t>(Device))),
                        USMAllocatorConfigInstance
                            .Configs[usm_settings::MemType::SharedReadOnly]));

    DeviceMemAllocContexts.emplace(
        std::piecewise_construct, std::make_tuple(Device->ZeDevice),
        std::make_tuple(
            std::unique_ptr<SystemMemory>(
                new USMDeviceMemoryAlloc(reinterpret_cast<ur_context_handle_t>(this), reinterpret_cast<ur_device_handle_t>(Device))),
            USMAllocatorConfigInstance.Configs[usm_settings::MemType::Device]));
  };

  // Recursive helper to call createUSMAllocators for all sub-devices
  std::function<void(ur_device_handle_t)> createUSMAllocatorsRecursive;
  createUSMAllocatorsRecursive =
      [createUSMAllocators,
       &createUSMAllocatorsRecursive](ur_device_handle_t Device) -> void {
    createUSMAllocators(Device);
    for (auto &SubDevice : Device->SubDevices)
      createUSMAllocatorsRecursive(SubDevice);
  };

  // Create USM allocator context for each pair (device, context).
  //
  for (auto &Device : Devices) {
    createUSMAllocatorsRecursive(Device);
  }
  // Create USM allocator context for host. Device and Shared USM allocations
  // are device-specific. Host allocations are not device-dependent therefore
  // we don't need a map with device as key.
  HostMemAllocContext = std::make_unique<USMAllocContext>(
      std::unique_ptr<SystemMemory>(new USMHostMemoryAlloc(reinterpret_cast<ur_context_handle_t>(this))),
      USMAllocatorConfigInstance.Configs[usm_settings::MemType::Host]);

  // We may allocate memory to this root device so create allocators.
  if (SingleRootDevice &&
      DeviceMemAllocContexts.find(SingleRootDevice->ZeDevice) ==
          DeviceMemAllocContexts.end()) {
    createUSMAllocators(SingleRootDevice);
  }

  // Create the immediate command list to be used for initializations.
  // Created as synchronous so level-zero performs implicit synchronization and
  // there is no need to query for completion in the plugin
  //
  // TODO: we use Device[0] here as the single immediate command-list
  // for buffer creation and migration. Initialization is in
  // in sync and is always performed to Devices[0] as well but
  // D2D migartion, if no P2P, is broken since it should use
  // immediate command-list for the specfic devices, and this single one.
  //
  ur_device_handle_t Device = SingleRootDevice ? SingleRootDevice : Devices[0];

  // Prefer to use copy engine for initialization copies,
  // if available and allowed (main copy engine with index 0).
  ZeStruct<ze_command_queue_desc_t> ZeCommandQueueDesc;
  const auto &Range = getRangeOfAllowedCopyEngines((ur_device_handle_t)Device);
  ZeCommandQueueDesc.ordinal =
      Device->QueueGroup[_ur_device_handle_t::queue_group_info_t::Compute].ZeOrdinal;
  if (Range.first >= 0 &&
      Device->QueueGroup[_ur_device_handle_t::queue_group_info_t::MainCopy].ZeOrdinal !=
          -1)
    ZeCommandQueueDesc.ordinal =
        Device->QueueGroup[_ur_device_handle_t::queue_group_info_t::MainCopy].ZeOrdinal;

  ZeCommandQueueDesc.index = 0;
  ZeCommandQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
  ZE2UR_CALL(
      zeCommandListCreateImmediate,
      (ZeContext, Device->ZeDevice, &ZeCommandQueueDesc, &ZeCommandListInit));
  return UR_RESULT_SUCCESS;
}

ur_device_handle_t _ur_context_handle_t::getRootDevice() const {
  assert(Devices.size() > 0);

  if (Devices.size() == 1)
    return Devices[0];

  // Check if we have context with subdevices of the same device (context
  // may include root device itself as well)
  ur_device_handle_t ContextRootDevice =
      Devices[0]->RootDevice ? Devices[0]->RootDevice : Devices[0];

  // For context with sub subdevices, the ContextRootDevice might still
  // not be the root device.
  // Check whether the ContextRootDevice is the subdevice or root device.
  if (ContextRootDevice->isSubDevice()) {
    ContextRootDevice = ContextRootDevice->RootDevice;
  }

  for (auto &Device : Devices) {
    if ((!Device->RootDevice && Device != ContextRootDevice) ||
        (Device->RootDevice && Device->RootDevice != ContextRootDevice)) {
      ContextRootDevice = nullptr;
      break;
    }
  }
  return ContextRootDevice;
}

