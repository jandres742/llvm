//===--------- pi_level_zero.hpp - Level Zero Plugin ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

/// \defgroup sycl_pi_level_zero Level Zero Plugin
/// \ingroup sycl_pi

/// \file pi_level_zero.hpp
/// Declarations for Level Zero Plugin. It is the interface between the
/// device-agnostic SYCL runtime layer and underlying Level Zero runtime.
///
/// \ingroup sycl_pi_level_zero

#ifndef PI_LEVEL_ZERO_HPP
#define PI_LEVEL_ZERO_HPP

// This version should be incremented for any change made to this file or its
// corresponding .cpp file.
#define _PI_LEVEL_ZERO_PLUGIN_VERSION 1

#define _PI_LEVEL_ZERO_PLUGIN_VERSION_STRING                                   \
  _PI_PLUGIN_VERSION_STRING(_PI_LEVEL_ZERO_PLUGIN_VERSION)

#include <atomic>
#include <cassert>
#include <cstring>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <sycl/detail/pi.h>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <sycl/detail/iostream_proxy.hpp>
#include <ze_api.h>
#include <zes_api.h>

// Share code between this PI L0 Plugin and UR L0 Adapter
#include <pi2ur.hpp>
#include <ur/adapters/level_zero/ur_level_zero.hpp>
#include <ur/usm_allocator.hpp>
#include "ur/usm_allocator_config.hpp"

template <class To, class From> To pi_cast(From Value) {
  // TODO: see if more sanity checks are possible.
  assert(sizeof(From) == sizeof(To));
  return (To)(Value);
}

template <> uint32_t inline pi_cast(uint64_t Value) {
  // Cast value and check that we don't lose any information.
  uint32_t CastedValue = (uint32_t)(Value);
  assert((uint64_t)CastedValue == Value);
  return CastedValue;
}

// Define the types that are opaque in pi.h in a manner suitabale for Level Zero
// plugin

struct _pi_platform : public _ur_platform_handle_t {
  using _ur_platform_handle_t::_ur_platform_handle_t;

  // Keep track of all contexts in the platform. This is needed to manage
  // a lifetime of memory allocations in each context when there are kernels
  // with indirect access.
  // TODO: should be deleted when memory isolation in the context is implemented
  // in the driver.
  std::list<pi_context> Contexts;
  pi_shared_mutex ContextsMutex;
};

extern usm_settings::USMAllocatorConfig USMAllocatorConfigInstance;

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

struct _pi_device : _ur_device_handle_t {
  using _ur_device_handle_t::_ur_device_handle_t;
};

struct _pi_context : _ur_context_handle_t {
  _pi_context(ze_context_handle_t ZeContext, pi_uint32 NumDevices,
              const pi_device *Devs, bool OwnZeContext)
      : _ur_context_handle_t(ZeContext,
                             reinterpret_cast<uint32_t>(NumDevices),
                             reinterpret_cast<ur_device_handle_t *>(const_cast<pi_device *>(Devs)),
                             OwnZeContext){
    SingleRootDevice = getRootDevice();
    // NOTE: one must additionally call initialize() to complete
    // PI context creation.
  }

  // Finalize the PI context
  pi_result finalize();

  // Return the Platform, which is the same for all devices in the context
  pi_platform getPlatform() const;

  // Checks if Device is covered by this context.
  // For that the Device or its root devices need to be in the context.
  bool isValidDevice(pi_device Device) const;

  // Retrieves a command list for executing on this device along with
  // a fence to be used in tracking the execution of this command list.
  // If a command list has been created on this device which has
  // completed its commands, then that command list and its associated fence
  // will be reused. Otherwise, a new command list and fence will be created for
  // running on this device. L0 fences are created on a L0 command queue so the
  // caller must pass a command queue to create a new fence for the new command
  // list if a command list/fence pair is not available. All Command Lists &
  // associated fences are destroyed at Device Release.
  // If UseCopyEngine is true, the command will eventually be executed in a
  // copy engine. Otherwise, the command will be executed in a compute engine.
  // If AllowBatching is true, then the command list returned may already have
  // command in it, if AllowBatching is false, any open command lists that
  // already exist in Queue will be closed and executed.
  // If ForcedCmdQueue is not nullptr, the resulting command list must be tied
  // to the contained command queue. This option is ignored if immediate
  // command lists are used.
  // When using immediate commandlists, retrieves an immediate command list
  // for executing on this device. Immediate commandlists are created only
  // once for each SYCL Queue and after that they are reused.
  pi_result
  getAvailableCommandList(pi_queue Queue, pi_command_list_ptr_t &CommandList,
                          bool UseCopyEngine, bool AllowBatching = false,
                          ze_command_queue_handle_t *ForcedCmdQueue = nullptr);

  // Get index of the free slot in the available pool. If there is no available
  // pool then create new one. The HostVisible parameter tells if we need a
  // slot for a host-visible event. The ProfilingEnabled tells is we need a
  // slot for an event with profiling capabilities.
  pi_result getFreeSlotInExistingOrNewPool(ze_event_pool_handle_t &, size_t &,
                                           bool HostVisible,
                                           bool ProfilingEnabled);

  // Decrement number of events living in the pool upon event destroy
  // and return the pool to the cache if there are no unreleased events.
  pi_result decrementUnreleasedEventsInPool(pi_event Event);

  // Get pi_event from cache.
  pi_event getEventFromContextCache(bool HostVisible, bool WithProfiling);

  // Add pi_event to cache.
  void addEventToContextCache(pi_event);

private:

  auto getZeEventPoolCache(bool HostVisible, bool WithProfiling) {
    if (HostVisible)
      return WithProfiling ? &ZeEventPoolCache[0] : &ZeEventPoolCache[1];
    else
      return WithProfiling ? &ZeEventPoolCache[2] : &ZeEventPoolCache[3];
  }

  // Get the cache of events for a provided scope and profiling mode.
  auto getEventCache(bool HostVisible, bool WithProfiling) {
    if (HostVisible)
      return WithProfiling ? &EventCaches[0] : &EventCaches[1];
    else
      return WithProfiling ? &EventCaches[2] : &EventCaches[3];
  }
};

struct _pi_queue : _ur_queue_handle_t {
  // ForceComputeIndex, if non-negative, indicates that the queue must be fixed
  // to that particular compute CCS.
  _pi_queue(std::vector<ze_command_queue_handle_t> &ComputeQueues,
            std::vector<ze_command_queue_handle_t> &CopyQueues,
            pi_context Context, pi_device Device, bool OwnZeCommandQueue,
            pi_queue_properties Properties = 0, int ForceComputeIndex = -1);

  // Wait for all commandlists associated with this Queue to finish operations.
  pi_result synchronize();

  // Return the queue group to use based on standard/immediate commandlist mode,
  // and if immediate mode, the thread-specific group.
  pi_queue_group_t &getQueueGroup(bool UseCopyEngine);

  // This function considers multiple factors including copy engine
  // availability and user preference and returns a boolean that is used to
  // specify if copy engine will eventually be used for a particular command.
  bool useCopyEngine(bool PreferCopyEngine = true) const;

  // Kernel is not necessarily submitted for execution during
  // piEnqueueKernelLaunch, it may be batched. That's why we need to save the
  // list of kernels which is going to be submitted but have not been submitted
  // yet. This is needed to capture memory allocations for each kernel with
  // indirect access in the list at the moment when kernel is really submitted
  // for execution.
  std::vector<pi_kernel> KernelsToBeSubmitted;

  // Update map of memory references made by the kernels about to be submitted
  void CaptureIndirectAccesses();

  // ComputeCommandBatch holds data related to batching of non-copy commands.
  // CopyCommandBatch holds data related to batching of copy commands.
  command_batch ComputeCommandBatch, CopyCommandBatch;

  // Returns true if any commands for this queue are allowed to
  // be batched together.
  // For copy commands, IsCopy is set to 'true'.
  // For non-copy commands, IsCopy is set to 'false'.
  bool isBatchingAllowed(bool IsCopy) const;

  // Returns true if the queue is a in-order queue.
  bool isInOrderQueue() const;

  // Returns true if the queue has discard events property.
  bool isDiscardEvents() const;

  // Returns true if the queue has explicit priority set by user.
  bool isPriorityLow() const;
  bool isPriorityHigh() const;

  // adjust the queue's batch size, knowing that the current command list
  // is being closed with a full batch.
  // For copy commands, IsCopy is set to 'true'.
  // For non-copy commands, IsCopy is set to 'false'.
  void adjustBatchSizeForFullBatch(bool IsCopy);

  // adjust the queue's batch size, knowing that the current command list
  // is being closed with only a partial batch of commands.
  // For copy commands, IsCopy is set to 'true'.
  // For non-copy commands, IsCopy is set to 'false'.
  void adjustBatchSizeForPartialBatch(bool IsCopy);

  // Helper function to create a new command-list to this queue and associated
  // fence tracking its completion. This command list & fence are added to the
  // map of command lists in this queue with ZeFenceInUse = false.
  // The caller must hold a lock of the queue already.
  pi_result
  createCommandList(bool UseCopyEngine, pi_command_list_ptr_t &CommandList,
                    ze_command_queue_handle_t *ForcedCmdQueue = nullptr);

  /// @brief Resets the command list and associated fence in the map and removes
  /// events from the command list.
  /// @param CommandList The caller must verify that this command list and fence
  /// have been signalled.
  /// @param MakeAvailable If the reset command list should be made available,
  /// then MakeAvailable needs to be set to true.
  /// @param EventListToCleanup  The EventListToCleanup contains a list of
  /// events from the command list which need to be cleaned up.
  /// @param CheckStatus Hint informing whether we need to check status of the
  /// events before removing them from the immediate command list. This is
  /// needed because immediate command lists are not associated with fences and
  /// in general status of the event needs to be checked.
  /// @return PI_SUCCESS if successful, PI error code otherwise.
  pi_result resetCommandList(pi_command_list_ptr_t CommandList,
                             bool MakeAvailable,
                             std::vector<pi_event> &EventListToCleanup,
                             bool CheckStatus = true);

  // Returns true if an OpenCommandList has commands that need to be submitted.
  // If IsCopy is 'true', then the OpenCommandList containing copy commands is
  // checked. Otherwise, the OpenCommandList containing compute commands is
  // checked.
  bool hasOpenCommandList(bool IsCopy) const {
    auto CommandBatch = (IsCopy) ? CopyCommandBatch : ComputeCommandBatch;
    return CommandBatch.OpenCommandList != CommandListMap.end();
  }
  // Attach a command list to this queue.
  // For non-immediate commandlist also close and execute it.
  // Note that this command list cannot be appended to after this.
  // The "IsBlocking" tells if the wait for completion is required.
  // If OKToBatchCommand is true, then this command list may be executed
  // immediately, or it may be left open for other future command to be
  // batched into.
  // If IsBlocking is true, then batching will not be allowed regardless
  // of the value of OKToBatchCommand
  //
  // For immediate commandlists, no close and execute is necessary.
  pi_result executeCommandList(pi_command_list_ptr_t CommandList,
                               bool IsBlocking = false,
                               bool OKToBatchCommand = false);

  // If there is an open command list associated with this queue,
  // close it, execute it, and reset the corresponding OpenCommandList.
  // If IsCopy is 'true', then the OpenCommandList containing copy commands is
  // executed. Otherwise OpenCommandList containing compute commands is
  // executed.
  pi_result executeOpenCommandList(bool IsCopy);

  // Gets the open command containing the event, or CommandListMap.end()
  pi_command_list_ptr_t eventOpenCommandList(pi_event Event);

  // Wrapper function to execute both OpenCommandLists (Copy and Compute).
  // This wrapper is helpful when all 'open' commands need to be executed.
  // Call-sites instances: piQuueueFinish, piQueueRelease, etc.
  pi_result executeAllOpenCommandLists() {
    using IsCopy = bool;
    if (auto Res = executeOpenCommandList(IsCopy{false}))
      return Res;
    if (auto Res = executeOpenCommandList(IsCopy{true}))
      return Res;
    return PI_SUCCESS;
  }

  // Inserts a barrier waiting for all unfinished events in ActiveBarriers into
  // CmdList. Any finished events will be removed from ActiveBarriers.
  pi_result insertActiveBarriers(pi_command_list_ptr_t &CmdList,
                                 bool UseCopyEngine);

  // Get event from the queue's cache.
  // Returns nullptr if the cache doesn't contain any reusable events or if the
  // cache contains only one event which corresponds to the previous command and
  // can't be used for the current command because we can't use the same event
  // two times in a row and have to do round-robin between two events. Otherwise
  // it picks an event from the beginning of the cache and returns it. Event
  // from the last command is always appended to the end of the list.
  pi_event getEventFromQueueCache(bool HostVisible);

  // Put pi_event to the cache. Provided pi_event object is not used by
  // any command but its ZeEvent is used by many pi_event objects.
  // Commands to wait and reset ZeEvent must be submitted to the queue before
  // calling this method.
  pi_result addEventToQueueCache(pi_event Event);

  // Append command to provided command list to wait and reset the last event if
  // it is discarded and create new pi_event wrapper using the same native event
  // and put it to the cache. We call this method after each command submission
  // to make native event available to use by next commands.
  pi_result resetDiscardedEvent(pi_command_list_ptr_t);

  // Append command to the command list to signal new event if the last event in
  // the command list is discarded. While we submit commands in scope of the
  // same command list we can reset and reuse events but when we switch to a
  // different command list we currently need to signal new event and wait for
  // it in the new command list using barrier.
  pi_result signalEventFromCmdListIfLastEventDiscarded(pi_command_list_ptr_t);

  // Insert a barrier waiting for the last command event into the beginning of
  // command list. This barrier guarantees that command list execution starts
  // only after completion of previous command list which signals aforementioned
  // event. It allows to reset and reuse same event handles inside all command
  // lists in the queue.
  pi_result
  insertStartBarrierIfDiscardEventsMode(pi_command_list_ptr_t &CmdList);

  // Helper method telling whether we need to reuse discarded event in this
  // queue.
  bool doReuseDiscardedEvents();
};

struct _pi_mem : _pi_object {
  // Keeps the PI context of this memory handle.
  pi_context Context;

  // Enumerates all possible types of accesses.
  enum access_mode_t { unknown, read_write, read_only, write_only };

  // Interface of the _pi_mem object

  // Get the Level Zero handle of the current memory object
  virtual pi_result getZeHandle(char *&ZeHandle, access_mode_t,
                                pi_device Device = nullptr) = 0;

  // Get a pointer to the Level Zero handle of the current memory object
  virtual pi_result getZeHandlePtr(char **&ZeHandlePtr, access_mode_t,
                                   pi_device Device = nullptr) = 0;

  // Method to get type of the derived object (image or buffer)
  virtual bool isImage() const = 0;

  virtual ~_pi_mem() = default;

protected:
  _pi_mem(pi_context Ctx) : Context{Ctx} {}
};

struct _pi_buffer;
using pi_buffer = _pi_buffer *;

struct _pi_buffer final : _pi_mem {
  // Buffer constructor
  _pi_buffer(pi_context Context, size_t Size, char *HostPtr,
             bool ImportedHostPtr);

  // Sub-buffer constructor
  _pi_buffer(pi_buffer Parent, size_t Origin, size_t Size)
      : _pi_mem(Parent->Context), Size(Size), SubBuffer{Parent, Origin} {}

  // Interop-buffer constructor
  _pi_buffer(pi_context Context, size_t Size, pi_device Device,
             char *ZeMemHandle, bool OwnZeMemHandle);

  // Returns a pointer to the USM allocation representing this PI buffer
  // on the specified Device. If Device is nullptr then the returned
  // USM allocation is on the device where this buffer was used the latest.
  // The returned allocation is always valid, i.e. its contents is
  // up-to-date and any data copies needed for that are performed under
  // the hood.
  //
  virtual pi_result getZeHandle(char *&ZeHandle, access_mode_t,
                                pi_device Device = nullptr) override;
  virtual pi_result getZeHandlePtr(char **&ZeHandlePtr, access_mode_t,
                                   pi_device Device = nullptr) override;

  bool isImage() const override { return false; }

  bool isSubBuffer() const { return SubBuffer.Parent != nullptr; }

  // Frees all allocations made for the buffer.
  pi_result free();

  // Information about a single allocation representing this buffer.
  struct allocation_t {
    // Level Zero memory handle is really just a naked pointer.
    // It is just convenient to have it char * to simplify offset arithmetics.
    char *ZeHandle{nullptr};
    // Indicates if this allocation's data is valid.
    bool Valid{false};
    // Specifies the action that needs to be taken for this
    // allocation at buffer destruction.
    enum {
      keep,       // do nothing, the allocation is not owned by us
      unimport,   // release of the imported allocation
      free,       // free from the pooling context (default)
      free_native // free with a native call
    } ReleaseAction{free};
  };

  // We maintain multiple allocations on possibly all devices in the context.
  // The "nullptr" device identifies a host allocation representing buffer.
  // Sub-buffers don't maintain own allocations but rely on parent buffer.
  std::unordered_map<pi_device, allocation_t> Allocations;
  pi_device LastDeviceWithValidAllocation{nullptr};

  // Flag to indicate that this memory is allocated in host memory.
  // Integrated device accesses this memory.
  bool OnHost{false};

  // Tells the host allocation to use for buffer map operations.
  char *MapHostPtr{nullptr};

  // Supplementary data to keep track of the mappings of this buffer
  // created with piEnqueueMemBufferMap.
  struct Mapping {
    // The offset in the buffer giving the start of the mapped region.
    size_t Offset;
    // The size of the mapped region.
    size_t Size;
  };

  // The key is the host pointer representing an active mapping.
  // The value is the information needed to maintain/undo the mapping.
  std::unordered_map<void *, Mapping> Mappings;

  // The size and alignment of the buffer
  size_t Size;
  size_t getAlignment() const;

  struct {
    _pi_mem *Parent;
    size_t Origin; // only valid if Parent != nullptr
  } SubBuffer;
};

// TODO: add proper support for images on context with multiple devices.
struct _pi_image final : _pi_mem {
  // Image constructor
  _pi_image(pi_context Ctx, ze_image_handle_t Image)
      : _pi_mem(Ctx), ZeImage{Image} {}

  virtual pi_result getZeHandle(char *&ZeHandle, access_mode_t,
                                pi_device = nullptr) override {
    ZeHandle = pi_cast<char *>(ZeImage);
    return PI_SUCCESS;
  }
  virtual pi_result getZeHandlePtr(char **&ZeHandlePtr, access_mode_t,
                                   pi_device = nullptr) override {
    ZeHandlePtr = pi_cast<char **>(&ZeImage);
    return PI_SUCCESS;
  }

  bool isImage() const override { return true; }

#ifndef NDEBUG
  // Keep the descriptor of the image (for debugging purposes)
  ZeStruct<ze_image_desc_t> ZeImageDesc;
#endif // !NDEBUG

  // Level Zero image handle.
  ze_image_handle_t ZeImage;
};

struct _pi_ze_event_list_t {
  // List of level zero events for this event list.
  ze_event_handle_t *ZeEventList = {nullptr};

  // List of pi_events for this event list.
  pi_event *PiEventList = {nullptr};

  // length of both the lists.  The actual allocation of these lists
  // may be longer than this length.  This length is the actual number
  // of elements in the above arrays that are valid.
  pi_uint32 Length = {0};

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
  pi_result createAndRetainPiZeEventList(pi_uint32 EventListLength,
                                         const pi_event *EventList,
                                         pi_queue CurQueue, bool UseCopyEngine);

  // Add all the events in this object's PiEventList to the end
  // of the list EventsToBeReleased. Destroy pi_ze_event_list_t data
  // structure fields making it look empty.
  pi_result collectEventsForReleaseAndDestroyPiZeEventList(
      std::list<pi_event> &EventsToBeReleased);

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

struct _pi_event : _ur_event_handle_t {
  _pi_event(ze_event_handle_t ZeEvent, ze_event_pool_handle_t ZeEventPool,
            pi_context Context, pi_command_type CommandType, bool OwnZeEvent)
      : _ur_event_handle_t(ZeEvent, ZeEventPool, Context, CommandType, OwnZeEvent) {}

  // Get the host-visible event or create one and enqueue its signal.
  pi_result getOrCreateHostVisibleEvent(ze_event_handle_t &HostVisibleEvent);

  // Tells if this event is with profiling capabilities.
  bool isProfilingEnabled() const {
    return !Queue || // tentatively assume user events are profiling enabled
           (Queue->Properties & PI_QUEUE_FLAG_PROFILING_ENABLE) != 0;
  }

  // List of events that were in the wait list of the command that will
  // signal this event.  These events must be retained when the command is
  // enqueued, and must then be released when this event has signalled.
  // This list must be destroyed once the event has signalled.
  _pi_ze_event_list_t WaitList;

  // Command list associated with the pi_event.
  std::optional<pi_command_list_ptr_t> CommandList;

  // Reset _pi_event object.
  pi_result reset();
};

struct _pi_program : _pi_object {
  // Possible states of a program.
  typedef enum {
    // The program has been created from intermediate language (SPIR-V), but it
    // is not yet compiled.
    IL,

    // The program has been created by loading native code, but it has not yet
    // been built.  This is equivalent to an OpenCL "program executable" that
    // is loaded via clCreateProgramWithBinary().
    Native,

    // The program was notionally compiled from SPIR-V form.  However, since we
    // postpone compilation until the module is linked, the internal state
    // still represents the module as SPIR-V.
    Object,

    // The program has been built or linked, and it is represented as a Level
    // Zero module.
    Exe,

    // An error occurred during piProgramLink, but we created a _pi_program
    // object anyways in order to hold the ZeBuildLog.  Note that the ZeModule
    // may or may not be nullptr in this state, depending on the error.
    Invalid
  } state;

  // A utility class that converts specialization constants into the form
  // required by the Level Zero driver.
  class SpecConstantShim {
  public:
    SpecConstantShim(pi_program Program) {
      ZeSpecConstants.numConstants = Program->SpecConstants.size();
      ZeSpecContantsIds.reserve(ZeSpecConstants.numConstants);
      ZeSpecContantsValues.reserve(ZeSpecConstants.numConstants);

      for (auto &SpecConstant : Program->SpecConstants) {
        ZeSpecContantsIds.push_back(SpecConstant.first);
        ZeSpecContantsValues.push_back(SpecConstant.second);
      }
      ZeSpecConstants.pConstantIds = ZeSpecContantsIds.data();
      ZeSpecConstants.pConstantValues = ZeSpecContantsValues.data();
    }

    const ze_module_constants_t *ze() { return &ZeSpecConstants; }

  private:
    std::vector<uint32_t> ZeSpecContantsIds;
    std::vector<const void *> ZeSpecContantsValues;
    ze_module_constants_t ZeSpecConstants;
  };

  // Construct a program in IL or Native state.
  _pi_program(state St, pi_context Context, const void *Input, size_t Length)
      : Context{Context}, OwnZeModule{true}, State{St},
        Code{new uint8_t[Length]}, CodeLength{Length}, ZeModule{nullptr},
        ZeBuildLog{nullptr} {
    std::memcpy(Code.get(), Input, Length);
  }

  // Construct a program in Exe or Invalid state.
  _pi_program(state St, pi_context Context, ze_module_handle_t ZeModule,
              ze_module_build_log_handle_t ZeBuildLog)
      : Context{Context}, OwnZeModule{true}, State{St}, ZeModule{ZeModule},
        ZeBuildLog{ZeBuildLog} {}

  // Construct a program in Exe state (interop).
  _pi_program(state St, pi_context Context, ze_module_handle_t ZeModule,
              bool OwnZeModule)
      : Context{Context}, OwnZeModule{OwnZeModule}, State{St},
        ZeModule{ZeModule}, ZeBuildLog{nullptr} {}

  // Construct a program in Invalid state with a custom error message.
  _pi_program(state St, pi_context Context, const std::string &ErrorMessage)
      : Context{Context}, OwnZeModule{true}, ErrorMessage{ErrorMessage},
        State{St}, ZeModule{nullptr}, ZeBuildLog{nullptr} {}

  ~_pi_program();

  const pi_context Context; // Context of the program.

  // Indicates if we own the ZeModule or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  const bool OwnZeModule;

  // This error message is used only in Invalid state to hold a custom error
  // message from a call to piProgramLink.
  const std::string ErrorMessage;

  state State;

  // In IL and Object states, this contains the SPIR-V representation of the
  // module.  In Native state, it contains the native code.
  std::unique_ptr<uint8_t[]> Code; // Array containing raw IL / native code.
  size_t CodeLength{0};            // Size (bytes) of the array.

  // Used only in IL and Object states.  Contains the SPIR-V specialization
  // constants as a map from the SPIR-V "SpecID" to a buffer that contains the
  // associated value.  The caller of the PI layer is responsible for
  // maintaining the storage of this buffer.
  std::unordered_map<uint32_t, const void *> SpecConstants;

  // Used only in Object state.  Contains the build flags from the last call to
  // piProgramCompile().
  std::string BuildFlags;

  // The Level Zero module handle.  Used primarily in Exe state.
  ze_module_handle_t ZeModule;

  // The Level Zero build log from the last call to zeModuleCreate().
  ze_module_build_log_handle_t ZeBuildLog;
};

struct _pi_kernel : _pi_object {
  _pi_kernel(ze_kernel_handle_t Kernel, bool OwnZeKernel, pi_program Program)
      : ZeKernel{Kernel}, OwnZeKernel{OwnZeKernel}, Program{Program},
        MemAllocs{}, SubmissionsCount{0} {}

  // Completed initialization of PI kernel. Must be called after construction.
  pi_result initialize();

  // Returns true if kernel has indirect access, false otherwise.
  bool hasIndirectAccess() {
    // Currently indirect access flag is set for all kernels and there is no API
    // to check if kernel actually indirectly access smth.
    return true;
  }

  // Level Zero function handle.
  ze_kernel_handle_t ZeKernel;

  // Indicates if we own the ZeKernel or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  bool OwnZeKernel;

  // Keep the program of the kernel.
  pi_program Program;

  // Hash function object for the unordered_set below.
  struct Hash {
    size_t operator()(const std::pair<void *const, MemAllocRecord> *P) const {
      return std::hash<void *>()(P->first);
    }
  };

  // If kernel has indirect access we need to make a snapshot of all existing
  // memory allocations to defer deletion of these memory allocations to the
  // moment when kernel execution has finished.
  // We store pointers to the elements because pointers are not invalidated by
  // insert/delete for std::unordered_map (iterators are invalidated). We need
  // to take a snapshot instead of just reference-counting the allocations,
  // because picture of active allocations can change during kernel execution
  // (new allocations can be added) and we need to know which memory allocations
  // were retained by this kernel to release them (and don't touch new
  // allocations) at kernel completion. Same kernel may be submitted several
  // times and retained allocations may be different at each submission. That's
  // why we have a set of memory allocations here and increase ref count only
  // once even if kernel is submitted many times. We don't want to know how many
  // times and which allocations were retained by each submission. We release
  // all allocations in the set only when SubmissionsCount == 0.
  std::unordered_set<std::pair<void *const, MemAllocRecord> *, Hash> MemAllocs;

  // Counter to track the number of submissions of the kernel.
  // When this value is zero, it means that kernel is not submitted for an
  // execution - at this time we can release memory allocations referenced by
  // this kernel. We can do this when RefCount turns to 0 but it is too late
  // because kernels are cached in the context by SYCL RT and they are released
  // only during context object destruction. Regular RefCount is not usable to
  // track submissions because user/SYCL RT can retain kernel object any number
  // of times. And that's why there is no value of RefCount which can mean zero
  // submissions.
  std::atomic<pi_uint32> SubmissionsCount;

  // Keeps info about an argument to the kernel enough to set it with
  // zeKernelSetArgumentValue.
  struct ArgumentInfo {
    uint32_t Index;
    size_t Size;
    const pi_mem Value;
    _pi_mem::access_mode_t AccessMode{_pi_mem::unknown};
  };
  // Arguments that still need to be set (with zeKernelSetArgumentValue)
  // before kernel is enqueued.
  std::vector<ArgumentInfo> PendingArguments;

  // Cache of the kernel properties.
  ZeCache<ZeStruct<ze_kernel_properties_t>> ZeKernelProperties;
  ZeCache<std::string> ZeKernelName;
};

struct _pi_sampler : _pi_object {
  _pi_sampler(ze_sampler_handle_t Sampler) : ZeSampler{Sampler} {}

  // Level Zero sampler handle.
  ze_sampler_handle_t ZeSampler;
};

#endif // PI_LEVEL_ZERO_HPP
