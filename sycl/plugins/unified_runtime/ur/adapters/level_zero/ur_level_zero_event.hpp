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

#include <sycl/detail/pi.h>
#include <ur/ur.hpp>
#include <ur_api.h>
#include <ze_api.h>
#include <zes_api.h>

#include "ur_level_zero_common.hpp"

struct _ur_event_handle_t : _pi_object {
  _ur_event_handle_t(ze_event_handle_t ZeEvent, ze_event_pool_handle_t ZeEventPool, pi_context Context, pi_command_type CommandType, bool OwnZeEvent): 
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
  pi_context Context;

  // Keeps the command-queue and command associated with the event.
  // These are NULL for the user events.
  pi_queue Queue = {nullptr};
  pi_command_type CommandType;

  // Opaque data to hold any data needed for CommandType.
  void *CommandData;

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

};
