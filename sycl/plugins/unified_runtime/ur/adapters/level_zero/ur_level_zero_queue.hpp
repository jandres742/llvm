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

// Structure describing the specific use of a command-list in a queue.
// This is because command-lists are re-used across multiple queues
// in the same context.
struct pi_command_list_info_t {
  // The Level-Zero fence that will be signalled at completion.
  // Immediate commandlists do not have an associated fence.
  // A nullptr for the fence indicates that this is an immediate commandlist.
  ze_fence_handle_t ZeFence{nullptr};
  // Record if the fence is in use.
  // This is needed to avoid leak of the tracked command-list if the fence
  // was not yet signaled at the time all events in that list were already
  // completed (we are polling the fence at events completion). The fence
  // may be still "in-use" due to sporadic delay in HW.
  bool ZeFenceInUse{false};

  // Record the queue to which the command list will be submitted.
  ze_command_queue_handle_t ZeQueue{nullptr};
  // Keeps the ordinal of the ZeQueue queue group. Invalid if ZeQueue==nullptr
  uint32_t ZeQueueGroupOrdinal{0};
  // Helper functions to tell if this is a copy command-list.
  bool isCopy(pi_queue Queue) const;

  // Keeps events created by commands submitted into this command-list.
  // TODO: use this for explicit wait/cleanup of events at command-list
  // completion.
  // TODO: use this for optimizing events in the same command-list, e.g.
  // only have last one visible to the host.
  std::vector<pi_event> EventList{};
  size_t size() const { return EventList.size(); }
  void append(pi_event Event) { EventList.push_back(Event); }
};

// The map type that would track all command-lists in a queue.
using pi_command_list_map_t =
    std::unordered_map<ze_command_list_handle_t, pi_command_list_info_t>;
// The iterator pointing to a specific command-list in use.
using pi_command_list_ptr_t = pi_command_list_map_t::iterator;


struct _ur_queue_handle_t : _pi_object {
  _ur_queue_handle_t(std::vector<ze_command_queue_handle_t> &ComputeQueues,
            std::vector<ze_command_queue_handle_t> &CopyQueues,
            pi_context Context, pi_device Device, bool OwnZeCommandQueue,
            pi_queue_properties Properties = 0, int ForceComputeIndex = -1);

  using queue_type = _ur_device_handle_t::queue_group_info_t::type;
  // PI queue is in general a one to many mapping to L0 native queues.
  struct pi_queue_group_t {
    pi_queue Queue;
    pi_queue_group_t() = delete;

    // The Queue argument captures the enclosing PI queue.
    // The Type argument specifies the type of this queue group.
    // The actual ZeQueues are populated at PI queue construction.
    pi_queue_group_t(pi_queue Queue, queue_type Type)
        : Queue(Queue), Type(Type) {}

    // The type of the queue group.
    queue_type Type;
    bool isCopy() const { return Type != queue_type::Compute; }

    // Level Zero command queue handles.
    std::vector<ze_command_queue_handle_t> ZeQueues;

    // Immediate commandlist handles, one per Level Zero command queue handle.
    // These are created only once, along with the L0 queues (see above)
    // and reused thereafter.
    std::vector<pi_command_list_ptr_t> ImmCmdLists;

    // Return the index of the next queue to use based on a
    // round robin strategy and the queue group ordinal.
    // If QueryOnly is true then return index values but don't update internal
    // index data members of the queue.
    uint32_t getQueueIndex(uint32_t *QueueGroupOrdinal, uint32_t *QueueIndex,
                           bool QueryOnly = false);

    // Get the ordinal for a command queue handle.
    int32_t getCmdQueueOrdinal(ze_command_queue_handle_t CmdQueue);

    // This function will return one of possibly multiple available native
    // queues and the value of the queue group ordinal.
    ze_command_queue_handle_t &getZeQueue(uint32_t *QueueGroupOrdinal);

    // This function returns the next immediate commandlist to use.
    pi_command_list_ptr_t &getImmCmdList();

    // These indices are to filter specific range of the queues to use,
    // and to organize round-robin across them.
    uint32_t UpperIndex{0};
    uint32_t LowerIndex{0};
    uint32_t NextIndex{0};
  };

  // A map of compute groups containing compute queue handles, one per thread.
  // When a queue is accessed from multiple host threads, a separate queue group
  // is created for each thread. The key used for mapping is the thread ID.
  std::unordered_map<std::thread::id, pi_queue_group_t> ComputeQueueGroupsByTID;

  // A group containing copy queue handles. The main copy engine, if available,
  // comes first followed by link copy engines, if available.
  // When a queue is accessed from multiple host threads, a separate queue group
  // is created for each thread. The key used for mapping is the thread ID.
  std::unordered_map<std::thread::id, pi_queue_group_t> CopyQueueGroupsByTID;

  // Keeps the PI context to which this queue belongs.
  // This field is only set at _pi_queue creation time, and cannot change.
  // Therefore it can be accessed without holding a lock on this _pi_queue.
  const pi_context Context;

  // Keeps the PI device to which this queue belongs.
  // This field is only set at _pi_queue creation time, and cannot change.
  // Therefore it can be accessed without holding a lock on this _pi_queue.
  const pi_device Device;

  // Keeps track of the event associated with the last enqueued command into
  // this queue. this is used to add dependency with the last command to add
  // in-order semantics and updated with the latest event each time a new
  // command is enqueued.
  pi_event LastCommandEvent = nullptr;

  // Indicates if we own the ZeCommandQueue or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  bool OwnZeCommandQueue;

  // Keeps the properties of this queue.
  pi_queue_properties Properties;

    // Map of all command lists used in this queue.
  pi_command_list_map_t CommandListMap;

  // Helper data structure to hold all variables related to batching
  struct command_batch {
    // These two members are used to keep track of how often the
    // batching closes and executes a command list before reaching the
    // QueueComputeBatchSize limit, versus how often we reach the limit.
    // This info might be used to vary the QueueComputeBatchSize value.
    uint32_t NumTimesClosedEarly = {0};
    uint32_t NumTimesClosedFull = {0};

    // Open command list fields for batching commands into this queue.
    pi_command_list_ptr_t OpenCommandList{};

    // Approximate number of commands that are allowed to be batched for
    // this queue.
    // Added this member to the queue rather than using a global variable
    // so that future implementation could use heuristics to change this on
    // a queue specific basis. And by putting it in the queue itself, this
    // is thread safe because of the locking of the queue that occurs.
    uint32_t QueueBatchSize = {0};
  };

  // A helper structure to keep active barriers of the queue.
  // It additionally manages ref-count of events in this list.
  struct active_barriers {
    std::vector<pi_event> Events;
    void add(pi_event &Event);
    pi_result clear();
    bool empty() { return Events.empty(); }
    std::vector<pi_event> &vector() { return Events; }
  };
  // A collection of currently active barriers.
  // These should be inserted into a command list whenever an available command
  // list is needed for a command.
  active_barriers ActiveBarriers;

  // Besides each PI object keeping a total reference count in
  // _pi_object::RefCount we keep special track of the queue *external*
  // references. This way we are able to tell when the queue is being finished
  // externally, and can wait for internal references to complete, and do proper
  // cleanup of the queue.
  // This counter doesn't track the lifetime of a queue object, it only tracks
  // the number of external references. I.e. even if it reaches zero a queue
  // object may not be destroyed and can be used internally in the plugin.
  // That's why we intentionally don't use atomic type for this counter to
  // enforce guarding with a mutex all the work involving this counter.
  uint32_t RefCountExternal{1};

  // Indicates that the queue is healthy and all operations on it are OK.
  bool Healthy{true};

  // The following data structures and methods are used only for handling
  // in-order queue with discard_events property. Some commands in such queue
  // may have discarded event. Which means that event is not visible outside of
  // the plugin. It is possible to reset and reuse discarded events in the same
  // in-order queue because of the dependency between commands. We don't have to
  // wait event completion to do this. We use the following 2-event model to
  // reuse events inside each command list:
  //
  // Operation1 = zeCommantListAppendMemoryCopy (signal ze_event1)
  // zeCommandListAppendBarrier(wait for ze_event1)
  // zeCommandListAppendEventReset(ze_event1)
  // # Create new pi_event using ze_event1 and append to the cache.
  //
  // Operation2 = zeCommandListAppendMemoryCopy (signal ze_event2)
  // zeCommandListAppendBarrier(wait for ze_event2)
  // zeCommandListAppendEventReset(ze_event2)
  // # Create new pi_event using ze_event2 and append to the cache.
  //
  // # Get pi_event from the beginning of the cache because there are two events
  // # there. So it is guaranteed that we do round-robin between two events -
  // # event from the last command is appended to the cache.
  // Operation3 = zeCommandListAppendMemoryCopy (signal ze_event1)
  // # The same ze_event1 is used for Operation1 and Operation3.
  //
  // When we switch to a different command list we need to signal new event and
  // wait for it in the new command list using barrier.
  // [CmdList1]
  // Operation1 = zeCommantListAppendMemoryCopy (signal event1)
  // zeCommandListAppendBarrier(wait for event1)
  // zeCommandListAppendEventReset(event1)
  // zeCommandListAppendSignalEvent(NewEvent)
  //
  // [CmdList2]
  // zeCommandListAppendBarrier(wait for NewEvent)
  //
  // This barrier guarantees that command list execution starts only after
  // completion of previous command list which signals aforementioned event. It
  // allows to reset and reuse same event handles inside all command lists in
  // scope of the queue. It means that we need 2 reusable events of each type
  // (host-visible and device-scope) per queue at maximum.

  // This data member keeps track of the last used command list and allows to
  // handle switch of immediate command lists because immediate command lists
  // are never closed unlike regular command lists.
  pi_command_list_ptr_t LastUsedCommandList = CommandListMap.end();

  // Vector of 2 lists of reusable events: host-visible and device-scope.
  // They are separated to allow faster access to stored events depending on
  // requested type of event. Each list contains events which can be reused
  // inside all command lists in the queue as described in the 2-event model.
  // Leftover events in the cache are relased at the queue destruction.
  std::vector<std::list<pi_event>> EventCaches{2};

};
