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
#include "ur_level_zero_module.hpp"
#include <ur_bindings.hpp>

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueKernelLaunch(
    ur_queue_handle_t hQueue,   ///< [in] handle of the queue object
    ur_kernel_handle_t hKernel, ///< [in] handle of the kernel object
    uint32_t workDim, ///< [in] number of dimensions, from 1 to 3, to specify
                      ///< the global and work-group work-items
    const size_t
        *pGlobalWorkOffset, ///< [in] pointer to an array of workDim unsigned
                            ///< values that specify the offset used to
                            ///< calculate the global ID of a work-item
    const size_t *pGlobalWorkSize, ///< [in] pointer to an array of workDim
                                   ///< unsigned values that specify the number
                                   ///< of global work-items in workDim that
                                   ///< will execute the kernel function
    const size_t
        *pLocalWorkSize, ///< [in][optional] pointer to an array of workDim
                         ///< unsigned values that specify the number of local
                         ///< work-items forming a work-group that will execute
                         ///< the kernel function. If nullptr, the runtime
                         ///< implementation will choose the work-group size.
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before the kernel execution. If nullptr, the
                          ///< numEventsInWaitList must be 0, indicating that no
                          ///< wait event.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular kernel execution instance.
) {

  _ur_queue_handle_t *UrQueue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _ur_kernel_handle_t *UrKernel = ur_cast<_ur_kernel_handle_t *>(hKernel);

  // printf("%s %d UrQueue %lx\n", __FILE__, __LINE__, (unsigned long int)UrQueue);
  // printf("%s %d UrQueue->Device %lx\n", __FILE__, __LINE__, (unsigned long int)UrQueue->Device);
  // printf("%s %d UrKernel %lx\n", __FILE__, __LINE__, (unsigned long int)UrKernel);
  // if (UrKernel) {
  //   printf("%s %d UrKernel->Program %lx\n", __FILE__, __LINE__, (unsigned long int)UrKernel->Program);
  // }
  // printf("%s %d phEvent %lx\n", __FILE__, __LINE__, (unsigned long int)phEvent);
  

  // Lock automatically releases when this goes out of scope.
  std::scoped_lock<pi_shared_mutex, pi_shared_mutex, pi_shared_mutex> Lock(
      UrQueue->Mutex, UrKernel->Mutex, UrKernel->Program->Mutex);
  if (pGlobalWorkOffset != NULL) {
    if (!UrQueue->Device->Platform->ZeDriverGlobalOffsetExtensionFound) {
      zePrint("No global offset extension found on this driver\n");
      return UR_RESULT_ERROR_INVALID_VALUE;
    }

    ZE2UR_CALL(zeKernelSetGlobalOffsetExp, (UrKernel->ZeKernel,
                                            pGlobalWorkOffset[0],
                                            pGlobalWorkOffset[1],
                                            pGlobalWorkOffset[2]));
  }

  // If there are any pending arguments set them now.
  for (auto &Arg : UrKernel->PendingArguments) {
    // The ArgValue may be a NULL pointer in which case a NULL value is used for
    // the kernel argument declared as a pointer to global or constant memory.
    char **ZeHandlePtr = nullptr;
    if (Arg.Value) {
        // printf("%s %d Arg.Value %lx\n", __FILE__, __LINE__, (unsigned long int)Arg.Value);
    //   _pi_buffer *ArgumentBuffer = reinterpret_cast<_pi_buffer *>(Arg.Value);
      UR_CALL(Arg.Value->getZeHandlePtr(ZeHandlePtr,
                                        Arg.AccessMode,
                                        UrQueue->Device));
        // printf("%s %d\n", __FILE__, __LINE__);
    }
    // printf("%s %d\n", __FILE__, __LINE__);
    ZE2UR_CALL(zeKernelSetArgumentValue, (UrKernel->ZeKernel,
                                          Arg.Index,
                                          Arg.Size,
                                          ZeHandlePtr));
  }
  // printf("%s %d\n", __FILE__, __LINE__);
  UrKernel->PendingArguments.clear();

  ze_group_count_t ZeThreadGroupDimensions{1, 1, 1};
  uint32_t WG[3] {};
  // printf("%s %d\n", __FILE__, __LINE__);

#if 0
  // global_work_size of unused dimensions must be set to 1
  PI_ASSERT(WorkDim == 3 || GlobalWorkSize[2] == 1, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(WorkDim >= 2 || GlobalWorkSize[1] == 1, PI_ERROR_INVALID_VALUE);
#endif

  if (pLocalWorkSize) {
    WG[0] = static_cast<uint32_t>(pLocalWorkSize[0]);
    WG[1] = static_cast<uint32_t>(pLocalWorkSize[1]);
    WG[2] = static_cast<uint32_t>(pLocalWorkSize[2]);
  } else {
    // We can't call to zeKernelSuggestGroupSize if 64-bit GlobalWorkSize
    // values do not fit to 32-bit that the API only supports currently.
    bool SuggestGroupSize = true;
    for (int I : {0, 1, 2}) {
      if (pGlobalWorkSize[I] > UINT32_MAX) {
        SuggestGroupSize = false;
      }
    }
    if (SuggestGroupSize) {
      ZE2UR_CALL(zeKernelSuggestGroupSize, (UrKernel->ZeKernel,
                                            pGlobalWorkSize[0],
                                            pGlobalWorkSize[1],
                                            pGlobalWorkSize[2],
                                            &WG[0],
                                            &WG[1],
                                            &WG[2]));
    } else {
      for (int I : {0, 1, 2}) {
        // Try to find a I-dimension WG size that the GlobalWorkSize[I] is
        // fully divisable with. Start with the max possible size in
        // each dimension.
        uint32_t GroupSize[] = {
            UrQueue->Device->ZeDeviceComputeProperties->maxGroupSizeX,
            UrQueue->Device->ZeDeviceComputeProperties->maxGroupSizeY,
            UrQueue->Device->ZeDeviceComputeProperties->maxGroupSizeZ};
        GroupSize[I] = std::min(size_t(GroupSize[I]), pGlobalWorkSize[I]);
        while (pGlobalWorkSize[I] % GroupSize[I]) {
          --GroupSize[I];
        }
        if (pGlobalWorkSize[I] / GroupSize[I] > UINT32_MAX) {
          zePrint("urEnqueueKernelLaunch: can't find a WG size "
                  "suitable for global work size > UINT32_MAX\n");
          return UR_RESULT_ERROR_INVALID_WORK_GROUP_SIZE;
        }
        WG[I] = GroupSize[I];
      }
      zePrint("urEnqueueKernelLaunch: using computed WG size = {%d, %d, %d}\n",
              WG[0], WG[1], WG[2]);
    }
  }

  // printf("%s %d\n", __FILE__, __LINE__);

  // TODO: assert if sizes do not fit into 32-bit?

  switch (workDim) {
  case 3:
    ZeThreadGroupDimensions.groupCountX =
        static_cast<uint32_t>(pGlobalWorkSize[0] / WG[0]);
    ZeThreadGroupDimensions.groupCountY =
        static_cast<uint32_t>(pGlobalWorkSize[1] / WG[1]);
    ZeThreadGroupDimensions.groupCountZ =
        static_cast<uint32_t>(pGlobalWorkSize[2] / WG[2]);
    break;
  case 2:
    ZeThreadGroupDimensions.groupCountX =
        static_cast<uint32_t>(pGlobalWorkSize[0] / WG[0]);
    ZeThreadGroupDimensions.groupCountY =
        static_cast<uint32_t>(pGlobalWorkSize[1] / WG[1]);
    WG[2] = 1;
    break;
  case 1:
    ZeThreadGroupDimensions.groupCountX =
        static_cast<uint32_t>(pGlobalWorkSize[0] / WG[0]);
    WG[1] = WG[2] = 1;
    break;

  default:
    zePrint("piEnqueueKernelLaunch: unsupported work_dim\n");
    return UR_RESULT_ERROR_INVALID_VALUE;
  }

  // printf("%s %d\n", __FILE__, __LINE__);

  // Error handling for non-uniform group size case
  if (pGlobalWorkSize[0] !=
      size_t(ZeThreadGroupDimensions.groupCountX) * WG[0]) {
    zePrint("urEnqueueKernelLaunch: invalid work_dim. The range is not a "
            "multiple of the group size in the 1st dimension\n");
    return UR_RESULT_ERROR_INVALID_WORK_GROUP_SIZE;
  }
  if (pGlobalWorkSize[1] !=
      size_t(ZeThreadGroupDimensions.groupCountY) * WG[1]) {
    zePrint("urEnqueueKernelLaunch: invalid work_dim. The range is not a "
            "multiple of the group size in the 2nd dimension\n");
    return UR_RESULT_ERROR_INVALID_WORK_GROUP_SIZE;
  }
  if (pGlobalWorkSize[2] !=
      size_t(ZeThreadGroupDimensions.groupCountZ) * WG[2]) {
    zePrint("piEnqueueKernelLaunch: invalid work_dim. The range is not a "
            "multiple of the group size in the 3rd dimension\n");
    return UR_RESULT_ERROR_INVALID_WORK_GROUP_SIZE;
  }

  // printf("%s %d\n", __FILE__, __LINE__);

  ZE2UR_CALL(zeKernelSetGroupSize, (UrKernel->ZeKernel,
                                    WG[0],
                                    WG[1],
                                    WG[2]));

  bool UseCopyEngine = false;
  _pi_ze_event_list_t TmpWaitList;
  // printf("%s %d\n", __FILE__, __LINE__);
  UR_CALL(TmpWaitList.createAndRetainPiZeEventList(numEventsInWaitList,
                                                          reinterpret_cast<const ur_event_handle_t *>(phEventWaitList),
                                                          reinterpret_cast<ur_queue_handle_t>(UrQueue), 
                                                          UseCopyEngine));

  // Get a new command list to be used on this call
  pi_command_list_ptr_t CommandList{};
  UR_CALL(UrQueue->Context->getAvailableCommandList(hQueue,
                                                           CommandList,
                                                           UseCopyEngine,
                                                           true /* AllowBatching */));

  ze_event_handle_t ZeEvent = nullptr;
#if 0
  ur_event_handle_t InternalEvent {};
  bool IsInternal = phEvent == nullptr;
  ur_event_handle_t *hEvent = phEvent ? phEvent : &InternalEvent;
#else
  ur_event_handle_t InternalEvent {};
  bool IsInternal = phEvent == nullptr;
  ur_event_handle_t *hEvent = phEvent ? phEvent : &InternalEvent;
#endif

  // printf("%s %d IsInternal %d phEvent %lx hEvent %lx\n",
  //   __FILE__, __LINE__,
  //   IsInternal,
  //   (unsigned long int)phEvent,
  //   (unsigned long int)hEvent);

  UR_CALL(createEventAndAssociateQueue(hQueue,
                                       hEvent,
                                       PI_COMMAND_TYPE_NDRANGE_KERNEL,
                                       CommandList,
                                       IsInternal));
  
  // printf("%s %d\n", __FILE__, __LINE__);
  _ur_event_handle_t **pUrEvent = reinterpret_cast<_ur_event_handle_t **>(phEvent);
  
  ZeEvent = (*pUrEvent)->ZeEvent;
  (*pUrEvent)->WaitList = TmpWaitList;

  // Save the kernel in the event, so that when the event is signalled
  // the code can do a piKernelRelease on this kernel.
  (*pUrEvent)->CommandData = (void *)UrKernel;

  // Increment the reference count of the Kernel and indicate that the Kernel is
  // in use. Once the event has been signalled, the code in
  // CleanupCompletedEvent(Event) will do a piReleaseKernel to update the
  // reference count on the kernel, using the kernel saved in CommandData.
#if 0
  UR_CALL(piKernelRetain(UrKernel));
#endif

  // Add to list of kernels to be submitted
  if (IndirectAccessTrackingEnabled)
    UrQueue->KernelsToBeSubmitted.push_back(reinterpret_cast<ur_kernel_handle_t>(UrKernel));

  if (UrQueue->Device->useImmediateCommandLists() &&
      IndirectAccessTrackingEnabled) {
    // If using immediate commandlists then gathering of indirect
    // references and appending to the queue (which means submission)
    // must be done together.
    std::unique_lock<pi_shared_mutex> ContextsLock(
        UrQueue->Device->Platform->ContextsMutex, std::defer_lock);
    // We are going to submit kernels for execution. If indirect access flag is
    // set for a kernel then we need to make a snapshot of existing memory
    // allocations in all contexts in the platform. We need to lock the mutex
    // guarding the list of contexts in the platform to prevent creation of new
    // memory alocations in any context before we submit the kernel for
    // execution.
    ContextsLock.lock();
    UrQueue->CaptureIndirectAccesses();
    // Add the command to the command list, which implies submission.
    ZE2UR_CALL(zeCommandListAppendLaunchKernel, (CommandList->first,
                                                 UrKernel->ZeKernel,
                                                 &ZeThreadGroupDimensions,
                                                 ZeEvent,
                                                 (*pUrEvent)->WaitList.Length,
                                                 (*pUrEvent)->WaitList.ZeEventList));
  } else {
    // Add the command to the command list for later submission.
    // No lock is needed here, unlike the immediate commandlist case above,
    // because the kernels are not actually submitted yet. Kernels will be
    // submitted only when the comamndlist is closed. Then, a lock is held.
    ZE2UR_CALL(zeCommandListAppendLaunchKernel, (CommandList->first,
                                                 UrKernel->ZeKernel,
                                                 &ZeThreadGroupDimensions,
                                                 ZeEvent,
                                                 (*pUrEvent)->WaitList.Length,
                                                 (*pUrEvent)->WaitList.ZeEventList));
  }

  // printf("%s %d\n", __FILE__, __LINE__);

  zePrint("calling zeCommandListAppendLaunchKernel() with"
          "  ZeEvent %#llx\n", ur_cast<std::uintptr_t>(ZeEvent));
#if 0
  printZeEventList((*pUrEvent)->WaitList);
#endif

  // Execute command list asynchronously, as the event will be used
  // to track down its completion.
  UR_CALL(UrQueue->executeCommandList(CommandList, false, true));

  // printf("%s %d\n", __FILE__, __LINE__);

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueDeviceGlobalVariableWrite(
    ur_queue_handle_t Queue,     ///< [in] handle of the queue to submit to.
    ur_program_handle_t Program, ///< [in] handle of the program containing the
                                  ///< device global variable.
    const char
        *Name, ///< [in] the unique identifier for the device global variable.
    bool BlockingWrite, ///< [in] indicates if this operation should block.
    size_t Count,       ///< [in] the number of bytes to copy.
    size_t Offset, ///< [in] the byte offset into the device global variable to
                   ///< start copying.
    const void *Src, ///< [in] pointer to where the data must be copied from.
    uint32_t NumEventsInWaitList, ///< [in] size of the event wait list.
    const ur_event_handle_t
        *EventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before the kernel execution. If nullptr, the
                          ///< numEventsInWaitList must be 0, indicating that no
                          ///< wait event.
    ur_event_handle_t
        *Event ///< [in,out][optional] return an event object that identifies
                 ///< this particular kernel execution instance.
) {
  std::scoped_lock<pi_shared_mutex> lock(Queue->Mutex);

  // Find global variable pointer
  size_t GlobalVarSize = 0;
  void *GlobalVarPtr = nullptr;
  ZE2UR_CALL(zeModuleGetGlobalPointer, (Program->ZeModule,
                                        Name,
                                        &GlobalVarSize,
                                        &GlobalVarPtr));
  if (GlobalVarSize < Offset + Count) {
    setErrorMessage("Write device global variable is out of range.",
                    UR_RESULT_ERROR_INVALID_VALUE);
    return UR_RESULT_ERROR_UNKNOWN;
  }

  // Copy engine is preferred only for host to device transfer.
  // Device to device transfers run faster on compute engines.
  bool PreferCopyEngine = !IsDevicePointer(Queue->Context, Src);

  // Temporary option added to use copy engine for D2D copy
  PreferCopyEngine |= UseCopyEngineForD2DCopy;

  return enqueueMemCopyHelper(PI_COMMAND_TYPE_DEVICE_GLOBAL_VARIABLE_WRITE,
                              Queue,
                              ur_cast<char *>(GlobalVarPtr) + Offset,
                              BlockingWrite,
                              Count,
                              Src,
                              NumEventsInWaitList,
                              EventWaitList,
                              Event,
                              PreferCopyEngine);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueDeviceGlobalVariableRead(
    ur_queue_handle_t Queue,     ///< [in] handle of the queue to submit to.
    ur_program_handle_t Program, ///< [in] handle of the program containing the
                                  ///< device global variable.
    const char
        *Name, ///< [in] the unique identifier for the device global variable.
    bool BlockingRead, ///< [in] indicates if this operation should block.
    size_t Count,      ///< [in] the number of bytes to copy.
    size_t Offset, ///< [in] the byte offset into the device global variable to
                   ///< start copying.
    void *Dst,    ///< [in] pointer to where the data must be copied to.
    uint32_t NumEventsInWaitList, ///< [in] size of the event wait list.
    const ur_event_handle_t
        *EventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before the kernel execution. If nullptr, the
                          ///< numEventsInWaitList must be 0, indicating that no
                          ///< wait event.
    ur_event_handle_t
        *Event ///< [in,out][optional] return an event object that identifies
                 ///< this particular kernel execution instance.
) {
  
  std::scoped_lock<pi_shared_mutex> lock(Queue->Mutex);

  // Find global variable pointer
  size_t GlobalVarSize = 0;
  void *GlobalVarPtr = nullptr;
  ZE2UR_CALL(zeModuleGetGlobalPointer, (Program->ZeModule,
                                        Name,
                                        &GlobalVarSize,
                                        &GlobalVarPtr));
  if (GlobalVarSize < Offset + Count) {
    setErrorMessage("Read from device global variable is out of range.",
                    UR_RESULT_ERROR_INVALID_VALUE);
    return UR_RESULT_ERROR_UNKNOWN;
  }

  // Copy engine is preferred only for host to device transfer.
  // Device to device transfers run faster on compute engines.
  bool PreferCopyEngine = !IsDevicePointer(Queue->Context, Dst);

  // Temporary option added to use copy engine for D2D copy
  PreferCopyEngine |= UseCopyEngineForD2DCopy;

  return enqueueMemCopyHelper(PI_COMMAND_TYPE_DEVICE_GLOBAL_VARIABLE_READ,
                              Queue,
                              Dst,
                              BlockingRead,
                              Count,
                              ur_cast<char *>(GlobalVarPtr) + Offset,
                              NumEventsInWaitList,
                              EventWaitList,
                              Event,
                              PreferCopyEngine);
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelCreate(
    ur_program_handle_t hProgram, ///< [in] handle of the program instance
    const char *pKernelName,      ///< [in] pointer to null-terminated string.
    ur_kernel_handle_t
        *phKernel ///< [out] pointer to handle of kernel object created.
) {
  _ur_program_handle_t *UrProgram = reinterpret_cast<_ur_program_handle_t *>(hProgram);

  std::shared_lock<pi_shared_mutex> Guard(UrProgram->Mutex);
  if (UrProgram->State != _ur_program_handle_t::state::Exe) {
    return UR_RESULT_ERROR_INVALID_PROGRAM_EXECUTABLE;
  }

  // printf("%s %d UrProgram %lx\n", __FILE__, __LINE__, (unsigned long int)UrProgram);

  ZeStruct<ze_kernel_desc_t> ZeKernelDesc;
  ZeKernelDesc.flags = 0;
  ZeKernelDesc.pKernelName = pKernelName;

  ze_kernel_handle_t ZeKernel;
  ZE2UR_CALL(zeKernelCreate, (UrProgram->ZeModule, &ZeKernelDesc, &ZeKernel));

  try {
    _ur_kernel_handle_t *UrKernel = new _ur_kernel_handle_t(ZeKernel, true, hProgram);
    *phKernel = reinterpret_cast<ur_kernel_handle_t>(UrKernel);
    // printf("%s %d UrKernel %lx\n", __FILE__, __LINE__, (unsigned long int)UrKernel);
    // printf("%s %d UrKernel->Program %lx\n", __FILE__, __LINE__, (unsigned long int)UrKernel->Program);
  } catch (const std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  UR_CALL((*phKernel)->initialize());

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgValue(
    ur_kernel_handle_t Kernel, ///< [in] handle of the kernel object
    uint32_t ArgIndex, ///< [in] argument index in range [0, num args - 1]
    size_t ArgSize,    ///< [in] size of argument type
    const void
        *PArgValue ///< [in] argument value represented as matching arg type.
) {
  // OpenCL: "the arg_value pointer can be NULL or point to a NULL value
  // in which case a NULL value will be used as the value for the argument
  // declared as a pointer to global or constant memory in the kernel"
  //
  // We don't know the type of the argument but it seems that the only time
  // SYCL RT would send a pointer to NULL in 'arg_value' is when the argument
  // is a NULL pointer. Treat a pointer to NULL in 'arg_value' as a NULL.
  if (ArgSize == sizeof(void *) && PArgValue &&
      *(void **)(const_cast<void *>(PArgValue)) == nullptr) {
    PArgValue = nullptr;
  }

  std::scoped_lock<pi_shared_mutex> Guard(Kernel->Mutex);
  ZE2UR_CALL(zeKernelSetArgumentValue, (Kernel->ZeKernel,
                                        ArgIndex,
                                        ArgSize,
                                        PArgValue));

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgLocal(
    ur_kernel_handle_t hKernel, ///< [in] handle of the kernel object
    uint32_t argIndex, ///< [in] argument index in range [0, num args - 1]
    size_t argSize     ///< [in] size of the local buffer to be allocated by the
                       ///< runtime
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelGetInfo(
    ur_kernel_handle_t Kernel, ///< [in] handle of the Kernel object
    ur_kernel_info_t ParamName,  ///< [in] name of the Kernel property to query
    size_t propSize,            ///< [in] the size of the Kernel property value.
    void *pKernelInfo, ///< [in,out][optional] array of bytes holding the kernel
                       ///< info property. If propSize is not equal to or
                       ///< greater than the real number of bytes needed to
                       ///< return the info then the
                       ///< ::UR_RESULT_ERROR_INVALID_SIZE error is returned and
                       ///< pKernelInfo is not used.
    size_t *pPropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data being queried by propName.
) {

  UrL0ReturnHelper ReturnValue(propSize, pKernelInfo, pPropSizeRet);

  std::shared_lock<pi_shared_mutex> Guard(Kernel->Mutex);
  switch (ParamName) {
  case UR_KERNEL_INFO_CONTEXT:
    return ReturnValue(ur_context_handle_t{Kernel->Program->Context});
  case UR_KERNEL_INFO_PROGRAM:
    return ReturnValue(ur_program_handle_t{Kernel->Program});
  case UR_KERNEL_INFO_FUNCTION_NAME:
    try {
      std::string &KernelName = *Kernel->ZeKernelName.operator->();
      return ReturnValue(static_cast<const char *>(KernelName.c_str()));
    } catch (const std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }
  case UR_KERNEL_INFO_NUM_ARGS:
    return ReturnValue(uint32_t{Kernel->ZeKernelProperties->numKernelArgs});
  case UR_KERNEL_INFO_REFERENCE_COUNT:
    return ReturnValue(uint32_t{Kernel->RefCount.load()});
  case UR_KERNEL_INFO_ATTRIBUTES:
    try {
      uint32_t Size;
      ZE2UR_CALL(zeKernelGetSourceAttributes, (Kernel->ZeKernel,
                                               &Size,
                                               nullptr));
      char *attributes = new char[Size];
      ZE2UR_CALL(zeKernelGetSourceAttributes,
              (Kernel->ZeKernel, &Size, &attributes));
      auto Res = ReturnValue(attributes);
      delete[] attributes;
      return Res;
    } catch (const std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }
  default:
    zePrint("Unsupported ParamName in piKernelGetInfo: ParamName=%d(0x%x)\n",
            ParamName, ParamName);
    return UR_RESULT_ERROR_INVALID_VALUE;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelGetGroupInfo(
    ur_kernel_handle_t Kernel, ///< [in] handle of the Kernel object
    ur_device_handle_t Device, ///< [in] handle of the Device object
    ur_kernel_group_info_t
        ParamName,     ///< [in] name of the work Group property to query
    size_t ParamValueSize,  ///< [in] size of the Kernel Work Group property value
    void *ParamValue, ///< [in,out][optional][range(0, propSize)] value of the
                      ///< Kernel Work Group property.
    size_t *ParamValueSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data being queried by propName.
) {
  UrL0ReturnHelper ReturnValue(ParamValueSize, ParamValue, ParamValueSizeRet);

  std::shared_lock<pi_shared_mutex> Guard(Kernel->Mutex);
  switch (ParamName) {
    case UR_KERNEL_GROUP_INFO_GLOBAL_WORK_SIZE: {
      // TODO: To revisit after level_zero/issues/262 is resolved
      struct {
        size_t Arr[3];
      } WorkSize = {{Device->ZeDeviceComputeProperties->maxGroupSizeX,
                    Device->ZeDeviceComputeProperties->maxGroupSizeY,
                    Device->ZeDeviceComputeProperties->maxGroupSizeZ}};
      return ReturnValue(WorkSize);
    }
    case UR_KERNEL_GROUP_INFO_WORK_GROUP_SIZE: {
      // As of right now, L0 is missing API to query kernel and device specific
      // max work group size.
      return ReturnValue(
          pi_uint64{Device->ZeDeviceComputeProperties->maxTotalGroupSize});
    }
    case UR_KERNEL_GROUP_INFO_COMPILE_WORK_GROUP_SIZE: {
      struct {
        size_t Arr[3];
      } WgSize = {{Kernel->ZeKernelProperties->requiredGroupSizeX,
                  Kernel->ZeKernelProperties->requiredGroupSizeY,
                  Kernel->ZeKernelProperties->requiredGroupSizeZ}};
      return ReturnValue(WgSize);
    }
    case UR_KERNEL_GROUP_INFO_LOCAL_MEM_SIZE:
      return ReturnValue(pi_uint32{Kernel->ZeKernelProperties->localMemSize});
    case UR_KERNEL_GROUP_INFO_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: {
      return ReturnValue(size_t{Device->ZeDeviceProperties->physicalEUSimdWidth});
    }
    case UR_KERNEL_GROUP_INFO_PRIVATE_MEM_SIZE: {
      return ReturnValue(pi_uint32{Kernel->ZeKernelProperties->privateMemSize});
    }
    default: {
      zePrint("Unknown ParamName in urKernelGetGroupInfo: ParamName=%d(0x%x)\n",
              ParamName, ParamName);
      return UR_RESULT_ERROR_INVALID_VALUE;
    }
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelGetSubGroupInfo(
    ur_kernel_handle_t Kernel, ///< [in] handle of the Kernel object
    ur_device_handle_t Device, ///< [in] handle of the Device object
    ur_kernel_sub_group_info_t PropName,     ///< [in] name of the SubGroup property to query
    size_t PropSize,  ///< [in] size of the Kernel SubGroup property value
    void *PropValue, ///< [in,out][range(0, propSize)][optional] value of the
                      ///< Kernel SubGroup property.
    size_t *PropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data being queried by propName.
) {
  std::ignore = Device;

  UrReturnHelper ReturnValue(PropSize, PropValue, PropSizeRet);

  std::shared_lock<pi_shared_mutex> Guard(Kernel->Mutex);
  if (PropName == UR_KERNEL_SUB_GROUP_INFO_MAX_SUB_GROUP_SIZE) {
    ReturnValue(uint32_t{Kernel->ZeKernelProperties->maxSubgroupSize});
  } else if (PropName == UR_KERNEL_SUB_GROUP_INFO_MAX_NUM_SUB_GROUPS) {
    ReturnValue(uint32_t{Kernel->ZeKernelProperties->maxNumSubgroups});
  } else if (PropName == UR_KERNEL_SUB_GROUP_INFO_COMPILE_NUM_SUB_GROUPS) {
    ReturnValue(uint32_t{Kernel->ZeKernelProperties->requiredNumSubGroups});
  } else if (PropName == UR_KERNEL_SUB_GROUP_INFO_SUB_GROUP_SIZE_INTEL) {
    ReturnValue(uint32_t{Kernel->ZeKernelProperties->requiredSubgroupSize});
  } else {
    die("urKernelGetSubGroupInfo: parameter not implemented");
    return {};
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelRetain(
    ur_kernel_handle_t Kernel ///< [in] handle for the Kernel to retain
) {
  Kernel->RefCount.increment();

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelRelease(
    ur_kernel_handle_t Kernel ///< [in] handle for the Kernel to release
) {
  if (!Kernel->RefCount.decrementAndTest())
    return UR_RESULT_SUCCESS;

  auto KernelProgram = Kernel->Program;
  if (Kernel->OwnZeKernel)
    ZE2UR_CALL(zeKernelDestroy, (Kernel->ZeKernel));
  if (IndirectAccessTrackingEnabled) {
    UR_CALL(urContextRelease(KernelProgram->Context));
  }
  // do a release on the program this kernel was part of
  UR_CALL(urProgramRelease(KernelProgram));
  delete Kernel;

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgPointer(
    ur_kernel_handle_t Kernel, ///< [in] handle of the kernel object
    uint32_t ArgIndex,    ///< [in] argument index in range [0, num args - 1]
    size_t ArgSize,       ///< [in] size of argument type
    const void *ArgValue ///< [in][optional] SVM pointer to memory location
                          ///< holding the argument value. If null then argument
                          ///< value is considered null.
) {
  UR_CALL(urKernelSetArgValue(Kernel, ArgIndex, ArgSize, ArgValue));
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetExecInfo(
    ur_kernel_handle_t hKernel,     ///< [in] handle of the kernel object
    ur_kernel_exec_info_t propName, ///< [in] name of the execution attribute
    size_t propSize,                ///< [in] size in byte the attribute value
    const void *pPropValue ///< [in][range(0, propSize)] pointer to memory
                           ///< location holding the property value.
) {
  std::scoped_lock<pi_shared_mutex> Guard(hKernel->Mutex);
  if (propName == UR_KERNEL_EXEC_INFO_USM_INDIRECT_ACCESS &&
      *(static_cast<const pi_bool *>(pPropValue)) == PI_TRUE) {
    // The whole point for users really was to not need to know anything
    // about the types of allocations kernel uses. So in DPC++ we always
    // just set all 3 modes for each kernel.
    ze_kernel_indirect_access_flags_t IndirectFlags =
        ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST |
        ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE |
        ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED;
    ZE2UR_CALL(zeKernelSetIndirectAccess, (hKernel->ZeKernel,
                                           IndirectFlags));
  } else {
    zePrint("urKernelSetExecInfo: unsupported ParamName\n");
    return UR_RESULT_ERROR_INVALID_VALUE;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgSampler(
    ur_kernel_handle_t Kernel, ///< [in] handle of the kernel object
    uint32_t ArgIndex, ///< [in] argument index in range [0, num args - 1]
    ur_sampler_handle_t ArgValue ///< [in] handle of Sampler object.
) {
  std::scoped_lock<pi_shared_mutex> Guard(Kernel->Mutex);
  ZE2UR_CALL(zeKernelSetArgumentValue, (ur_cast<ze_kernel_handle_t>(Kernel->ZeKernel),
                                        ur_cast<uint32_t>(ArgIndex),
                                        sizeof(void *),
                                        &ArgValue->ZeSampler));

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgMemObj(
    ur_kernel_handle_t Kernel, ///< [in] handle of the kernel object
    uint32_t ArgIndex, ///< [in] argument index in range [0, num args - 1]
    ur_mem_handle_t ArgValue ///< [in][optional] handle of Memory object.
) {
  std::scoped_lock<pi_shared_mutex> Guard(Kernel->Mutex);
  // The ArgValue may be a NULL pointer in which case a NULL value is used for
  // the kernel argument declared as a pointer to global or constant memory.

  _ur_mem_handle_t *UrMem = ur_cast<_ur_mem_handle_t *>(ArgValue);

  // printf("%s %d ArgValue %lx\n", __FILE__, __LINE__, (unsigned long int)ArgValue);

  auto Arg = UrMem ? UrMem : nullptr;
  Kernel->PendingArguments.push_back(
      {ArgIndex, sizeof(void *), Arg, _ur_mem_handle_t::read_write});

  return UR_RESULT_SUCCESS; 
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelGetNativeHandle(
    ur_kernel_handle_t Kernel, ///< [in] handle of the kernel.
    ur_native_handle_t *NativeKernel ///< [out] a pointer to the native handle of the kernel.
) {
  std::shared_lock<pi_shared_mutex> Guard(Kernel->Mutex);

  *NativeKernel = reinterpret_cast<ur_native_handle_t>(Kernel->ZeKernel);
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelCreateWithNativeHandle(
    ur_native_handle_t hNativeKernel, ///< [in] the native handle of the kernel.
    ur_context_handle_t hContext,     ///< [in] handle of the context object
    ur_kernel_handle_t
        *phKernel ///< [out] pointer to the handle of the kernel object created.
) {
  ze_kernel_handle_t ZeKernel = ur_cast<ze_kernel_handle_t>(hNativeKernel);
  _ur_kernel_handle_t *Kernel = nullptr;
  try {
    Kernel = new _ur_kernel_handle_t(ZeKernel,
                                     true, // OwnZeKernel
                                     hContext);
    *phKernel = reinterpret_cast<ur_kernel_handle_t>(Kernel);
  } catch (const std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  UR_CALL(Kernel->initialize());

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urModuleCreate(
    ur_context_handle_t hContext, ///< [in] handle of the context instance.
    const void *pIL,              ///< [in] pointer to IL string.
    size_t length,                ///< [in] length of IL in bytes.
    const char
        *pOptions, ///< [in] pointer to compiler options null-terminated string.
    ur_modulecreate_callback_t
        pfnNotify,   ///< [in][optional] A function pointer to a notification
                     ///< routine that is called when program compilation is
                     ///< complete.
    void *pUserData, ///< [in][optional] Passed as an argument when pfnNotify is
                     ///< called.
    ur_module_handle_t
        *phModule ///< [out] pointer to handle of Module object created.
) {
  // printf("%s %d\n", __FILE__, __LINE__);
  // Construct a ze_module_program_exp_desc_t which contains information about
  // all of the modules that will be linked together.
  ZeStruct<ze_module_program_exp_desc_t> ZeModuleDescExp;
  std::vector<size_t> CodeSizes(1u);
  std::vector<const uint8_t *> CodeBufs(1u);
  std::vector<const char *> BuildFlagPtrs(1u);
// printf("%s %d\n", __FILE__, __LINE__);
  ZeModuleDescExp.count = 1u;
  ZeModuleDescExp.inputSizes = &length;
  ZeModuleDescExp.pInputModules = reinterpret_cast<const uint8_t **>(&pIL);
  ZeModuleDescExp.pBuildFlags = BuildFlagPtrs.data();
// printf("%s %d\n", __FILE__, __LINE__);
  ZeStruct<ze_module_desc_t> ZeModuleDesc;
  ZeModuleDesc.pNext = &ZeModuleDescExp;
  ZeModuleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
// printf("%s %d\n", __FILE__, __LINE__);
  // This works around a bug in the Level Zero driver.  When "ZE_DEBUG=-1",
  // the driver does validation of the API calls, and it expects
  // "pInputModule" to be non-NULL and "inputSize" to be non-zero.  This
  // validation is wrong when using the "ze_module_program_exp_desc_t"
  // extension because those fields are supposed to be ignored.  As a
  // workaround, set both fields to 1.
  //
  // TODO: Remove this workaround when the driver is fixed.
  ZeModuleDesc.pInputModule = reinterpret_cast<const uint8_t *>(1);
  ZeModuleDesc.inputSize = 1;
// printf("%s %d\n", __FILE__, __LINE__);
  ZeModuleDesc.pNext = nullptr;
  ZeModuleDesc.inputSize = ZeModuleDescExp.inputSizes[0];
  ZeModuleDesc.pInputModule = ZeModuleDescExp.pInputModules[0];
  ZeModuleDesc.pBuildFlags = ZeModuleDescExp.pBuildFlags[0];
#if 0
  ZeModuleDesc.pConstants = ZeModuleDescExp.pConstants[0];
#endif
// printf("%s %d\n", __FILE__, __LINE__);
  // Call the Level Zero API to compile, link, and create the module.
  _ur_context_handle_t *UrContext = reinterpret_cast<_ur_context_handle_t *>(hContext);
  // printf("%s %d UrContext %lx\n", __FILE__, __LINE__,
    // (unsigned long int)UrContext);
  // if (UrContext) {
  //   printf("%s %d UrContext->Devices[0] %lx\n", __FILE__, __LINE__, (unsigned long int)UrContext->Devices[0]);
  // }
  ze_device_handle_t ZeDevice = UrContext->Devices[0]->ZeDevice;
  // printf("%s %d\n", __FILE__, __LINE__);
  ze_context_handle_t ZeContext = UrContext->ZeContext;
  // printf("%s %d\n", __FILE__, __LINE__);
  ze_module_handle_t ZeModule = nullptr;
  ze_module_build_log_handle_t ZeBuildLog = nullptr;
  // printf("%s %d\n", __FILE__, __LINE__);
  ZE2UR_CALL(zeModuleCreate, (ZeContext,
                              ZeDevice, &ZeModuleDesc,
                              &ZeModule, &ZeBuildLog));
  // printf("%s %d\n", __FILE__, __LINE__);

  _ur_module_handle_t *UrModule = new _ur_module_handle_t(hContext, pIL, length);

  *phModule = reinterpret_cast<ur_module_handle_t>(UrModule);

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urModuleRetain(
    ur_module_handle_t hModule ///< [in] handle for the Module to retain
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urModuleRelease(
    ur_module_handle_t hModule ///< [in] handle for the Module to release
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urModuleGetNativeHandle(
    ur_module_handle_t hModule, ///< [in] handle of the module.
    ur_native_handle_t
        *phNativeModule ///< [out] a pointer to the native handle of the module.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urModuleCreateWithNativeHandle(
    ur_native_handle_t hNativeModule, ///< [in] the native handle of the module.
    ur_context_handle_t hContext,     ///< [in] handle of the context instance.
    ur_module_handle_t
        *phModule ///< [out] pointer to the handle of the module object created.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}


ur_result_t _ur_kernel_handle_t::initialize() {
  // Retain the program and context to show it's used by this kernel.
  UR_CALL(urProgramRetain(Program));

  if (IndirectAccessTrackingEnabled)
    // TODO: do piContextRetain without the guard
    UR_CALL(urContextRetain(Program->Context));

  // Set up how to obtain kernel properties when needed.
  ZeKernelProperties.Compute = [this](ze_kernel_properties_t &Properties) {
    ZE_CALL_NOCHECK(zeKernelGetProperties, (ZeKernel, &Properties));
  };

  // Cache kernel name.
  ZeKernelName.Compute = [this](std::string &Name) {
    size_t Size = 0;
    ZE_CALL_NOCHECK(zeKernelGetName, (ZeKernel, &Size, nullptr));
    char *KernelName = new char[Size];
    ZE_CALL_NOCHECK(zeKernelGetName, (ZeKernel, &Size, KernelName));
    Name = KernelName;
    delete[] KernelName;
  };

  return UR_RESULT_SUCCESS;
}