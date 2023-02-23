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

#include "ur_level_zero_common.hpp"
#include "ur_level_zero_queue.hpp"
#include <ur_bindings.hpp>

UR_APIEXPORT ur_result_t UR_APICALL urQueueGetInfo(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_queue_info_t propName, ///< [in] name of the queue property to query
    size_t propValueSize, ///< [in] size in bytes of the queue property value
                          ///< provided
    void *pPropValue,     ///< [out] value of the queue property
    size_t
        *pPropSizeRet ///< [out] size in bytes returned in queue property value
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueCreate(
    ur_context_handle_t hContext, ///< [in] handle of the context object
    ur_device_handle_t hDevice,   ///< [in] handle of the device object
    const ur_queue_property_t
        *pProps, ///< [in] specifies a list of queue properties and their
                 ///< corresponding values. Each property name is immediately
                 ///< followed by the corresponding desired value. The list is
                 ///< terminated with a 0. If a property value is not specified,
                 ///< then its default value will be used.
    ur_queue_handle_t
        *phQueue ///< [out] pointer to handle of queue object created
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueRetain(
    ur_queue_handle_t hQueue ///< [in] handle of the queue object to get access
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueRelease(
    ur_queue_handle_t hQueue ///< [in] handle of the queue object to release
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueGetNativeHandle(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue.
    ur_native_handle_t
        *phNativeQueue ///< [out] a pointer to the native handle of the queue.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueCreateWithNativeHandle(
    ur_native_handle_t hNativeQueue, ///< [in] the native handle of the queue.
    ur_context_handle_t hContext,    ///< [in] handle of the context object
    ur_queue_handle_t
        *phQueue ///< [out] pointer to the handle of the queue object created.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueFinish(
    ur_queue_handle_t hQueue ///< [in] handle of the queue to be finished.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueFlush(
    ur_queue_handle_t hQueue ///< [in] handle of the queue to be flushed.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

// Configuration of the command-list batching.
struct zeCommandListBatchConfig {
  // Default value of 0. This specifies to use dynamic batch size adjustment.
  // Other values will try to collect specified amount of commands.
  uint32_t Size{0};

  // If doing dynamic batching, specifies start batch size.
  uint32_t DynamicSizeStart{4};

  // The maximum size for dynamic batch.
  uint32_t DynamicSizeMax{64};

  // The step size for dynamic batch increases.
  uint32_t DynamicSizeStep{1};

  // Thresholds for when increase batch size (number of closed early is small
  // and number of closed full is high).
  uint32_t NumTimesClosedEarlyThreshold{3};
  uint32_t NumTimesClosedFullThreshold{8};

  // Tells the starting size of a batch.
  uint32_t startSize() const { return Size > 0 ? Size : DynamicSizeStart; }
  // Tells is we are doing dynamic batch size adjustment.
  bool dynamic() const { return Size == 0; }
};


// Helper function to initialize static variables that holds batch config info
// for compute and copy command batching.
static const zeCommandListBatchConfig ZeCommandListBatchConfig(bool IsCopy) {
  zeCommandListBatchConfig Config{}; // default initialize

  // Default value of 0. This specifies to use dynamic batch size adjustment.
  const auto BatchSizeStr =
      (IsCopy) ? std::getenv("SYCL_PI_LEVEL_ZERO_COPY_BATCH_SIZE")
               : std::getenv("SYCL_PI_LEVEL_ZERO_BATCH_SIZE");
  if (BatchSizeStr) {
    pi_int32 BatchSizeStrVal = std::atoi(BatchSizeStr);
    // Level Zero may only support a limted number of commands per command
    // list.  The actual upper limit is not specified by the Level Zero
    // Specification.  For now we allow an arbitrary upper limit.
    if (BatchSizeStrVal > 0) {
      Config.Size = BatchSizeStrVal;
    } else if (BatchSizeStrVal == 0) {
      Config.Size = 0;
      // We are requested to do dynamic batching. Collect specifics, if any.
      // The extended format supported is ":" separated values.
      //
      // NOTE: these extra settings are experimental and are intended to
      // be used only for finding a better default heuristic.
      //
      std::string BatchConfig(BatchSizeStr);
      size_t Ord = 0;
      size_t Pos = 0;
      while (true) {
        if (++Ord > 5)
          break;

        Pos = BatchConfig.find(":", Pos);
        if (Pos == std::string::npos)
          break;
        ++Pos; // past the ":"

        pi_uint32 Val;
        try {
          Val = std::stoi(BatchConfig.substr(Pos));
        } catch (...) {
          if (IsCopy)
            zePrint(
                "SYCL_PI_LEVEL_ZERO_COPY_BATCH_SIZE: failed to parse value\n");
          else
            zePrint("SYCL_PI_LEVEL_ZERO_BATCH_SIZE: failed to parse value\n");
          break;
        }
        switch (Ord) {
        case 1:
          Config.DynamicSizeStart = Val;
          break;
        case 2:
          Config.DynamicSizeMax = Val;
          break;
        case 3:
          Config.DynamicSizeStep = Val;
          break;
        case 4:
          Config.NumTimesClosedEarlyThreshold = Val;
          break;
        case 5:
          Config.NumTimesClosedFullThreshold = Val;
          break;
        default:
          die("Unexpected batch config");
        }
        if (IsCopy)
          zePrint("SYCL_PI_LEVEL_ZERO_COPY_BATCH_SIZE: dynamic batch param "
                  "#%d: %d\n",
                  (int)Ord, (int)Val);
        else
          zePrint(
              "SYCL_PI_LEVEL_ZERO_BATCH_SIZE: dynamic batch param #%d: %d\n",
              (int)Ord, (int)Val);
      };

    } else {
      // Negative batch sizes are silently ignored.
      if (IsCopy)
        zePrint("SYCL_PI_LEVEL_ZERO_COPY_BATCH_SIZE: ignored negative value\n");
      else
        zePrint("SYCL_PI_LEVEL_ZERO_BATCH_SIZE: ignored negative value\n");
    }
  }
  return Config;
}


// SYCL_PI_LEVEL_ZERO_USE_COMPUTE_ENGINE can be set to an integer (>=0) in
// which case all compute commands will be submitted to the command-queue
// with the given index in the compute command group. If it is instead set
// to negative then all available compute engines may be used.
//
// The default value is "0".
//
static const std::pair<int, int> getRangeOfAllowedComputeEngines() {
  static const char *EnvVar =
      std::getenv("SYCL_PI_LEVEL_ZERO_USE_COMPUTE_ENGINE");
  // If the environment variable is not set only use "0" CCS for now.
  // TODO: allow all CCSs when HW support is complete.
  if (!EnvVar)
    return std::pair<int, int>(0, 0);

  auto EnvVarValue = std::atoi(EnvVar);
  if (EnvVarValue >= 0) {
    return std::pair<int, int>(EnvVarValue, EnvVarValue);
  }

  return std::pair<int, int>(0, INT_MAX);
}

// Static variable that holds batch config info for compute command batching.
static const zeCommandListBatchConfig ZeCommandListBatchComputeConfig = [] {
  using IsCopy = bool;
  return ZeCommandListBatchConfig(IsCopy{false});
}();

// Static variable that holds batch config info for copy command batching.
static const zeCommandListBatchConfig ZeCommandListBatchCopyConfig = [] {
  using IsCopy = bool;
  return ZeCommandListBatchConfig(IsCopy{true});
}();


_ur_queue_handle_t::_ur_queue_handle_t(std::vector<ze_command_queue_handle_t> &ComputeQueues,
                                       std::vector<ze_command_queue_handle_t> &CopyQueues,
                                       ur_context_handle_t Context,
                                       ur_device_handle_t Device,
                                       bool OwnZeCommandQueue,
                                      pi_queue_properties Properties,
                                      int ForceComputeIndex): Context{Context},
                                                              Device{Device},
                                                              OwnZeCommandQueue{OwnZeCommandQueue},
                                                              Properties(Properties) {
// Compute group initialization.
  // First, see if the queue's device allows for round-robin or it is
  // fixed to one particular compute CCS (it is so for sub-sub-devices).
  auto &ComputeQueueGroupInfo = Device->QueueGroup[queue_type::Compute];
  pi_queue_group_t ComputeQueueGroup{reinterpret_cast<ur_queue_handle_t>(this), queue_type::Compute};
  ComputeQueueGroup.ZeQueues = ComputeQueues;
  // Create space to hold immediate commandlists corresponding to the
  // ZeQueues
  if (Device->useImmediateCommandLists()) {
    ComputeQueueGroup.ImmCmdLists = std::vector<pi_command_list_ptr_t>(
        ComputeQueueGroup.ZeQueues.size(), CommandListMap.end());
  }
  if (ComputeQueueGroupInfo.ZeIndex >= 0) {
    // Sub-sub-device

    // sycl::ext::intel::property::queue::compute_index works with any
    // backend/device by allowing single zero index if multiple compute CCSes
    // are not supported. Sub-sub-device falls into the same bucket.
    assert(ForceComputeIndex <= 0);
    ComputeQueueGroup.LowerIndex = ComputeQueueGroupInfo.ZeIndex;
    ComputeQueueGroup.UpperIndex = ComputeQueueGroupInfo.ZeIndex;
    ComputeQueueGroup.NextIndex = ComputeQueueGroupInfo.ZeIndex;
  } else if (ForceComputeIndex >= 0) {
    ComputeQueueGroup.LowerIndex = ForceComputeIndex;
    ComputeQueueGroup.UpperIndex = ForceComputeIndex;
    ComputeQueueGroup.NextIndex = ForceComputeIndex;
  } else {
    // Set-up to round-robin across allowed range of engines.
    uint32_t FilterLowerIndex = getRangeOfAllowedComputeEngines().first;
    uint32_t FilterUpperIndex = getRangeOfAllowedComputeEngines().second;
    FilterUpperIndex = std::min((size_t)FilterUpperIndex,
                                FilterLowerIndex + ComputeQueues.size() - 1);
    if (FilterLowerIndex <= FilterUpperIndex) {
      ComputeQueueGroup.LowerIndex = FilterLowerIndex;
      ComputeQueueGroup.UpperIndex = FilterUpperIndex;
      ComputeQueueGroup.NextIndex = ComputeQueueGroup.LowerIndex;
    } else {
      die("No compute queue available/allowed.");
    }
  }
  if (Device->useImmediateCommandLists()) {
    // Create space to hold immediate commandlists corresponding to the
    // ZeQueues
    ComputeQueueGroup.ImmCmdLists = std::vector<pi_command_list_ptr_t>(
        ComputeQueueGroup.ZeQueues.size(), CommandListMap.end());
  }

  // Thread id will be used to create separate queue groups per thread.
  auto TID = std::this_thread::get_id();
  ComputeQueueGroupsByTID.insert({TID, ComputeQueueGroup});

  // Copy group initialization.
  pi_queue_group_t CopyQueueGroup{reinterpret_cast<ur_queue_handle_t>(this), queue_type::MainCopy};
  const auto &Range = getRangeOfAllowedCopyEngines((ur_device_handle_t)Device);
  if (Range.first < 0 || Range.second < 0) {
    // We are asked not to use copy engines, just do nothing.
    // Leave CopyQueueGroup.ZeQueues empty, and it won't be used.
  } else {
    uint32_t FilterLowerIndex = Range.first;
    uint32_t FilterUpperIndex = Range.second;
    FilterUpperIndex = std::min((size_t)FilterUpperIndex,
                                FilterLowerIndex + CopyQueues.size() - 1);
    if (FilterLowerIndex <= FilterUpperIndex) {
      CopyQueueGroup.ZeQueues = CopyQueues;
      CopyQueueGroup.LowerIndex = FilterLowerIndex;
      CopyQueueGroup.UpperIndex = FilterUpperIndex;
      CopyQueueGroup.NextIndex = CopyQueueGroup.LowerIndex;
      // Create space to hold immediate commandlists corresponding to the
      // ZeQueues
      if (Device->useImmediateCommandLists()) {
        CopyQueueGroup.ImmCmdLists = std::vector<pi_command_list_ptr_t>(
            CopyQueueGroup.ZeQueues.size(), CommandListMap.end());
      }
    }
  }
  CopyQueueGroupsByTID.insert({TID, CopyQueueGroup});

  // Initialize compute/copy command batches.
  ComputeCommandBatch.OpenCommandList = CommandListMap.end();
  CopyCommandBatch.OpenCommandList = CommandListMap.end();
  ComputeCommandBatch.QueueBatchSize =
      ZeCommandListBatchComputeConfig.startSize();
  CopyCommandBatch.QueueBatchSize = ZeCommandListBatchCopyConfig.startSize();

}

void _ur_queue_handle_t::adjustBatchSizeForFullBatch(bool IsCopy) {
  auto &CommandBatch = IsCopy ? CopyCommandBatch : ComputeCommandBatch;
  auto &ZeCommandListBatchConfig =
      IsCopy ? ZeCommandListBatchCopyConfig : ZeCommandListBatchComputeConfig;
  pi_uint32 &QueueBatchSize = CommandBatch.QueueBatchSize;
  // QueueBatchSize of 0 means never allow batching.
  if (QueueBatchSize == 0 || !ZeCommandListBatchConfig.dynamic())
    return;
  CommandBatch.NumTimesClosedFull += 1;

  // If the number of times the list has been closed early is low, and
  // the number of times it has been closed full is high, then raise
  // the batching size slowly. Don't raise it if it is already pretty
  // high.
  if (CommandBatch.NumTimesClosedEarly <=
          ZeCommandListBatchConfig.NumTimesClosedEarlyThreshold &&
      CommandBatch.NumTimesClosedFull >
          ZeCommandListBatchConfig.NumTimesClosedFullThreshold) {
    if (QueueBatchSize < ZeCommandListBatchConfig.DynamicSizeMax) {
      QueueBatchSize += ZeCommandListBatchConfig.DynamicSizeStep;
      zePrint("Raising QueueBatchSize to %d\n", QueueBatchSize);
    }
    CommandBatch.NumTimesClosedEarly = 0;
    CommandBatch.NumTimesClosedFull = 0;
  }
}

void _ur_queue_handle_t::adjustBatchSizeForPartialBatch(bool IsCopy) {
  auto &CommandBatch = IsCopy ? CopyCommandBatch : ComputeCommandBatch;
  auto &ZeCommandListBatchConfig =
      IsCopy ? ZeCommandListBatchCopyConfig : ZeCommandListBatchComputeConfig;
  pi_uint32 &QueueBatchSize = CommandBatch.QueueBatchSize;
  // QueueBatchSize of 0 means never allow batching.
  if (QueueBatchSize == 0 || !ZeCommandListBatchConfig.dynamic())
    return;
  CommandBatch.NumTimesClosedEarly += 1;

  // If we are closing early more than about 3x the number of times
  // it is closing full, lower the batch size to the value of the
  // current open command list. This is trying to quickly get to a
  // batch size that will be able to be closed full at least once
  // in a while.
  if (CommandBatch.NumTimesClosedEarly >
      (CommandBatch.NumTimesClosedFull + 1) * 3) {
    QueueBatchSize = CommandBatch.OpenCommandList->second.size() - 1;
    if (QueueBatchSize < 1)
      QueueBatchSize = 1;
    zePrint("Lowering QueueBatchSize to %d\n", QueueBatchSize);
    CommandBatch.NumTimesClosedEarly = 0;
    CommandBatch.NumTimesClosedFull = 0;
  }
}

ur_result_t _ur_queue_handle_t::executeCommandList(pi_command_list_ptr_t CommandList,
                                        bool IsBlocking,
                                        bool OKToBatchCommand) {
  bool UseCopyEngine = CommandList->second.isCopy(reinterpret_cast<ur_queue_handle_t>(this));

  // If the current LastCommandEvent is the nullptr, then it means
  // either that no command has ever been issued to the queue
  // or it means that the LastCommandEvent has been signalled and
  // therefore that this Queue is idle.
  //
  // NOTE: this behavior adds some flakyness to the batching
  // since last command's event may or may not be completed by the
  // time we get here depending on timings and system/gpu load.
  // So, disable it for modes where we print PI traces. Printing
  // traces incurs much different timings than real execution
  // ansyway, and many regression tests use it.
  //
  bool CurrentlyEmpty = !PrintTrace && this->LastCommandEvent == nullptr;

  // The list can be empty if command-list only contains signals of proxy
  // events. It is possible that executeCommandList is called twice for the same
  // command list without new appended command. We don't to want process the
  // same last command event twice that's why additionally check that new
  // command was appended to the command list.
  if (!CommandList->second.EventList.empty() &&
      this->LastCommandEvent != CommandList->second.EventList.back()) {
    this->LastCommandEvent = CommandList->second.EventList.back();
    if (doReuseDiscardedEvents()) {
      UR_CALL(resetDiscardedEvent(CommandList));
    }
  }

  this->LastUsedCommandList = CommandList;

  if (!Device->useImmediateCommandLists()) {
    // Batch if allowed to, but don't batch if we know there are no kernels
    // from this queue that are currently executing.  This is intended to get
    // kernels started as soon as possible when there are no kernels from this
    // queue awaiting execution, while allowing batching to occur when there
    // are kernels already executing. Also, if we are using fixed size batching,
    // as indicated by !ZeCommandListBatch.dynamic(), then just ignore
    // CurrentlyEmpty as we want to strictly follow the batching the user
    // specified.
    auto &CommandBatch = UseCopyEngine ? CopyCommandBatch : ComputeCommandBatch;
    auto &ZeCommandListBatchConfig = UseCopyEngine
                                         ? ZeCommandListBatchCopyConfig
                                         : ZeCommandListBatchComputeConfig;
    if (OKToBatchCommand && this->isBatchingAllowed(UseCopyEngine) &&
        (!ZeCommandListBatchConfig.dynamic() || !CurrentlyEmpty)) {

      if (hasOpenCommandList(UseCopyEngine) &&
          CommandBatch.OpenCommandList != CommandList)
        die("executeCommandList: OpenCommandList should be equal to"
            "null or CommandList");

      if (CommandList->second.size() < CommandBatch.QueueBatchSize) {
        CommandBatch.OpenCommandList = CommandList;
        return UR_RESULT_SUCCESS;
      }

      adjustBatchSizeForFullBatch(UseCopyEngine);
      CommandBatch.OpenCommandList = CommandListMap.end();
    }
  }

  auto &ZeCommandQueue = CommandList->second.ZeQueue;
  // Scope of the lock must be till the end of the function, otherwise new mem
  // allocs can be created between the moment when we made a snapshot and the
  // moment when command list is closed and executed. But mutex is locked only
  // if indirect access tracking enabled, because std::defer_lock is used.
  // unique_lock destructor at the end of the function will unlock the mutex
  // if it was locked (which happens only if IndirectAccessTrackingEnabled is
  // true).
  std::unique_lock<pi_shared_mutex> ContextsLock(
      Device->Platform->ContextsMutex, std::defer_lock);

  if (IndirectAccessTrackingEnabled) {
    // We are going to submit kernels for execution. If indirect access flag is
    // set for a kernel then we need to make a snapshot of existing memory
    // allocations in all contexts in the platform. We need to lock the mutex
    // guarding the list of contexts in the platform to prevent creation of new
    // memory alocations in any context before we submit the kernel for
    // execution.
    ContextsLock.lock();
    CaptureIndirectAccesses();
  }

  if (!Device->useImmediateCommandLists()) {
    // In this mode all inner-batch events have device visibility only,
    // and we want the last command in the batch to signal a host-visible
    // event that anybody waiting for any event in the batch will
    // really be using.
    // We need to create a proxy host-visible event only if the list of events
    // in the command list is not empty, otherwise we are going to just create
    // and remove proxy event right away and dereference deleted object
    // afterwards.
    if (DeviceEventsSetting == LastCommandInBatchHostVisible &&
        !CommandList->second.EventList.empty()) {
      // If there are only internal events in the command list then we don't
      // need to create host proxy event.
      auto Result =
          std::find_if(CommandList->second.EventList.begin(),
                       CommandList->second.EventList.end(),
                       [](ur_event_handle_t E) { return E->hasExternalRefs(); });
      if (Result != CommandList->second.EventList.end()) {
        // Create a "proxy" host-visible event.
        //
        ur_event_handle_t HostVisibleEvent;
        auto Res = createEventAndAssociateQueue(
            reinterpret_cast<ur_queue_handle_t>(this), &HostVisibleEvent, PI_COMMAND_TYPE_USER, CommandList,
            /* IsInternal */ false, /* ForceHostVisible */ true);
        if (Res)
          return Res;

        // Update each command's event in the command-list to "see" this
        // proxy event as a host-visible counterpart.
        for (auto &Event : CommandList->second.EventList) {
          std::scoped_lock<pi_shared_mutex> EventLock(Event->Mutex);
          // Internal event doesn't need host-visible proxy.
          if (!Event->hasExternalRefs())
            continue;

          if (!Event->HostVisibleEvent) {
            Event->HostVisibleEvent = reinterpret_cast<ur_event_handle_t>(HostVisibleEvent);
            HostVisibleEvent->RefCount.increment();
          }
        }

        // Decrement the reference count of the event such that all the
        // remaining references are from the other commands in this batch and
        // from the command-list itself. This host-visible event will not be
        // waited/released by SYCL RT, so it must be destroyed after all events
        // in the batch are gone. We know that refcount is more than 2 because
        // we check that EventList of the command list is not empty above, i.e.
        // after createEventAndAssociateQueue ref count is 2 and then +1 for
        // each event in the EventList.
        UR_CALL(piEventReleaseInternal(HostVisibleEvent));

        if (doReuseDiscardedEvents()) {
          // If we have in-order queue with discarded events then we want to
          // treat this event as regular event. We insert a barrier in the next
          // command list to wait for this event.
          LastCommandEvent = HostVisibleEvent;
        } else {
          // For all other queues treat this as a special event and indicate no
          // cleanup is needed.
          // TODO: always treat this host event as a regular event.
          UR_CALL(piEventReleaseInternal(HostVisibleEvent));
          HostVisibleEvent->CleanedUp = true;
        }

        // Finally set to signal the host-visible event at the end of the
        // command-list after a barrier that waits for all commands
        // completion.
        if (doReuseDiscardedEvents() && LastCommandEvent &&
            LastCommandEvent->IsDiscarded) {
          // If we the last event is discarded then we already have a barrier
          // inserted, so just signal the event.
          ZE2UR_CALL(zeCommandListAppendSignalEvent,
                  (CommandList->first, HostVisibleEvent->ZeEvent));
        } else {
          ZE2UR_CALL(zeCommandListAppendBarrier,
                  (CommandList->first, HostVisibleEvent->ZeEvent, 0, nullptr));
        }
      } else {
        // If we don't have host visible proxy then signal event if needed.
        this->signalEventFromCmdListIfLastEventDiscarded(CommandList);
      }
    } else {
      // If we don't have host visible proxy then signal event if needed.
      this->signalEventFromCmdListIfLastEventDiscarded(CommandList);
    }

    // Close the command list and have it ready for dispatch.
    ZE2UR_CALL(zeCommandListClose, (CommandList->first));
    this->LastUsedCommandList = CommandListMap.end();
    // Offload command list to the GPU for asynchronous execution
    auto ZeCommandList = CommandList->first;
    auto ZeResult = ZE_CALL_NOCHECK(
        zeCommandQueueExecuteCommandLists,
        (ZeCommandQueue, 1, &ZeCommandList, CommandList->second.ZeFence));
    if (ZeResult != ZE_RESULT_SUCCESS) {
      this->Healthy = false;
      if (ZeResult == ZE_RESULT_ERROR_UNKNOWN) {
        // Turn into a more informative end-user error.
        return UR_RESULT_ERROR_UNKNOWN;
      }
      return ze2urResult(ZeResult);
    }
  }

  // Check global control to make every command blocking for debugging.
  if (IsBlocking || (ZeSerialize & ZeSerializeBlock) != 0) {
    if (Device->useImmediateCommandLists()) {
      synchronize();
    } else {
      // Wait until command lists attached to the command queue are executed.
      ZE2UR_CALL(zeHostSynchronize, (ZeCommandQueue));
    }
  }
  return UR_RESULT_SUCCESS;
}

bool _ur_queue_handle_t::doReuseDiscardedEvents() {
  return ReuseDiscardedEvents && isInOrderQueue() && isDiscardEvents();
}

ur_result_t _ur_queue_handle_t::resetDiscardedEvent(pi_command_list_ptr_t CommandList) {
  if (LastCommandEvent && LastCommandEvent->IsDiscarded) {
    ZE2UR_CALL(zeCommandListAppendBarrier,
            (CommandList->first, nullptr, 1, &(LastCommandEvent->ZeEvent)));
    ZE2UR_CALL(zeCommandListAppendEventReset,
            (CommandList->first, LastCommandEvent->ZeEvent));

    // Create new pi_event but with the same ze_event_handle_t. We are going
    // to use this pi_event for the next command with discarded event.
    _ur_event_handle_t *PiEvent;
    try {
      PiEvent = new _ur_event_handle_t(LastCommandEvent->ZeEvent,
                              LastCommandEvent->ZeEventPool,
                              reinterpret_cast<ur_context_handle_t>(Context),
                              PI_COMMAND_TYPE_USER, true);
    } catch (const std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }

    if (LastCommandEvent->isHostVisible())
      PiEvent->HostVisibleEvent = reinterpret_cast<ur_event_handle_t>(PiEvent);

    UR_CALL(addEventToQueueCache(reinterpret_cast<ur_event_handle_t>(PiEvent)));
  }

  return UR_RESULT_SUCCESS;
}

ur_result_t _ur_queue_handle_t::addEventToQueueCache(ur_event_handle_t Event) {
  auto Cache = Event->isHostVisible() ? &EventCaches[0] : &EventCaches[1];
  Cache->emplace_back(Event);
  return UR_RESULT_SUCCESS;
}

void _ur_queue_handle_t::active_barriers::add(ur_event_handle_t &Event) {
  Event->RefCount.increment();
  Events.push_back(Event);
}

ur_result_t _ur_queue_handle_t::active_barriers::clear() {
  for (const auto &Event : Events)
    UR_CALL(piEventReleaseInternal(Event));
  Events.clear();
  return UR_RESULT_SUCCESS;
}

ur_result_t piQueueReleaseInternal(pi_queue Queue) {
  // PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  ur_queue_handle_t UrQueue = reinterpret_cast<ur_queue_handle_t>(Queue);

  if (!UrQueue->RefCount.decrementAndTest())
    return UR_RESULT_SUCCESS;

  for (auto &Cache : UrQueue->EventCaches)
    for (auto &Event : Cache)
      UR_CALL(piEventReleaseInternal(Event));

  if (UrQueue->OwnZeCommandQueue) {
    for (auto &QueueMap :
         {UrQueue->ComputeQueueGroupsByTID, UrQueue->CopyQueueGroupsByTID})
      for (auto &QueueGroup : QueueMap)
        for (auto &ZeQueue : QueueGroup.second.ZeQueues)
          if (ZeQueue)
            ZE2UR_CALL(zeCommandQueueDestroy, (ZeQueue));
  }

  zePrint("piQueueRelease(compute) NumTimesClosedFull %d, "
          "NumTimesClosedEarly %d\n",
          UrQueue->ComputeCommandBatch.NumTimesClosedFull,
          UrQueue->ComputeCommandBatch.NumTimesClosedEarly);
  zePrint("piQueueRelease(copy) NumTimesClosedFull %d, NumTimesClosedEarly "
          "%d\n",
          UrQueue->CopyCommandBatch.NumTimesClosedFull,
          UrQueue->CopyCommandBatch.NumTimesClosedEarly);

  delete UrQueue;

  return UR_RESULT_SUCCESS;
}

bool _ur_queue_handle_t::isBatchingAllowed(bool IsCopy) const {
  auto &CommandBatch = IsCopy ? CopyCommandBatch : ComputeCommandBatch;
  return (CommandBatch.QueueBatchSize > 0 &&
          ((ZeSerialize & ZeSerializeBlock) == 0));
}

bool _ur_queue_handle_t::isDiscardEvents() const {
  return ((this->Properties & PI_EXT_ONEAPI_QUEUE_FLAG_DISCARD_EVENTS) != 0);
}

bool _ur_queue_handle_t::isPriorityLow() const {
  return ((this->Properties & PI_EXT_ONEAPI_QUEUE_FLAG_PRIORITY_LOW) != 0);
}

bool _ur_queue_handle_t::isPriorityHigh() const {
  return ((this->Properties & PI_EXT_ONEAPI_QUEUE_FLAG_PRIORITY_HIGH) != 0);
}

bool _ur_queue_handle_t::isInOrderQueue() const {
  // If out-of-order queue property is not set, then this is a in-order queue.
  return ((this->Properties & PI_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE) ==
          0);
}

// Helper function to perform the necessary cleanup of the events from reset cmd
// list.
ur_result_t
CleanupEventListFromResetCmdList(std::vector<ur_event_handle_t> &EventListToCleanup,
                                 bool QueueLocked) {
  for (auto &Event : EventListToCleanup) {
    // We don't need to synchronize the events since the fence associated with
    // the command list was synchronized.
    {
      std::scoped_lock<pi_shared_mutex> EventLock(Event->Mutex);
      Event->Completed = true;
    }
    UR_CALL(CleanupCompletedEvent(Event, QueueLocked));
    // This event was removed from the command list, so decrement ref count
    // (it was incremented when they were added to the command list).
    UR_CALL(piEventReleaseInternal(Event));
  }
  return UR_RESULT_SUCCESS;
}

// Wait on all operations in flight on this Queue.
// The caller is expected to hold a lock on the Queue.
// For standard commandlists sync the L0 queues directly.
// For immediate commandlists add barriers to all commandlists associated
// with the Queue. An alternative approach would be to wait on all Events
// associated with the in-flight operations.
// TODO: Event release in immediate commandlist mode is driven by the SYCL
// runtime. Need to investigate whether relase can be done earlier, at sync
// points such as this, to reduce total number of active Events.
ur_result_t _ur_queue_handle_t::synchronize() {
  if (!Healthy)
    return UR_RESULT_SUCCESS;

  auto syncImmCmdList = [](_ur_queue_handle_t *Queue, pi_command_list_ptr_t ImmCmdList) {
    if (ImmCmdList == Queue->CommandListMap.end())
      return UR_RESULT_SUCCESS;

    ur_event_handle_t Event;
    ur_result_t Res = createEventAndAssociateQueue(
        reinterpret_cast<ur_queue_handle_t>(Queue), &Event, PI_COMMAND_TYPE_USER, ImmCmdList, false);
    if (Res != UR_RESULT_SUCCESS)
      return Res;
    auto zeEvent = Event->ZeEvent;
    ZE2UR_CALL(zeCommandListAppendBarrier,
            (ImmCmdList->first, zeEvent, 0, nullptr));
    ZE2UR_CALL(zeHostSynchronize, (zeEvent));
    Event->Completed = true;
#if 0
    UR_CALL(piEventRelease(Event));
#endif
    // Cleanup all events from the synced command list.
    auto EventListToCleanup = std::move(ImmCmdList->second.EventList);
    ImmCmdList->second.EventList.clear();
    CleanupEventListFromResetCmdList(EventListToCleanup, true);
    return UR_RESULT_SUCCESS;
  };

  for (auto &QueueMap : {ComputeQueueGroupsByTID, CopyQueueGroupsByTID})
    for (auto &QueueGroup : QueueMap) {
      if (Device->useImmediateCommandLists()) {
        for (auto ImmCmdList : QueueGroup.second.ImmCmdLists)
          syncImmCmdList(this, ImmCmdList);
      } else {
        for (auto &ZeQueue : QueueGroup.second.ZeQueues)
          if (ZeQueue)
            ZE2UR_CALL(zeHostSynchronize, (ZeQueue));
      }
    }
  LastCommandEvent = nullptr;

  // With the entire queue synchronized, the active barriers must be done so we
  // can remove them.
  if (auto Res = ActiveBarriers.clear())
    return Res;

  return UR_RESULT_SUCCESS;
}

ur_event_handle_t _ur_queue_handle_t::getEventFromQueueCache(bool HostVisible) {
  auto Cache = HostVisible ? &EventCaches[0] : &EventCaches[1];

  // If we don't have any events, return nullptr.
  // If we have only a single event then it was used by the last command and we
  // can't use it now because we have to enforce round robin between two events.
  if (Cache->size() < 2)
    return nullptr;

  // If there are two events then return an event from the beginning of the list
  // since event of the last command is added to the end of the list.
  auto It = Cache->begin();
  ur_event_handle_t RetEvent = *It;
  Cache->erase(It);
  return RetEvent;
}


ur_result_t createEventAndAssociateQueue(
    ur_queue_handle_t Queue, ur_event_handle_t *Event, pi_command_type CommandType,
    pi_command_list_ptr_t CommandList, bool IsInternal,
    bool ForceHostVisible) {

  if (!ForceHostVisible)
    ForceHostVisible = DeviceEventsSetting == AllHostVisible;

  // If event is discarded then try to get event from the queue cache.
  *Event =
      IsInternal ? Queue->getEventFromQueueCache(ForceHostVisible) : nullptr;

  if (*Event == nullptr)
    UR_CALL(EventCreate(Queue->Context, Queue, ForceHostVisible, Event));

  (*Event)->Queue = Queue;
  (*Event)->CommandType = CommandType;
  (*Event)->IsDiscarded = IsInternal;
  (*Event)->CommandList = CommandList;
  // Discarded event doesn't own ze_event, it is used by multiple pi_event
  // objects. We destroy corresponding ze_event by releasing events from the
  // events cache at queue destruction. Event in the cache owns the Level Zero
  // event.
  if (IsInternal)
    (*Event)->OwnZeEvent = false;

  // Append this Event to the CommandList, if any
  if (CommandList != Queue->CommandListMap.end()) {
    CommandList->second.append(*Event);
    (*Event)->RefCount.increment();
  }

  // We need to increment the reference counter here to avoid pi_queue
  // being released before the associated pi_event is released because
  // piEventRelease requires access to the associated pi_queue.
  // In piEventRelease, the reference counter of the Queue is decremented
  // to release it.
  Queue->RefCount.increment();

  // SYCL RT does not track completion of the events, so it could
  // release a PI event as soon as that's not being waited in the app.
  // But we have to ensure that the event is not destroyed before
  // it is really signalled, so retain it explicitly here and
  // release in CleanupCompletedEvent(Event).
  // If the event is internal then don't increment the reference count as this
  // event will not be waited/released by SYCL RT, so it must be destroyed by
  // EventRelease in resetCommandList.
#if 0
  if (!IsInternal)
    UR_CALL(piEventRetain(*Event));
#endif

  return UR_RESULT_SUCCESS;
}

void _ur_queue_handle_t::CaptureIndirectAccesses() {
  for (auto &Kernel : KernelsToBeSubmitted) {
    if (!Kernel->hasIndirectAccess())
      continue;

    auto &Contexts = Device->Platform->Contexts;
    for (auto &Ctx : Contexts) {
      for (auto &Elem : Ctx->MemAllocs) {
        const auto &Pair = Kernel->MemAllocs.insert(&Elem);
        // Kernel is referencing this memory allocation from now.
        // If this memory allocation was already captured for this kernel, it
        // means that kernel is submitted several times. Increase reference
        // count only once because we release all allocations only when
        // SubmissionsCount turns to 0. We don't want to know how many times
        // allocation was retained by each submission.
        if (Pair.second)
          Elem.second.RefCount.increment();
      }
    }
    Kernel->SubmissionsCount++;
  }
  KernelsToBeSubmitted.clear();
}

ur_result_t _ur_queue_handle_t::signalEventFromCmdListIfLastEventDiscarded(
    pi_command_list_ptr_t CommandList) {
  // We signal new event at the end of command list only if we have queue with
  // discard_events property and the last command event is discarded.
  if (!(doReuseDiscardedEvents() && LastCommandEvent &&
        LastCommandEvent->IsDiscarded))
    return UR_RESULT_SUCCESS;

  ur_event_handle_t Event;
  UR_CALL(createEventAndAssociateQueue(
      reinterpret_cast<ur_queue_handle_t>(this), &Event, PI_COMMAND_TYPE_USER, CommandList,
      /* IsDiscarded */ false, /* ForceHostVisible */ false))
#if 0
  UR_CALL(piEventReleaseInternal(Event));
#endif
  LastCommandEvent = Event;

  ZE2UR_CALL(zeCommandListAppendSignalEvent, (CommandList->first, Event->ZeEvent));
  return UR_RESULT_SUCCESS;
}


ur_result_t _ur_queue_handle_t::executeOpenCommandList(bool IsCopy) {
  auto &CommandBatch = IsCopy ? CopyCommandBatch : ComputeCommandBatch;
  // If there are any commands still in the open command list for this
  // queue, then close and execute that command list now.
  if (hasOpenCommandList(IsCopy)) {
    adjustBatchSizeForPartialBatch(IsCopy);
    auto Res = executeCommandList(CommandBatch.OpenCommandList, false, false);
    CommandBatch.OpenCommandList = CommandListMap.end();
    return Res;
  }

  return UR_RESULT_SUCCESS;
}

ur_result_t _ur_queue_handle_t::resetCommandList(pi_command_list_ptr_t CommandList,
                                      bool MakeAvailable,
                                      std::vector<ur_event_handle_t> &EventListToCleanup,
                                      bool CheckStatus) {
  bool UseCopyEngine = CommandList->second.isCopy(reinterpret_cast<ur_queue_handle_t>(this));

  // Immediate commandlists do not have an associated fence.
  if (CommandList->second.ZeFence != nullptr) {
    // Fence had been signalled meaning the associated command-list completed.
    // Reset the fence and put the command list into a cache for reuse in PI
    // calls.
    ZE2UR_CALL(zeFenceReset, (CommandList->second.ZeFence));
    ZE2UR_CALL(zeCommandListReset, (CommandList->first));
    CommandList->second.ZeFenceInUse = false;
  }

  auto &EventList = CommandList->second.EventList;
  // Check if standard commandlist or fully synced in-order queue.
  // If one of those conditions is met then we are sure that all events are
  // completed so we don't need to check event status.
  if (!CheckStatus || CommandList->second.ZeFence != nullptr ||
      (isInOrderQueue() && !LastCommandEvent)) {
    // Remember all the events in this command list which needs to be
    // released/cleaned up and clear event list associated with command list.
    std::move(std::begin(EventList), std::end(EventList),
              std::back_inserter(EventListToCleanup));
    EventList.clear();
  } else if (!isDiscardEvents()) {
    // For immediate commandlist reset only those events that have signalled.
    // If events in the queue are discarded then we can't check their status.
    for (auto it = EventList.begin(); it != EventList.end();) {
      std::scoped_lock<pi_shared_mutex> EventLock((*it)->Mutex);
      ze_result_t ZeResult =
          (*it)->Completed
              ? ZE_RESULT_SUCCESS
              : ZE_CALL_NOCHECK(zeEventQueryStatus, ((*it)->ZeEvent));
      // Break early as soon as we found first incomplete event because next
      // events are submitted even later. We are not trying to find all
      // completed events here because it may be costly. I.e. we are checking
      // only elements which are most likely completed because they were
      // submitted earlier. It is guaranteed that all events will be eventually
      // cleaned up at queue sync/release.
      if (ZeResult == ZE_RESULT_NOT_READY)
        break;

      if (ZeResult != ZE_RESULT_SUCCESS)
        return ze2urResult(ZeResult);

      EventListToCleanup.push_back(std::move((*it)));
      it = EventList.erase(it);
    }
  }

  // Standard commandlists move in and out of the cache as they are recycled.
  // Immediate commandlists are always available.
  if (CommandList->second.ZeFence != nullptr && MakeAvailable) {
    std::scoped_lock<pi_mutex> Lock(this->Context->ZeCommandListCacheMutex);
    auto &ZeCommandListCache =
        UseCopyEngine
            ? this->Context->ZeCopyCommandListCache[this->Device->ZeDevice]
            : this->Context->ZeComputeCommandListCache[this->Device->ZeDevice];
    ZeCommandListCache.push_back(CommandList->first);
  }

  return UR_RESULT_SUCCESS;
}

bool pi_command_list_info_t::isCopy(ur_queue_handle_t Queue) const {
  return ZeQueueGroupOrdinal !=
         (uint32_t)Queue->Device
             ->QueueGroup[_ur_device_handle_t::queue_group_info_t::type::Compute]
             .ZeOrdinal;
}

pi_command_list_ptr_t _ur_queue_handle_t::eventOpenCommandList(ur_event_handle_t Event) {
  using IsCopy = bool;

  if (Device->useImmediateCommandLists()) {
    // When using immediate commandlists there are no open command lists.
    return CommandListMap.end();
  }

  if (hasOpenCommandList(IsCopy{false})) {
    const auto &ComputeEventList =
        ComputeCommandBatch.OpenCommandList->second.EventList;
    if (std::find(ComputeEventList.begin(), ComputeEventList.end(), Event) !=
        ComputeEventList.end())
      return ComputeCommandBatch.OpenCommandList;
  }
  if (hasOpenCommandList(IsCopy{true})) {
    const auto &CopyEventList =
        CopyCommandBatch.OpenCommandList->second.EventList;
    if (std::find(CopyEventList.begin(), CopyEventList.end(), Event) !=
        CopyEventList.end())
      return CopyCommandBatch.OpenCommandList;
  }
  return CommandListMap.end();
}

_ur_queue_handle_t::pi_queue_group_t &_ur_queue_handle_t::getQueueGroup(bool UseCopyEngine) {
  auto &Map = (UseCopyEngine ? CopyQueueGroupsByTID : ComputeQueueGroupsByTID);
  auto &InitialGroup = Map.begin()->second;

  // Check if thread-specifc immediate commandlists are requested.
  if (Device->useImmediateCommandLists() == _ur_device_handle_t::PerThreadPerQueue) {
    // Thread id is used to create separate imm cmdlists per thread.
    auto Result = Map.insert({std::this_thread::get_id(), InitialGroup});
    auto &QueueGroupRef = Result.first->second;
    // If an entry for this thread does not exists, create an entry.
    if (Result.second) {
      // Create space for immediate commandlists, which are created on demand.
      QueueGroupRef.ImmCmdLists = std::vector<pi_command_list_ptr_t>(
          InitialGroup.ZeQueues.size(), CommandListMap.end());
    }
    return QueueGroupRef;
  }

  // If not PerThreadPerQueue then use the groups from Queue creation time.
  return InitialGroup;
}

// Return the index of the next queue to use based on a
// round robin strategy and the queue group ordinal.
uint32_t _ur_queue_handle_t::pi_queue_group_t::getQueueIndex(uint32_t *QueueGroupOrdinal,
                                                    uint32_t *QueueIndex,
                                                    bool QueryOnly) {
  auto CurrentIndex = NextIndex;

  if (!QueryOnly) {
    ++NextIndex;
    if (NextIndex > UpperIndex)
      NextIndex = LowerIndex;
  }

  // Find out the right queue group ordinal (first queue might be "main" or
  // "link")
  auto QueueType = Type;
  if (QueueType != queue_type::Compute)
    QueueType = (CurrentIndex == 0 && Queue->Device->hasMainCopyEngine())
                    ? queue_type::MainCopy
                    : queue_type::LinkCopy;

  *QueueGroupOrdinal = Queue->Device->QueueGroup[QueueType].ZeOrdinal;
  // Adjust the index to the L0 queue group since we represent "main" and
  // "link"
  // L0 groups with a single copy group ("main" would take "0" index).
  auto ZeCommandQueueIndex = CurrentIndex;
  if (QueueType == queue_type::LinkCopy && Queue->Device->hasMainCopyEngine()) {
    ZeCommandQueueIndex -= 1;
  }
  *QueueIndex = ZeCommandQueueIndex;

  return CurrentIndex;
}
