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
#include <optional>
#include <mutex>

#include <sycl/detail/pi.h>
#include <ur/ur.hpp>
#include <ur_api.h>
#include <ze_api.h>
#include <zes_api.h>

#include "ur_level_zero_common.hpp"
#include "ur_level_zero_queue.hpp"

// This is an experimental option that allows to disable caching of events in
// the context.
const bool DisableEventsCaching = [] {
  const char *DisableEventsCachingFlag =
      std::getenv("SYCL_PI_LEVEL_ZERO_DISABLE_EVENTS_CACHING");
  if (!DisableEventsCachingFlag)
    return false;
  return std::stoi(DisableEventsCachingFlag) != 0;
}();

// This is an experimental option that allows reset and reuse of uncompleted
// events in the in-order queue with discard_events property.
const bool ReuseDiscardedEvents = [] {
  const char *ReuseDiscardedEventsFlag =
      std::getenv("SYCL_PI_LEVEL_ZERO_REUSE_DISCARDED_EVENTS");
  if (!ReuseDiscardedEventsFlag)
    return true;
  return std::stoi(ReuseDiscardedEventsFlag) > 0;
}();

// Maximum number of events that can be present in an event ZePool is captured
// here. Setting it to 256 gave best possible performance for several
// benchmarks.
const pi_uint32 MaxNumEventsPerPool = [] {
  const auto MaxNumEventsPerPoolEnv =
      std::getenv("ZE_MAX_NUMBER_OF_EVENTS_PER_EVENT_POOL");
  pi_uint32 Result =
      MaxNumEventsPerPoolEnv ? std::atoi(MaxNumEventsPerPoolEnv) : 256;
  if (Result <= 0)
    Result = 256;
  return Result;
}();

const bool FilterEventWaitList = [] {
  const char *Ret = std::getenv("SYCL_PI_LEVEL_ZERO_FILTER_EVENT_WAIT_LIST");
  const bool RetVal = Ret ? std::stoi(Ret) : 1;
  return RetVal;
}();


ur_result_t piEventReleaseInternal(ur_event_handle_t Event);

ur_result_t EventCreate(ur_context_handle_t Context, ur_queue_handle_t Queue,
                             bool HostVisible, ur_event_handle_t *RetEvent);



struct _pi_ze_event_list_t {
  // List of level zero events for this event list.
  ze_event_handle_t *ZeEventList = {nullptr};

  // List of pi_events for this event list.
  ur_event_handle_t *PiEventList = {nullptr};

  // length of both the lists.  The actual allocation of these lists
  // may be longer than this length.  This length is the actual number
  // of elements in the above arrays that are valid.
  uint32_t Length = {0};

  // A mutex is needed for destroying the event list.
  // Creation is already thread-safe because we only create the list
  // when an event is initially created.  However, it might be
  // possible to have multiple threads racing to destroy the list,
  // so this will be used to make list destruction thread-safe.
  pi_mutex PiZeEventListMutex;

  // Initialize this using the array of events in EventList, and retain
  // all the pi_events in the created data structure.
  // CurQueue is the pi_queue that the command with this event wait
  // list is going to be added to.  That is needed to flush command
  // batches for wait events that are in other queues.
  // UseCopyEngine indicates if the next command (the one that this
  // event wait-list is for) is going to go to copy or compute
  // queue. This is used to properly submit the dependent open
  // command-lists.
  ur_result_t createAndRetainPiZeEventList(uint32_t EventListLength,
                                         const ur_event_handle_t *EventList,
                                         ur_queue_handle_t CurQueue, bool UseCopyEngine);

  // Add all the events in this object's PiEventList to the end
  // of the list EventsToBeReleased. Destroy pi_ze_event_list_t data
  // structure fields making it look empty.
  ur_result_t collectEventsForReleaseAndDestroyPiZeEventList(
      std::list<ur_event_handle_t> &EventsToBeReleased);

  // Had to create custom assignment operator because the mutex is
  // not assignment copyable. Just field by field copy of the other
  // fields.
  _pi_ze_event_list_t &operator=(const _pi_ze_event_list_t &other) {
    if (this != &other) {
      this->ZeEventList = other.ZeEventList;
      this->PiEventList = other.PiEventList;
      this->Length = other.Length;
    }
    return *this;
  }
};

void printZeEventList(const _pi_ze_event_list_t &PiZeEventList);

struct _ur_event_handle_t : _pi_object {
  _ur_event_handle_t(ze_event_handle_t ZeEvent, ze_event_pool_handle_t ZeEventPool, ur_context_handle_t Context, pi_command_type CommandType, bool OwnZeEvent): 
    ZeEvent{ZeEvent}, OwnZeEvent{OwnZeEvent}, ZeEventPool{ZeEventPool}, Context{Context}, CommandType{CommandType}, CommandData{nullptr} {}

  // Level Zero event handle.
  ze_event_handle_t ZeEvent;

  // Indicates if we own the ZeEvent or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  bool OwnZeEvent;

  // Level Zero event pool handle.
  ze_event_pool_handle_t ZeEventPool;

  // In case we use device-only events this holds their host-visible
  // counterpart. If this event is itself host-visble then HostVisibleEvent
  // points to this event. If this event is not host-visible then this field can
  // be: 1) null, meaning that a host-visible event wasn't yet created 2) a PI
  // event created internally that host will actually be redirected
  //    to wait/query instead of this PI event.
  //
  // The HostVisibleEvent is a reference counted PI event and can be used more
  // than by just this one event, depending on the mode (see EventsScope).
  //
  ur_event_handle_t HostVisibleEvent = {nullptr};
  bool isHostVisible() const { return this == const_cast<const _ur_event_handle_t *>(reinterpret_cast<_ur_event_handle_t *>(HostVisibleEvent)); }

  // Provide direct access to Context, instead of going via queue.
  // Not every PI event has a queue, and we need a handle to Context
  // to get to event pool related information.
  ur_context_handle_t Context;

  // Keeps the command-queue and command associated with the event.
  // These are NULL for the user events.
  ur_queue_handle_t UrQueue = {nullptr};
  pi_command_type CommandType;

  // Opaque data to hold any data needed for CommandType.
  void *CommandData;

  // Command list associated with the pi_event.
  std::optional<pi_command_list_ptr_t> CommandList;

  // List of events that were in the wait list of the command that will
  // signal this event.  These events must be retained when the command is
  // enqueued, and must then be released when this event has signalled.
  // This list must be destroyed once the event has signalled.
  _pi_ze_event_list_t WaitList;

  // Tracks if the needed cleanup was already performed for
  // a completed event. This allows to control that some cleanup
  // actions are performed only once.
  //
  bool CleanedUp = {false};

  // Indicates that this PI event had already completed in the sense
  // that no other synchromization is needed. Note that the underlying
  // L0 event (if any) is not guranteed to have been signalled, or
  // being visible to the host at all.
  bool Completed = {false};

  // Indicates that this event is discarded, i.e. it is not visible outside of
  // plugin.
  bool IsDiscarded = {false};

  // Besides each PI object keeping a total reference count in
  // _pi_object::RefCount we keep special track of the event *external*
  // references. This way we are able to tell when the event is not referenced
  // externally anymore, i.e. it can't be passed as a dependency event to
  // piEnqueue* functions and explicitly waited meaning that we can do some
  // optimizations:
  // 1. For in-order queues we can reset and reuse event even if it was not yet
  // completed by submitting a reset command to the queue (since there are no
  // external references, we know that nobody can wait this event somewhere in
  // parallel thread or pass it as a dependency which may lead to hang)
  // 2. We can avoid creating host proxy event.
  // This counter doesn't track the lifetime of an event object. Even if it
  // reaches zero an event object may not be destroyed and can be used
  // internally in the plugin.
  std::atomic<uint32_t> RefCountExternal{0};

  bool hasExternalRefs() { return RefCountExternal != 0; }

  // Reset _pi_event object.
  ur_result_t reset();

  // Tells if this event is with profiling capabilities.
  bool isProfilingEnabled() const;

  // Get the host-visible event or create one and enqueue its signal.
  ur_result_t getOrCreateHostVisibleEvent(ze_event_handle_t &HostVisibleEvent);

};

// Helper function to implement zeHostSynchronize.
// The behavior is to avoid infinite wait during host sync under ZE_DEBUG.
// This allows for a much more responsive debugging of hangs.
//
template <typename T, typename Func>
ze_result_t zeHostSynchronizeImpl(Func Api, T Handle);

// Template function to do various types of host synchronizations.
// This is intended to be used instead of direct calls to specific
// Level-Zero synchronization APIs.
//
template <typename T> ze_result_t zeHostSynchronize(T Handle);
template <> ze_result_t zeHostSynchronize(ze_event_handle_t Handle);
template <> ze_result_t zeHostSynchronize(ze_command_queue_handle_t Handle);

// Perform any necessary cleanup after an event has been signalled.
// This currently makes sure to release any kernel that may have been used by
// the event, updates the last command event in the queue and cleans up all dep
// events of the event.
// If the caller locks queue mutex then it must pass 'true' to QueueLocked.
ur_result_t CleanupCompletedEvent(ur_event_handle_t Event, bool QueueLocked = false);

enum EventsScope {
  // All events are created host-visible.
  AllHostVisible,
  // All events are created with device-scope and only when
  // host waits them or queries their status that a proxy
  // host-visible event is created and set to signal after
  // original event signals.
  OnDemandHostVisibleProxy,
  // All events are created with device-scope and only
  // when a batch of commands is submitted for execution a
  // last command in that batch is added to signal host-visible
  // completion of each command in this batch (the default mode).
  LastCommandInBatchHostVisible
};

// Get value of device scope events env var setting or default setting
static const int DeviceEventsSetting = [] {
  const char *DeviceEventsSettingStr =
      std::getenv("SYCL_PI_LEVEL_ZERO_DEVICE_SCOPE_EVENTS");
  if (DeviceEventsSettingStr) {
    // Override the default if user has explicitly chosen the events scope.
    switch (std::stoi(DeviceEventsSettingStr)) {
    case 0:
      return AllHostVisible;
    case 1:
      return OnDemandHostVisibleProxy;
    case 2:
      return LastCommandInBatchHostVisible;
    default:
      // fallthrough to default setting
      break;
    }
  }
  // This is our default setting, which is expected to be the fastest
  // with the modern GPU drivers.
  return AllHostVisible;
}();
