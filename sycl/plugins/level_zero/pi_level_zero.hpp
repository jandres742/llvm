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
};

extern usm_settings::USMAllocatorConfig USMAllocatorConfigInstance;

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

  // Checks if Device is covered by this context.
  // For that the Device or its root devices need to be in the context.
  bool isValidDevice(pi_device Device) const;
};

struct _pi_queue : _ur_queue_handle_t {
  // ForceComputeIndex, if non-negative, indicates that the queue must be fixed
  // to that particular compute CCS.
  _pi_queue(std::vector<ze_command_queue_handle_t> &ComputeQueues,
            std::vector<ze_command_queue_handle_t> &CopyQueues,
            pi_context Context, pi_device Device, bool OwnZeCommandQueue,
            pi_queue_properties Properties = 0, int ForceComputeIndex = -1);
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

struct _pi_event : _ur_event_handle_t {
  _pi_event(ze_event_handle_t ZeEvent, ze_event_pool_handle_t ZeEventPool,
            ur_context_handle_t Context, pi_command_type CommandType, bool OwnZeEvent)
      : _ur_event_handle_t(ZeEvent, ZeEventPool, Context, CommandType, OwnZeEvent) {}

  // Get the host-visible event or create one and enqueue its signal.
  pi_result getOrCreateHostVisibleEvent(ze_event_handle_t &HostVisibleEvent);

};

struct _pi_program : _ur_program_handle_t {
  // Construct a program in IL or Native state.
  _pi_program(state St, ur_context_handle_t Context, const void *Input, size_t Length):
    _ur_program_handle_t(St, Context, Input, Length) {}

  // Construct a program in Exe or Invalid state.
  _pi_program(state St, ur_context_handle_t Context, ze_module_handle_t ZeModule,
              ze_module_build_log_handle_t ZeBuildLog):
      _ur_program_handle_t(St, Context, ZeModule, ZeBuildLog) {}

  // Construct a program in Exe state (interop).
  _pi_program(state St, ur_context_handle_t Context, ze_module_handle_t ZeModule,
              bool OwnZeModule):
      _ur_program_handle_t(St, Context, ZeModule, OwnZeModule) {}

  // Construct a program in Invalid state with a custom error message.
  _pi_program(state St, ur_context_handle_t Context, const std::string &ErrorMessage):
      _ur_program_handle_t(St, Context, ErrorMessage) {}

  ~_pi_program() {};
};

struct _pi_kernel : _ur_kernel_handle_t {
  _pi_kernel(ze_kernel_handle_t Kernel, bool OwnZeKernel, ur_program_handle_t Program)
      : _ur_kernel_handle_t(Program), ZeKernel{Kernel}, OwnZeKernel{OwnZeKernel} {}

  // Completed initialization of PI kernel. Must be called after construction.
  pi_result initialize();

  // Level Zero function handle.
  ze_kernel_handle_t ZeKernel;

  // Indicates if we own the ZeKernel or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  bool OwnZeKernel;

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
