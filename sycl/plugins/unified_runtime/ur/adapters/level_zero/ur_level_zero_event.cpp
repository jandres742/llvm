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

#include "ur_level_zero_common.hpp"
#include "ur_level_zero_event.hpp"
#include <ur_bindings.hpp>

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueEventsWait(
    ur_queue_handle_t hQueue,     ///< [in] handle of the queue object
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t *
        phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                         ///< pointer to a list of events that must be complete
                         ///< before this command can be executed. If nullptr,
                         ///< the numEventsInWaitList must be 0, indicating that
                         ///< all previously enqueued commands must be complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueEventsWaitWithBarrier(
    ur_queue_handle_t hQueue,     ///< [in] handle of the queue object
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t *
        phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                         ///< pointer to a list of events that must be complete
                         ///< before this command can be executed. If nullptr,
                         ///< the numEventsInWaitList must be 0, indicating that
                         ///< all previously enqueued commands must be complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventGetInfo(
    ur_event_handle_t hEvent, ///< [in] handle of the event object
    ur_event_info_t propName, ///< [in] the name of the event property to query
    size_t propValueSize, ///< [in] size in bytes of the event property value
    void *pPropValue,     ///< [out][optional] value of the event property
    size_t
        *pPropValueSizeRet ///< [out][optional] bytes returned in event property
);

UR_APIEXPORT ur_result_t UR_APICALL urEventGetProfilingInfo(
    ur_event_handle_t hEvent, ///< [in] handle of the event object
    ur_profiling_info_t
        propName, ///< [in] the name of the profiling property to query
    size_t
        propValueSize, ///< [in] size in bytes of the profiling property value
    void *pPropValue,  ///< [out][optional] value of the profiling property
    size_t *pPropValueSizeRet ///< [out][optional] pointer to the actual size in
                              ///< bytes returned in propValue
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventWait(
    uint32_t numEvents, ///< [in] number of events in the event list
    const ur_event_handle_t
        *phEventWaitList ///< [in][range(0, numEvents)] pointer to a list of
                         ///< events to wait for completion
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventRetain(
    ur_event_handle_t hEvent ///< [in] handle of the event object
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventRelease(
    ur_event_handle_t hEvent ///< [in] handle of the event object
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventGetNativeHandle(
    ur_event_handle_t hEvent, ///< [in] handle of the event.
    ur_native_handle_t
        *phNativeEvent ///< [out] a pointer to the native handle of the event.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventCreateWithNativeHandle(
    ur_native_handle_t hNativeEvent, ///< [in] the native handle of the event.
    ur_context_handle_t hContext,    ///< [in] handle of the context object
    ur_event_handle_t
        *phEvent ///< [out] pointer to the handle of the event object created.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventSetCallback(
    ur_event_handle_t hEvent,       ///< [in] handle of the event object
    ur_execution_info_t execStatus, ///< [in] execution status of the event
    ur_event_callback_t pfnNotify,  ///< [in] execution status of the event
    void *pUserData ///< [in][out][optional] pointer to data to be passed to
                    ///< callback.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t piEventReleaseInternal(ur_event_handle_t Event) {
  // PI_ASSERT(Event, PI_ERROR_INVALID_EVENT);

  if (!Event->RefCount.decrementAndTest())
    return UR_RESULT_SUCCESS;

  if (Event->CommandType == PI_COMMAND_TYPE_MEM_BUFFER_UNMAP &&
      Event->CommandData) {
    // Free the memory allocated in the piEnqueueMemBufferMap.
    if (auto Res = ZeMemFreeHelper(Event->Context, Event->CommandData))
      return Res;
    Event->CommandData = nullptr;
  }
  if (Event->OwnZeEvent) {
    if (DisableEventsCaching) {
      ZE2UR_CALL(zeEventDestroy, (Event->ZeEvent));
      auto Context = Event->Context;
      if (auto Res = static_cast<ur_result_t>(Context->decrementUnreleasedEventsInPool(Event)))
        return Res;
    }
  }
  // It is possible that host-visible event was never created.
  // In case it was check if that's different from this same event
  // and release a reference to it.
  if (Event->HostVisibleEvent && Event->HostVisibleEvent != Event) {
    // Decrement ref-count of the host-visible proxy event.
    UR_CALL(piEventReleaseInternal(Event->HostVisibleEvent));
  }

  // Save pointer to the queue before deleting/resetting event.
  // When we add an event to the cache we need to check whether profiling is
  // enabled or not, so we access properties of the queue and that's why queue
  // must released later.
#if 0
  auto Queue = Event->Queue;
#endif
  if (DisableEventsCaching || !Event->OwnZeEvent) {
    delete Event;
  } else {
    Event->Context->addEventToContextCache(Event);
  }

  // We intentionally incremented the reference counter when an event is
  // created so that we can avoid pi_queue is released before the associated
  // pi_event is released. Here we have to decrement it so pi_queue
  // can be released successfully.
#if 0
  if (Queue) {
    UR_CALL(piQueueReleaseInternal(Queue));
  }
#endif

  return UR_RESULT_SUCCESS;
}

// Helper function to implement zeHostSynchronize.
// The behavior is to avoid infinite wait during host sync under ZE_DEBUG.
// This allows for a much more responsive debugging of hangs.
//
template <typename T, typename Func>
ze_result_t zeHostSynchronizeImpl(Func Api, T Handle) {
  if (!ZeDebug) {
    return Api(Handle, UINT64_MAX);
  }

  ze_result_t R;
  while ((R = Api(Handle, 1000)) == ZE_RESULT_NOT_READY)
    ;
  return R;
}

// Template function to do various types of host synchronizations.
// This is intended to be used instead of direct calls to specific
// Level-Zero synchronization APIs.
//
template <typename T> ze_result_t zeHostSynchronize(T Handle);
template <> ze_result_t zeHostSynchronize(ze_event_handle_t Handle) {
  return zeHostSynchronizeImpl(zeEventHostSynchronize, Handle);
}
template <> ze_result_t zeHostSynchronize(ze_command_queue_handle_t Handle) {
  return zeHostSynchronizeImpl(zeCommandQueueSynchronize, Handle);
}

// Perform any necessary cleanup after an event has been signalled.
// This currently makes sure to release any kernel that may have been used by
// the event, updates the last command event in the queue and cleans up all dep
// events of the event.
// If the caller locks queue mutex then it must pass 'true' to QueueLocked.
ur_result_t CleanupCompletedEvent(ur_event_handle_t Event, bool QueueLocked) {
  ur_kernel_handle_t AssociatedKernel = nullptr;
  // List of dependent events.
  std::list<ur_event_handle_t> EventsToBeReleased;
  ur_queue_handle_t AssociatedQueue = nullptr;
  {
    std::scoped_lock<pi_shared_mutex> EventLock(Event->Mutex);
    // Exit early of event was already cleanedup.
    if (Event->CleanedUp)
      return UR_RESULT_SUCCESS;

    AssociatedQueue = Event->Queue;

    // Remember the kernel associated with this event if there is one. We are
    // going to release it later.
    if (Event->CommandType == PI_COMMAND_TYPE_NDRANGE_KERNEL &&
        Event->CommandData) {
      AssociatedKernel = reinterpret_cast<ur_kernel_handle_t>(Event->CommandData);
      Event->CommandData = nullptr;
    }

    // Make a list of all the dependent events that must have signalled
    // because this event was dependent on them.
    Event->WaitList.collectEventsForReleaseAndDestroyPiZeEventList(
        EventsToBeReleased);

    Event->CleanedUp = true;
  }

  auto ReleaseIndirectMem = [](ur_kernel_handle_t Kernel) {
    if (IndirectAccessTrackingEnabled) {
      // piKernelRelease is called by CleanupCompletedEvent(Event) as soon as
      // kernel execution has finished. This is the place where we need to
      // release memory allocations. If kernel is not in use (not submitted by
      // some other thread) then release referenced memory allocations. As a
      // result, memory can be deallocated and context can be removed from
      // container in the platform. That's why we need to lock a mutex here.
      ur_platform_handle_t Plt = Kernel->Program->Context->getPlatform();
      std::scoped_lock<pi_shared_mutex> ContextsLock(Plt->ContextsMutex);

      if (--Kernel->SubmissionsCount == 0) {
        // Kernel is not submitted for execution, release referenced memory
        // allocations.
        for (auto &MemAlloc : Kernel->MemAllocs) {
          // std::pair<void *const, MemAllocRecord> *, Hash
          USMFreeHelper(MemAlloc->second.Context, MemAlloc->first,
                        MemAlloc->second.OwnZeMemHandle);
        }
        Kernel->MemAllocs.clear();
      }
    }
  };

  // We've reset event data members above, now cleanup resources.
  if (AssociatedKernel) {
    ReleaseIndirectMem(AssociatedKernel);
    // UR_CALL(piKernelRelease(AssociatedKernel));
  }

  if (AssociatedQueue) {
    {
      // Lock automatically releases when this goes out of scope.
      std::unique_lock<pi_shared_mutex> QueueLock(AssociatedQueue->Mutex,
                                                  std::defer_lock);
      if (!QueueLocked)
        QueueLock.lock();

      // If this event was the LastCommandEvent in the queue, being used
      // to make sure that commands were executed in-order, remove this.
      // If we don't do this, the event can get released and freed leaving
      // a dangling pointer to this event.  It could also cause unneeded
      // already finished events to show up in the wait list.
      if (AssociatedQueue->LastCommandEvent == Event) {
        AssociatedQueue->LastCommandEvent = nullptr;
      }
    }

    // Release this event since we explicitly retained it on creation and
    // association with queue. Events which don't have associated queue doesn't
    // require this release because it means that they are not created using
    // createEventAndAssociateQueue, i.e. additional retain was not made.
    UR_CALL(piEventReleaseInternal(Event));
  }

  // The list of dependent events will be appended to as we walk it so that this
  // algorithm doesn't go recursive due to dependent events themselves being
  // dependent on other events forming a potentially very deep tree, and deep
  // recursion.  That turned out to be a significant problem with the recursive
  // code that preceded this implementation.
  while (!EventsToBeReleased.empty()) {
    ur_event_handle_t DepEvent = EventsToBeReleased.front();
    DepEvent->Completed = true;
    EventsToBeReleased.pop_front();

    ur_kernel_handle_t DepEventKernel = nullptr;
    {
      std::scoped_lock<pi_shared_mutex> DepEventLock(DepEvent->Mutex);
      DepEvent->WaitList.collectEventsForReleaseAndDestroyPiZeEventList(
          EventsToBeReleased);
      if (IndirectAccessTrackingEnabled) {
        // DepEvent has finished, we can release the associated kernel if there
        // is one. This is the earliest place we can do this and it can't be
        // done twice, so it is safe. Lock automatically releases when this goes
        // out of scope.
        // TODO: this code needs to be moved out of the guard.
        if (DepEvent->CommandType == PI_COMMAND_TYPE_NDRANGE_KERNEL &&
            DepEvent->CommandData) {
          DepEventKernel = reinterpret_cast<ur_kernel_handle_t>(DepEvent->CommandData);
          DepEvent->CommandData = nullptr;
        }
      }
    }
    if (DepEventKernel) {
      ReleaseIndirectMem(DepEventKernel);
      // UR_CALL(piKernelRelease(DepEventKernel));
    }
    UR_CALL(piEventReleaseInternal(DepEvent));
  }

  return UR_RESULT_SUCCESS;
}

// Helper function for creating a PI event.
// The "Queue" argument specifies the PI queue where a command is submitted.
// The "HostVisible" argument specifies if event needs to be allocated from
// a host-visible pool.
//
ur_result_t EventCreate(ur_context_handle_t Context, ur_queue_handle_t Queue,
                             bool HostVisible, ur_event_handle_t *RetEvent) {
  bool ProfilingEnabled =
      !Queue || (Queue->Properties & PI_QUEUE_FLAG_PROFILING_ENABLE) != 0;

  if (auto CachedEvent =
          Context->getEventFromContextCache(HostVisible, ProfilingEnabled)) {
    *RetEvent = CachedEvent;
    return UR_RESULT_SUCCESS;
  }

  ze_event_handle_t ZeEvent;
  ze_event_pool_handle_t ZeEventPool = {};

  size_t Index = 0;

  if (auto Res = Context->getFreeSlotInExistingOrNewPool(
          ZeEventPool, Index, HostVisible, ProfilingEnabled))
    return Res;

  ZeStruct<ze_event_desc_t> ZeEventDesc;
  ZeEventDesc.index = Index;
  ZeEventDesc.wait = 0;

  if (HostVisible) {
    ZeEventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
  } else {
    //
    // Set the scope to "device" for every event. This is sufficient for
    // global device access and peer device access. If needed to be seen on
    // the host we are doing special handling, see EventsScope options.
    //
    // TODO: see if "sub-device" (ZE_EVENT_SCOPE_FLAG_SUBDEVICE) can better be
    //       used in some circumstances.
    //
    ZeEventDesc.signal = 0;
  }

  ZE2UR_CALL(zeEventCreate, (ZeEventPool, &ZeEventDesc, &ZeEvent));

  try {
    // PI_ASSERT(RetEvent, PI_ERROR_INVALID_VALUE);

    *RetEvent = new ur_event_handle_t_(ZeEvent,
                              ZeEventPool,
                              reinterpret_cast<ur_context_handle_t>(Context),
                              PI_COMMAND_TYPE_USER, true);
  } catch (const std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  if (HostVisible)
    (*RetEvent)->HostVisibleEvent = reinterpret_cast<ur_event_handle_t>(*RetEvent);

  return UR_RESULT_SUCCESS;
}

ur_result_t _ur_event_handle_t::reset() {
  Queue = nullptr;
  CleanedUp = false;
  Completed = false;
  CommandData = nullptr;
  CommandType = PI_COMMAND_TYPE_USER;
  WaitList = {};
  RefCountExternal = 0;
  RefCount.reset();
  CommandList = std::nullopt;

  if (!isHostVisible())
    HostVisibleEvent = nullptr;

  ZE2UR_CALL(zeEventHostReset, (ZeEvent));
  return UR_RESULT_SUCCESS;
}


ur_result_t _pi_ze_event_list_t::createAndRetainPiZeEventList(
    uint32_t EventListLength,
                                         const ur_event_handle_t *EventList,
                                         ur_queue_handle_t CurQueue, bool UseCopyEngine) {
  this->Length = 0;
  this->ZeEventList = nullptr;
  this->PiEventList = nullptr;

  if (CurQueue->isInOrderQueue() && CurQueue->LastCommandEvent != nullptr) {
    if (CurQueue->Device->useImmediateCommandLists()) {
      if (ReuseDiscardedEvents && CurQueue->isDiscardEvents()) {
        // If queue is in-order with discarded events and if
        // new command list is different from the last used command list then
        // signal new event from the last immediate command list. We are going
        // to insert a barrier in the new command list waiting for that event.
        auto QueueGroup = CurQueue->getQueueGroup(UseCopyEngine);
        uint32_t QueueGroupOrdinal, QueueIndex;
        auto NextIndex =
            QueueGroup.getQueueIndex(&QueueGroupOrdinal, &QueueIndex,
                                     /*QueryOnly */ true);
        auto NextImmCmdList = QueueGroup.ImmCmdLists[NextIndex];
        if (CurQueue->LastUsedCommandList != CurQueue->CommandListMap.end() &&
            CurQueue->LastUsedCommandList != NextImmCmdList) {
          CurQueue->signalEventFromCmdListIfLastEventDiscarded(
              CurQueue->LastUsedCommandList);
        }
      }
    } else {
      // Ensure LastCommandEvent's batch is submitted if it is differrent
      // from the one this command is going to. If we reuse discarded events
      // then signalEventFromCmdListIfLastEventDiscarded will be called at batch
      // close if needed.
      const auto &OpenCommandList =
          CurQueue->eventOpenCommandList(CurQueue->LastCommandEvent);
      if (OpenCommandList != CurQueue->CommandListMap.end() &&
          OpenCommandList->second.isCopy(CurQueue) != UseCopyEngine) {

        if (auto Res = CurQueue->executeOpenCommandList(
                OpenCommandList->second.isCopy(CurQueue)))
          return Res;
      }
    }
  }

  // For in-order queues, every command should be executed only after the
  // previous command has finished. The event associated with the last
  // enqueued command is added into the waitlist to ensure in-order semantics.
  bool IncludeLastCommandEvent =
      CurQueue->isInOrderQueue() && CurQueue->LastCommandEvent != nullptr;

  // If the last event is discarded then we already have a barrier waiting for
  // that event, so must not include the last command event into the wait
  // list because it will cause waiting for event which was reset.
  if (ReuseDiscardedEvents && CurQueue->isDiscardEvents() &&
      CurQueue->LastCommandEvent && CurQueue->LastCommandEvent->IsDiscarded)
    IncludeLastCommandEvent = false;

  try {
    pi_uint32 TmpListLength = 0;

    if (IncludeLastCommandEvent) {
      this->ZeEventList = new ze_event_handle_t[EventListLength + 1];
      this->PiEventList = new ur_event_handle_t[EventListLength + 1];
      std::shared_lock<pi_shared_mutex> Lock(CurQueue->LastCommandEvent->Mutex);
      this->ZeEventList[0] = CurQueue->LastCommandEvent->ZeEvent;
      this->PiEventList[0] = CurQueue->LastCommandEvent;
      TmpListLength = 1;
    } else if (EventListLength > 0) {
      this->ZeEventList = new ze_event_handle_t[EventListLength];
      this->PiEventList = new ur_event_handle_t[EventListLength];
    }

    if (EventListLength > 0) {
      for (pi_uint32 I = 0; I < EventListLength; I++) {
#if 0
        PI_ASSERT(EventList[I] != nullptr, PI_ERROR_INVALID_VALUE);
#endif
        {
          std::shared_lock<pi_shared_mutex> Lock(EventList[I]->Mutex);
          if (EventList[I]->Completed)
            continue;

          // Poll of the host-visible events.
          auto HostVisibleEvent = EventList[I]->HostVisibleEvent;
          if (FilterEventWaitList && HostVisibleEvent) {
            auto Res = ZE_CALL_NOCHECK(zeEventQueryStatus,
                                       (HostVisibleEvent->ZeEvent));
            if (Res == ZE_RESULT_SUCCESS) {
              // Event has already completed, don't put it into the list
              continue;
            }
          }
        }

        auto Queue = EventList[I]->Queue;
        if (Queue) {
          // The caller of createAndRetainPiZeEventList must already hold
          // a lock of the CurQueue. Additionally lock the Queue if it
          // is different from CurQueue.
          // TODO: rework this to avoid deadlock when another thread is
          //       locking the same queues but in a different order.
          auto Lock = ((Queue == CurQueue)
                           ? std::unique_lock<pi_shared_mutex>()
                           : std::unique_lock<pi_shared_mutex>(Queue->Mutex));

          // If the event that is going to be waited is in an open batch
          // different from where this next command is going to be added,
          // then we have to force execute of that open command-list
          // to avoid deadlocks.
          //
          const auto &OpenCommandList =
              Queue->eventOpenCommandList(EventList[I]);
          if (OpenCommandList != Queue->CommandListMap.end()) {

            if (Queue == CurQueue &&
                OpenCommandList->second.isCopy(Queue) == UseCopyEngine) {
              // Don't force execute the batch yet since the new command
              // is going to the same open batch as the dependent event.
            } else {
              if (auto Res = Queue->executeOpenCommandList(
                      OpenCommandList->second.isCopy(Queue)))
                return Res;
            }
          }
        } else {
          // There is a dependency on an interop-event.
          // Similarily to the above to avoid dead locks ensure that
          // execution of all prior commands in the current command-
          // batch is visible to the host. This may not be the case
          // when we intended to have only last command in the batch
          // produce host-visible event, e.g.
          //
          //  event0 = interop event
          //  event1 = command1 (already in batch, no deps)
          //  event2 = command2 (is being added, dep on event0)
          //  event3 = signal host-visible event for the batch
          //  event1.wait()
          //  event0.signal()
          //
          // Make sure that event1.wait() will wait for a host-visible
          // event that is signalled before the command2 is enqueued.
          if (DeviceEventsSetting != AllHostVisible) {
            CurQueue->executeAllOpenCommandLists();
          }
        }

        std::shared_lock<pi_shared_mutex> Lock(EventList[I]->Mutex);
        this->ZeEventList[TmpListLength] = EventList[I]->ZeEvent;
        this->PiEventList[TmpListLength] = EventList[I];
        TmpListLength += 1;
      }
    }

    this->Length = TmpListLength;

  } catch (...) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  }

  for (pi_uint32 I = 0; I < this->Length; I++) {
    this->PiEventList[I]->RefCount.increment();
  }

  return UR_RESULT_SUCCESS;
}

ur_result_t _pi_ze_event_list_t::collectEventsForReleaseAndDestroyPiZeEventList(
    std::list<ur_event_handle_t> &EventsToBeReleased) {
  // acquire a lock before reading the length and list fields.
  // Acquire the lock, copy the needed data locally, and reset
  // the fields, then release the lock.
  // Only then do we do the actual actions to release and destroy,
  // holding the lock for the minimum time necessary.
  pi_uint32 LocLength = 0;
  ze_event_handle_t *LocZeEventList = nullptr;
  ur_event_handle_t *LocPiEventList = nullptr;

  {
    // acquire the lock and copy fields locally
    // Lock automatically releases when this goes out of scope.
    std::scoped_lock<pi_mutex> lock(this->PiZeEventListMutex);

    LocLength = Length;
    LocZeEventList = ZeEventList;
    LocPiEventList = PiEventList;

    Length = 0;
    ZeEventList = nullptr;
    PiEventList = nullptr;

    // release lock by ending scope.
  }

  for (pi_uint32 I = 0; I < LocLength; I++) {
    // Add the event to be released to the list
    EventsToBeReleased.push_back(LocPiEventList[I]);
  }

  if (LocZeEventList != nullptr) {
    delete[] LocZeEventList;
  }
  if (LocPiEventList != nullptr) {
    delete[] LocPiEventList;
  }

  return UR_RESULT_SUCCESS;
}

// Tells if this event is with profiling capabilities.
bool _ur_event_handle_t::isProfilingEnabled() const {
  return !Queue || // tentatively assume user events are profiling enabled
          (Queue->Properties & PI_QUEUE_FLAG_PROFILING_ENABLE) != 0;
}