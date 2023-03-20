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
#include <mutex>

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
  // printf("%s %d phDevices[0] %lx\n", __FILE__, __LINE__, (unsigned long int)phDevices[0]);
  ZeStruct<ze_context_desc_t> ContextDesc {};

  // printf("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

  ze_context_handle_t ZeContext {};
  ZE2UR_CALL(zeContextCreate, (Platform->ZeDriver, &ContextDesc, &ZeContext));
  try {
    _ur_context_handle_t *Context = new _ur_context_handle_t(ZeContext,
                                                             DeviceCount,
                                                             const_cast<const ur_device_handle_t *>(phDevices),
                                                             true);
    
    // printf("%s %d phDevices[0] %lx\n", __FILE__, __LINE__, (unsigned long int)phDevices[0]);
    
    Context->initialize();
    // printf("%s %d phDevices[0] %lx\n", __FILE__, __LINE__, (unsigned long int)phDevices[0]);
    // printf("%s %d Context %lx getPlatform %lx\n", __FILE__, __LINE__, (unsigned long int)Context, (unsigned long int)Context->getPlatform());
    *phContext = reinterpret_cast<ur_context_handle_t>(Context);
    if (IndirectAccessTrackingEnabled) {
      std::scoped_lock<pi_shared_mutex> Lock(Platform->ContextsMutex);
      Platform->Contexts.push_back(*phContext);
    }
  } catch (const std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextRetain(
    ur_context_handle_t Context ///< [in] handle of the context to get a reference of.
) {
  Context->RefCount.increment();
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextRelease(
    ur_context_handle_t
      hContext ///< [in] handle of the context to release.
) {
  // printf("%s %d Context %lx\n", __FILE__, __LINE__, (unsigned long int)hContext);
  ur_platform_handle_t Plt = hContext->getPlatform();
  // printf("%s %d Plt %lx\n", __FILE__, __LINE__, (unsigned long int)Plt);  
  std::unique_lock<pi_shared_mutex> ContextsLock(Plt->ContextsMutex,
                                                 std::defer_lock);
  if (IndirectAccessTrackingEnabled)
    ContextsLock.lock();

  return ContextReleaseHelper(hContext);
}

UR_APIEXPORT ur_result_t UR_APICALL urContextGetInfo(
    ur_context_handle_t Context,      ///< [in] handle of the context
    ur_context_info_t ContextInfoType, ///< [in] type of the info to retrieve
    size_t PropSize,    ///< [in] the number of bytes of memory pointed to by
                        ///< pContextInfo.
    void *ContextInfo, ///< [out][optional] array of bytes holding the info.
                        ///< if propSize is not equal to or greater than the
                        ///< real number of bytes needed to return the info then
                        ///< the ::UR_RESULT_ERROR_INVALID_SIZE error is
                        ///< returned and pContextInfo is not used.
    size_t *PropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data queried by ContextInfoType.
) {
  std::shared_lock<pi_shared_mutex> Lock(Context->Mutex);
  UrReturnHelper ReturnValue(PropSize, ContextInfo, PropSizeRet);
  switch ((uint32_t)ContextInfoType) { // cast to avoid warnings on EXT enum values
  case UR_CONTEXT_INFO_DEVICES:
    return ReturnValue(&Context->Devices[0], Context->Devices.size());
  case UR_CONTEXT_INFO_NUM_DEVICES:
    return ReturnValue(pi_uint32(Context->Devices.size()));
  case UR_EXT_CONTEXT_INFO_REFERENCE_COUNT:
    return ReturnValue(pi_uint32{Context->RefCount.load()});
  case UR_CONTEXT_INFO_USM_MEMCPY2D_SUPPORT:
    // 2D USM memcpy is supported.
    return ReturnValue(pi_bool{true});
  case UR_CONTEXT_INFO_USM_FILL2D_SUPPORT:
  case UR_CONTEXT_INFO_USM_MEMSET2D_SUPPORT:
    // 2D USM fill and memset is not supported.
    return ReturnValue(pi_bool{false});
  default:
    // TODO: implement other parameters
    die("urGetContextInfo: unsuppported ParamName.");
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextGetNativeHandle(
    ur_context_handle_t Context,       ///< [in] handle of the context.
    ur_native_handle_t *NativeContext ///< [out] a pointer to the native
                                        ///< handle of the context.
) {
  *NativeContext = reinterpret_cast<ur_native_handle_t>(Context->ZeContext);
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextCreateWithNativeHandle(
    ur_native_handle_t NativeContext,            ///< [in] the native handle of the context.
    ur_context_handle_t *Context ///< [out] pointer to the handle of the
                                   ///< context object created.
) {
  try {
    ze_context_handle_t ZeContext = reinterpret_cast<ze_context_handle_t>(NativeContext);
    _ur_context_handle_t * UrContext = new _ur_context_handle_t(ZeContext);
    UrContext->initialize();
    *Context = reinterpret_cast<ur_context_handle_t>(UrContext);
  } catch (const std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  return UR_RESULT_SUCCESS;
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

// Helper function to release the context, a caller must lock the platform-level
// mutex guarding the container with contexts because the context can be removed
// from the list of tracked contexts.
ur_result_t ContextReleaseHelper(ur_context_handle_t Context) {

  // PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);

  if (!Context->RefCount.decrementAndTest())
    return UR_RESULT_SUCCESS;

  if (IndirectAccessTrackingEnabled) {
    ur_platform_handle_t Plt = Context->getPlatform();
    auto &Contexts = Plt->Contexts;
    auto It = std::find(Contexts.begin(), Contexts.end(), Context);
    if (It != Contexts.end())
      Contexts.erase(It);
  }
  ze_context_handle_t DestoryZeContext =
      Context->OwnZeContext ? Context->ZeContext : nullptr;

  // Clean up any live memory associated with Context
  ur_result_t Result = Context->finalize();

  // We must delete Context first and then destroy zeContext because
  // Context deallocation requires ZeContext in some member deallocation of
  // pi_context.
  delete Context;

  // Destruction of some members of pi_context uses L0 context
  // and therefore it must be valid at that point.
  // Technically it should be placed to the destructor of pi_context
  // but this makes API error handling more complex.
  if (DestoryZeContext)
    ZE2UR_CALL(zeContextDestroy, (DestoryZeContext));

  return Result;
}

ur_platform_handle_t _ur_context_handle_t::getPlatform() const {
  // printf("%s %d Devices[0] %lx\n", __FILE__, __LINE__, (unsigned long int)Devices[0]);
  return Devices[0]->Platform;
}

ur_result_t _ur_context_handle_t::finalize() {
  // This function is called when pi_context is deallocated, piContextRelease.
  // There could be some memory that may have not been deallocated.
  // For example, event and event pool caches would be still alive.

  if (!DisableEventsCaching) {
    std::scoped_lock<pi_mutex> Lock(EventCacheMutex);
    for (auto &EventCache : EventCaches) {
      for (auto &Event : EventCache) {
        ZE2UR_CALL(zeEventDestroy, (Event->ZeEvent));
        delete Event;
      }
      EventCache.clear();
    }
  }
  {
    std::scoped_lock<pi_mutex> Lock(ZeEventPoolCacheMutex);
    for (auto &ZePoolCache : ZeEventPoolCache) {
      for (auto &ZePool : ZePoolCache)
        ZE2UR_CALL(zeEventPoolDestroy, (ZePool));
      ZePoolCache.clear();
    }
  }

  // Destroy the command list used for initializations
  ZE2UR_CALL(zeCommandListDestroy, (ZeCommandListInit));

  std::scoped_lock<pi_mutex> Lock(ZeCommandListCacheMutex);
  for (auto &List : ZeComputeCommandListCache) {
    for (ze_command_list_handle_t &ZeCommandList : List.second) {
      if (ZeCommandList)
        ZE2UR_CALL(zeCommandListDestroy, (ZeCommandList));
    }
  }
  for (auto &List : ZeCopyCommandListCache) {
    for (ze_command_list_handle_t &ZeCommandList : List.second) {
      if (ZeCommandList)
        ZE2UR_CALL(zeCommandListDestroy, (ZeCommandList));
    }
  }
  return UR_RESULT_SUCCESS;
}

ur_result_t
_ur_context_handle_t::getFreeSlotInExistingOrNewPool(ze_event_pool_handle_t &Pool,
                                            size_t &Index, bool HostVisible,
                                            bool ProfilingEnabled) {
  // Lock while updating event pool machinery.
  std::scoped_lock<pi_mutex> Lock(ZeEventPoolCacheMutex);

  std::list<ze_event_pool_handle_t> *ZePoolCache =
      getZeEventPoolCache(HostVisible, ProfilingEnabled);

  if (!ZePoolCache->empty()) {
    if (NumEventsAvailableInEventPool[ZePoolCache->front()] == 0) {
      if (DisableEventsCaching) {
        // Remove full pool from the cache if events caching is disabled.
        ZePoolCache->erase(ZePoolCache->begin());
      } else {
        // If event caching is enabled then we don't destroy events so there is
        // no need to remove pool from the cache and add it back when it has
        // available slots. Just keep it in the tail of the cache so that all
        // pools can be destroyed during context destruction.
        ZePoolCache->push_front(nullptr);
      }
    }
  }
  if (ZePoolCache->empty()) {
    ZePoolCache->push_back(nullptr);
  }

  // We shall be adding an event to the front pool.
  ze_event_pool_handle_t *ZePool = &ZePoolCache->front();
  Index = 0;
  // Create one event ZePool per MaxNumEventsPerPool events
  if (*ZePool == nullptr) {
    ZeStruct<ze_event_pool_desc_t> ZeEventPoolDesc;
    ZeEventPoolDesc.count = MaxNumEventsPerPool;
    ZeEventPoolDesc.flags = 0;
    if (HostVisible)
      ZeEventPoolDesc.flags |= ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    if (ProfilingEnabled)
      ZeEventPoolDesc.flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    zePrint("ze_event_pool_desc_t flags set to: %d\n", ZeEventPoolDesc.flags);

    std::vector<ze_device_handle_t> ZeDevices;
    std::for_each(Devices.begin(), Devices.end(), [&](const ur_device_handle_t &D) {
      ZeDevices.push_back(D->ZeDevice);
    });

    ZE2UR_CALL(zeEventPoolCreate, (ZeContext, &ZeEventPoolDesc, ZeDevices.size(),
                                &ZeDevices[0], ZePool));
    NumEventsAvailableInEventPool[*ZePool] = MaxNumEventsPerPool - 1;
    NumEventsUnreleasedInEventPool[*ZePool] = 1;
  } else {
    Index = MaxNumEventsPerPool - NumEventsAvailableInEventPool[*ZePool];
    --NumEventsAvailableInEventPool[*ZePool];
    ++NumEventsUnreleasedInEventPool[*ZePool];
  }
  Pool = *ZePool;
  return UR_RESULT_SUCCESS;
}

ur_event_handle_t
_ur_context_handle_t::getEventFromContextCache(bool HostVisible,
                                               bool WithProfiling) {
  std::scoped_lock<pi_mutex> Lock(EventCacheMutex);
  auto Cache = getEventCache(HostVisible, WithProfiling);
  if (Cache->empty())
    return nullptr;

  auto It = Cache->begin();
  ur_event_handle_t Event = *It;
  Cache->erase(It);
  // We have to reset event before using it.
  Event->reset();
  return Event;
}

void _ur_context_handle_t::addEventToContextCache(ur_event_handle_t Event) {
  std::scoped_lock<pi_mutex> Lock(EventCacheMutex);
  auto Cache =
      getEventCache(Event->isHostVisible(), Event->isProfilingEnabled());
  Cache->emplace_back(Event);
}

ur_result_t _ur_context_handle_t::decrementUnreleasedEventsInPool(ur_event_handle_t Event) {
  std::shared_lock<pi_shared_mutex> EventLock(Event->Mutex, std::defer_lock);
  std::scoped_lock<pi_mutex, std::shared_lock<pi_shared_mutex>> LockAll(
      ZeEventPoolCacheMutex, EventLock);
  if (!Event->ZeEventPool) {
    // This must be an interop event created on a users's pool.
    // Do nothing.
    return UR_RESULT_SUCCESS;
  }

  std::list<ze_event_pool_handle_t> *ZePoolCache =
      getZeEventPoolCache(Event->isHostVisible(), Event->isProfilingEnabled());

  // Put the empty pool to the cache of the pools.
  if (NumEventsUnreleasedInEventPool[Event->ZeEventPool] == 0)
    die("Invalid event release: event pool doesn't have unreleased events");
  if (--NumEventsUnreleasedInEventPool[Event->ZeEventPool] == 0) {
    if (ZePoolCache->front() != Event->ZeEventPool) {
      ZePoolCache->push_back(Event->ZeEventPool);
    }
    NumEventsAvailableInEventPool[Event->ZeEventPool] = MaxNumEventsPerPool;
  }

  return UR_RESULT_SUCCESS;
}

// Get value of the threshold for number of events in immediate command lists.
// If number of events in the immediate command list exceeds this threshold then
// cleanup process for those events is executed.
static const size_t ImmCmdListsEventCleanupThreshold = [] {
  const char *ImmCmdListsEventCleanupThresholdStr = std::getenv(
      "SYCL_PI_LEVEL_ZERO_IMMEDIATE_COMMANDLISTS_EVENT_CLEANUP_THRESHOLD");
  static constexpr int Default = 20;
  if (!ImmCmdListsEventCleanupThresholdStr)
    return Default;

  int Threshold = std::atoi(ImmCmdListsEventCleanupThresholdStr);

  // Basically disable threshold if negative value is provided.
  if (Threshold < 0)
    return INT_MAX;

  return Threshold;
}();

// Retrieve an available command list to be used in a PI call.
ur_result_t _ur_context_handle_t::getAvailableCommandList(
    ur_queue_handle_t Queue, pi_command_list_ptr_t &CommandList, bool UseCopyEngine,
    bool AllowBatching, ze_command_queue_handle_t *ForcedCmdQueue) {
  // Immediate commandlists have been pre-allocated and are always available.
  if (Queue->Device->useImmediateCommandLists()) {
    CommandList = Queue->getQueueGroup(UseCopyEngine).getImmCmdList();
    if (CommandList->second.EventList.size() >
        ImmCmdListsEventCleanupThreshold) {
      std::vector<ur_event_handle_t> EventListToCleanup;
      Queue->resetCommandList(CommandList, false, EventListToCleanup);
      CleanupEventListFromResetCmdList(EventListToCleanup, true);
    }
    UR_CALL(Queue->insertStartBarrierIfDiscardEventsMode(CommandList));
    if (auto Res = Queue->insertActiveBarriers(CommandList, UseCopyEngine))
      return Res;
    return UR_RESULT_SUCCESS;
  }

  auto &CommandBatch =
      UseCopyEngine ? Queue->CopyCommandBatch : Queue->ComputeCommandBatch;
  // Handle batching of commands
  // First see if there is an command-list open for batching commands
  // for this queue.
  if (Queue->hasOpenCommandList(UseCopyEngine)) {
    if (AllowBatching) {
      CommandList = CommandBatch.OpenCommandList;
      UR_CALL(Queue->insertStartBarrierIfDiscardEventsMode(CommandList));
      return UR_RESULT_SUCCESS;
    }
    // If this command isn't allowed to be batched or doesn't match the forced
    // command queue, then we need to go ahead and execute what is already in
    // the batched list, and then go on to process this. On exit from
    // executeOpenCommandList OpenCommandList will be invalidated.
    if (auto Res = Queue->executeOpenCommandList(UseCopyEngine))
      return Res;
    // Note that active barriers do not need to be inserted here as they will
    // have been enqueued into the command-list when they were created.
  }

  // Create/Reuse the command list, because in Level Zero commands are added to
  // the command lists, and later are then added to the command queue.
  // Each command list is paired with an associated fence to track when the
  // command list is available for reuse.
  ur_result_t pi_result = UR_RESULT_ERROR_OUT_OF_RESOURCES;

  // Initally, we need to check if a command list has already been created
  // on this device that is available for use. If so, then reuse that
  // Level-Zero Command List and Fence for this PI call.
  {
    // Make sure to acquire the lock before checking the size, or there
    // will be a race condition.
    std::scoped_lock<pi_mutex> Lock(Queue->Context->ZeCommandListCacheMutex);
    // Under mutex since operator[] does insertion on the first usage for every
    // unique ZeDevice.
    auto &ZeCommandListCache =
        UseCopyEngine
            ? Queue->Context->ZeCopyCommandListCache[Queue->Device->ZeDevice]
            : Queue->Context
                  ->ZeComputeCommandListCache[Queue->Device->ZeDevice];

    for (auto ZeCommandListIt = ZeCommandListCache.begin();
         ZeCommandListIt != ZeCommandListCache.end(); ++ZeCommandListIt) {
      auto &ZeCommandList = *ZeCommandListIt;
      auto it = Queue->CommandListMap.find(ZeCommandList);
      if (it != Queue->CommandListMap.end()) {
        if (ForcedCmdQueue && *ForcedCmdQueue != it->second.ZeQueue)
          continue;
        CommandList = it;
        if (CommandList->second.ZeFence != nullptr)
          CommandList->second.ZeFenceInUse = true;
      } else {
        // If there is a command list available on this context, but it
        // wasn't yet used in this queue then create a new entry in this
        // queue's map to hold the fence and other associated command
        // list information.
        auto &QGroup = Queue->getQueueGroup(UseCopyEngine);
        uint32_t QueueGroupOrdinal;
        auto &ZeCommandQueue = ForcedCmdQueue
                                   ? *ForcedCmdQueue
                                   : QGroup.getZeQueue(&QueueGroupOrdinal);
        if (ForcedCmdQueue)
          QueueGroupOrdinal = QGroup.getCmdQueueOrdinal(ZeCommandQueue);

        ze_fence_handle_t ZeFence;
        ZeStruct<ze_fence_desc_t> ZeFenceDesc;
        ZE2UR_CALL(zeFenceCreate, (ZeCommandQueue, &ZeFenceDesc, &ZeFence));
        CommandList =
            Queue->CommandListMap
                .emplace(ZeCommandList,
                         pi_command_list_info_t{ZeFence, true, ZeCommandQueue,
                                                QueueGroupOrdinal})
                .first;
      }
      ZeCommandListCache.erase(ZeCommandListIt);
      if (auto Res = Queue->insertStartBarrierIfDiscardEventsMode(CommandList))
        return Res;
      if (auto Res = Queue->insertActiveBarriers(CommandList, UseCopyEngine))
        return Res;
      return UR_RESULT_SUCCESS;
    }
  }

  // If there are no available command lists in the cache, then we check for
  // command lists that have already signalled, but have not been added to the
  // available list yet. Each command list has a fence associated which tracks
  // if a command list has completed dispatch of its commands and is ready for
  // reuse. If a command list is found to have been signalled, then the
  // command list & fence are reset and we return.
  for (auto it = Queue->CommandListMap.begin();
       it != Queue->CommandListMap.end(); ++it) {
    // Make sure this is the command list type needed.
    if (UseCopyEngine != it->second.isCopy(reinterpret_cast<ur_queue_handle_t>(Queue)))
      continue;

    ze_result_t ZeResult =
        ZE_CALL_NOCHECK(zeFenceQueryStatus, (it->second.ZeFence));
    if (ZeResult == ZE_RESULT_SUCCESS) {
      std::vector<ur_event_handle_t> EventListToCleanup;
      Queue->resetCommandList(it, false, EventListToCleanup);
      CleanupEventListFromResetCmdList(EventListToCleanup,
                                       true /* QueueLocked */);
      CommandList = it;
      CommandList->second.ZeFenceInUse = true;
      if (auto Res = Queue->insertStartBarrierIfDiscardEventsMode(CommandList))
        return Res;
      return UR_RESULT_SUCCESS;
    }
  }

  // If there are no available command lists nor signalled command lists,
  // then we must create another command list.
  pi_result = Queue->createCommandList(UseCopyEngine, CommandList);
  CommandList->second.ZeFenceInUse = true;
  return pi_result;
}

bool _ur_context_handle_t::isValidDevice(ur_device_handle_t Device) const {
  while (Device) {
    if (std::find(Devices.begin(), Devices.end(), Device) != Devices.end())
      return true;
    Device = Device->RootDevice;
  }
  return false;
}
