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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueDeviceGlobalVariableWrite(
    ur_queue_handle_t hQueue,     ///< [in] handle of the queue to submit to.
    ur_program_handle_t hProgram, ///< [in] handle of the program containing the
                                  ///< device global variable.
    const char
        *name, ///< [in] the unique identifier for the device global variable.
    bool blockingWrite, ///< [in] indicates if this operation should block.
    size_t count,       ///< [in] the number of bytes to copy.
    size_t offset, ///< [in] the byte offset into the device global variable to
                   ///< start copying.
    const void *pSrc, ///< [in] pointer to where the data must be copied from.
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list.
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueDeviceGlobalVariableRead(
    ur_queue_handle_t hQueue,     ///< [in] handle of the queue to submit to.
    ur_program_handle_t hProgram, ///< [in] handle of the program containing the
                                  ///< device global variable.
    const char
        *name, ///< [in] the unique identifier for the device global variable.
    bool blockingRead, ///< [in] indicates if this operation should block.
    size_t count,      ///< [in] the number of bytes to copy.
    size_t offset, ///< [in] the byte offset into the device global variable to
                   ///< start copying.
    void *pDst,    ///< [in] pointer to where the data must be copied to.
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list.
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelCreate(
    ur_program_handle_t hProgram, ///< [in] handle of the program instance
    const char *pKernelName,      ///< [in] pointer to null-terminated string.
    ur_kernel_handle_t
        *phKernel ///< [out] pointer to handle of kernel object created.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgValue(
    ur_kernel_handle_t hKernel, ///< [in] handle of the kernel object
    uint32_t argIndex, ///< [in] argument index in range [0, num args - 1]
    size_t argSize,    ///< [in] size of argument type
    const void
        *pArgValue ///< [in] argument value represented as matching arg type.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
    ur_kernel_handle_t hKernel, ///< [in] handle of the Kernel object
    ur_kernel_info_t propName,  ///< [in] name of the Kernel property to query
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelGetGroupInfo(
    ur_kernel_handle_t hKernel, ///< [in] handle of the Kernel object
    ur_device_handle_t hDevice, ///< [in] handle of the Device object
    ur_kernel_group_info_t
        propName,     ///< [in] name of the work Group property to query
    size_t propSize,  ///< [in] size of the Kernel Work Group property value
    void *pPropValue, ///< [in,out][optional][range(0, propSize)] value of the
                      ///< Kernel Work Group property.
    size_t *pPropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data being queried by propName.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelGetSubGroupInfo(
    ur_kernel_handle_t hKernel, ///< [in] handle of the Kernel object
    ur_device_handle_t hDevice, ///< [in] handle of the Device object
    ur_kernel_sub_group_info_t
        propName,     ///< [in] name of the SubGroup property to query
    size_t propSize,  ///< [in] size of the Kernel SubGroup property value
    void *pPropValue, ///< [in,out][range(0, propSize)][optional] value of the
                      ///< Kernel SubGroup property.
    size_t *pPropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data being queried by propName.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelRetain(
    ur_kernel_handle_t hKernel ///< [in] handle for the Kernel to retain
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelRelease(
    ur_kernel_handle_t hKernel ///< [in] handle for the Kernel to release
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgPointer(
    ur_kernel_handle_t hKernel, ///< [in] handle of the kernel object
    uint32_t argIndex,    ///< [in] argument index in range [0, num args - 1]
    size_t argSize,       ///< [in] size of argument type
    const void *pArgValue ///< [in][optional] SVM pointer to memory location
                          ///< holding the argument value. If null then argument
                          ///< value is considered null.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetExecInfo(
    ur_kernel_handle_t hKernel,     ///< [in] handle of the kernel object
    ur_kernel_exec_info_t propName, ///< [in] name of the execution attribute
    size_t propSize,                ///< [in] size in byte the attribute value
    const void *pPropValue ///< [in][range(0, propSize)] pointer to memory
                           ///< location holding the property value.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgSampler(
    ur_kernel_handle_t hKernel, ///< [in] handle of the kernel object
    uint32_t argIndex, ///< [in] argument index in range [0, num args - 1]
    ur_sampler_handle_t hArgValue ///< [in] handle of Sampler object.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgMemObj(
    ur_kernel_handle_t hKernel, ///< [in] handle of the kernel object
    uint32_t argIndex, ///< [in] argument index in range [0, num args - 1]
    ur_mem_handle_t hArgValue ///< [in][optional] handle of Memory object.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelGetNativeHandle(
    ur_kernel_handle_t hKernel, ///< [in] handle of the kernel.
    ur_native_handle_t
        *phNativeKernel ///< [out] a pointer to the native handle of the kernel.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelCreateWithNativeHandle(
    ur_native_handle_t hNativeKernel, ///< [in] the native handle of the kernel.
    ur_context_handle_t hContext,     ///< [in] handle of the context object
    ur_kernel_handle_t
        *phKernel ///< [out] pointer to the handle of the kernel object created.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
