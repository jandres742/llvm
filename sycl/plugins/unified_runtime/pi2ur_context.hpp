//===---------------- pi2ur.hpp - PI API to UR API  --------------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//
#pragma once

// #include <unordered_map>

// #include "ur_api.h"
// #include <sycl/detail/pi.h>
// #include <ur/ur.hpp>
// #include "ur/adapters/level_zero/ur_level_zero.hpp"
#include "pi2ur_common.hpp"
// #include "ur/adapters/level_zero/ur_level_zero.hpp"
#include <ze_api.h>


namespace pi2ur {

struct _pi_context : _pi_object {
  _pi_context(ur_context_handle_t UrContext): UrContext{UrContext}  {}

  _pi_context(ur_context_handle_t UrContext,
                       uint32_t NumDevices,
                       const pi_device *Devs,
                       bool OwnZeContext) :
    UrContext{UrContext}, Devices{Devs, Devs + NumDevices}, OwnZeContext{OwnZeContext} {}

  ur_context_handle_t UrContext;

  // Keep the PI devices this PI context was created for.
  // This field is only set at _pi_context creation time, and cannot change.
  // Therefore it can be accessed without holding a lock on this _pi_context.
  //const std::vector<ur_device_handle_t> Devices;
  std::vector<pi_device> Devices;

    // Indicates if we own the ZeContext or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  bool OwnZeContext = false;
};

struct _pi_queue : _pi_object {
  _pi_queue(ur_context_handle_t UrContext, ur_queue_handle_t UrQueue, pi_device Device, bool OwnNativeHandle):
    UrContext{UrContext}, UrQueue{UrQueue}, PiDevice{Device},  OwnNativeHandle{OwnNativeHandle} {}

  ur_context_handle_t UrContext = {};
  ur_queue_handle_t UrQueue = {};

  pi_device PiDevice = {};
  bool OwnNativeHandle = false ;
};

struct _pi_device : _pi_object {
  _pi_device(ur_device_handle_t UrDevice): UrDevice{UrDevice}  {}

  ur_device_handle_t UrDevice;
};

struct _pi_program : _pi_object {
  // _ur_program_handle_t() {}

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
    // object anyways in order to hold the ZeBuildLog.  Note that the UrModule
    // may or may not be nullptr in this state, depending on the error.
    Invalid
  } state;

  // A utility class that converts specialization constants into the form
  // required by the Level Zero driver.
  class SpecConstantShim {
  public:
    SpecConstantShim(_pi_program *Program) {
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
  _pi_program(state St, _pi_context * Context, const void *Input, size_t Length)
      : Context{Context}, OwnZeModule{true}, State{St},
        Code{new uint8_t[Length]}, CodeLength{Length}, UrModule{nullptr},
        ZeBuildLog{nullptr} {
    std::memcpy(Code.get(), Input, Length);
  }

  // Construct a program in Exe or Invalid state.
  _pi_program(state St, _pi_context * Context, ur_module_handle_t UrModule,
              ze_module_build_log_handle_t ZeBuildLog)
      : Context{Context}, OwnZeModule{true}, State{St}, UrModule{UrModule},
        ZeBuildLog{ZeBuildLog} {}

  // Construct a program in Exe state (interop).
  _pi_program(state St, _pi_context * Context, ur_module_handle_t UrModule,
              bool OwnZeModule)
      : Context{Context}, OwnZeModule{OwnZeModule}, State{St},
        UrModule{UrModule}, ZeBuildLog{nullptr} {}

  // Construct a program in Invalid state with a custom error message.
  _pi_program(state St, _pi_context * Context, const std::string &ErrorMessage)
      : Context{Context}, OwnZeModule{true}, ErrorMessage{ErrorMessage},
        State{St}, UrModule{nullptr}, ZeBuildLog{nullptr} {}

  ~_pi_program();

  const _pi_context * Context; // Context of the program.

  // Indicates if we own the UrModule or it came from interop that
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

  std::vector<ur_module_handle_t> UrModules;

  // Used only in IL and Object states.  Contains the SPIR-V specialization
  // constants as a map from the SPIR-V "SpecID" to a buffer that contains the
  // associated value.  The caller of the PI layer is responsible for
  // maintaining the storage of this buffer.
  std::unordered_map<uint32_t, const void *> SpecConstants;

  // Used only in Object state.  Contains the build flags from the last call to
  // piProgramCompile().
  std::string BuildFlags;

  // The Level Zero module handle.  Used primarily in Exe state.
  ur_program_handle_t UrProgram;

  // The Level Zero module handle.  Used primarily in Exe state.
  ur_module_handle_t UrModule;

  ur_device_handle_t UrDevice;

  // The Level Zero build log from the last call to zeModuleCreate().
  ze_module_build_log_handle_t ZeBuildLog;
};

struct _pi_kernel :  _pi_object {
  _pi_kernel(ur_kernel_handle_t UrKernel): UrKernel{UrKernel}   {}
  _pi_kernel(char *NativeHandle, bool OwnNativeHandle):
    UrKernel{UrKernel}, NativeHandle{NativeHandle}, OwnNativeHandle{OwnNativeHandle}   {}

  ur_kernel_handle_t UrKernel {};
  _pi_program *PiProgram = nullptr;
  char *NativeHandle = nullptr;
  bool OwnNativeHandle = false;
};

struct _pi_mem : _pi_object {
  _pi_mem(ur_context_handle_t UrContext): UrContext{UrContext}  {}
  _pi_mem(ur_context_handle_t UrContext,
          size_t Size,
          void* NativeHandle,
          bool OwnNativeHandle): UrContext{UrContext},
                                 Size{Size},
                                 NativeHandle{NativeHandle},
                                 OwnNativeHandle{OwnNativeHandle}  {}

  ur_context_handle_t UrContext {};
  ur_mem_handle_t UrMemory {};
  size_t Size = 0;
  void *NativeHandle {};
  bool OwnNativeHandle = false;

  // Method to get type of the derived object (image or buffer)
  virtual bool isImage() const = 0;
};

struct _pi_buffer : _pi_mem {
  _pi_buffer(ur_context_handle_t UrContext): _pi_mem(UrContext)  {}
  _pi_buffer(ur_context_handle_t UrContext,
             size_t Size,
             void* NativeHandle,
             bool OwnNativeHandle): _pi_mem(UrContext, Size, NativeHandle, OwnNativeHandle) {}

  bool isImage() const override { return false; }
};

struct _pi_image : _pi_mem {
  _pi_image(ur_context_handle_t UrContext): _pi_mem(UrContext)  {}
  _pi_image(ur_context_handle_t UrContext,
             size_t Size,
             void* NativeHandle,
             bool OwnNativeHandle): _pi_mem(UrContext, Size, NativeHandle, OwnNativeHandle) {}

  bool isImage() const override { return true; }
};

struct _ur_sampler_handle_t : _pi_object {
  _ur_sampler_handle_t(ze_sampler_handle_t Sampler) : ZeSampler{Sampler} {}

  // Level Zero sampler handle.
  ze_sampler_handle_t ZeSampler;
};

struct _pi_sampler : _pi_object {
  _pi_sampler(ur_sampler_handle_t UrSampler) : UrSampler{UrSampler} {}

  // UR sampler handle.
  ur_sampler_handle_t UrSampler;
};


struct _pi_event : _pi_object {
  _pi_event(ur_event_handle_t UrEvent) : UrEvent{UrEvent} {}

  // UR sampler handle.
  ur_event_handle_t UrEvent {};
  std::atomic<uint32_t> RefCountExternal{0};

  // Indicates if we own the ZeEvent or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  bool OwnZeEvent;
};


// inline pi_result piContextCreate(const pi_context_properties *Properties,
//                           pi_uint32 NumDevices, const pi_device *Devices,
//                           void (*PFnNotify)(const char *ErrInfo,
//                                             const void *PrivateInfo, size_t CB,
//                                             void *UserData),
//                           void *UserData, pi_context *RetContext) {
//   printf("%s %d\n", __FILE__, __LINE__);
//   uint32_t DeviceCount = reinterpret_cast<uint32_t>(NumDevices);
//   ur_device_handle_t *phDevices = reinterpret_cast<ur_device_handle_t *>(const_cast<pi_device *>(Devices));
//   ur_context_handle_t *phContext = reinterpret_cast<ur_context_handle_t *>(RetContext);

//   HANDLE_ERRORS(urContextCreate(DeviceCount, phDevices, phContext));

//   return PI_SUCCESS;
// }

// inline pi_result piContextGetInfo(pi_context Context, pi_context_info ParamName,
//                            size_t ParamValueSize, void *ParamValue,
//                            size_t *ParamValueSizeRet) {

//   ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);
//   ur_context_info_t ContextInfoType{};
//   (urContextGetInfo(hContext, ContextInfoType, ParamValueSize, ParamValue, ParamValueSizeRet));

//   return PI_SUCCESS;
// }

// inline pi_result piContextRetain(pi_context Context) {
//   ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);

//   (urContextRetain(hContext));

//   return PI_SUCCESS;
// }


// inline pi_result piContextRelease(pi_context Context) {
//   ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);

//   (urContextRelease(hContext));

//   return PI_SUCCESS;
// }

} // namespace pi2ur
