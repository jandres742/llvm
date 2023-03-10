//===--- pi_unified_runtime.cpp - Unified Runtime PI Plugin  -----------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//

#include <cstring>

#include <pi2ur.hpp>
#include <pi_unified_runtime.hpp>

// Stub function to where all not yet supported PI API are bound
static void DieUnsupported() {
  die("Unified Runtime: functionality is not supported");
}

// All PI API interfaces are C interfaces
extern "C" {
__SYCL_EXPORT pi_result piPlatformsGet(pi_uint32 num_entries,
                                       pi_platform *platforms,
                                       pi_uint32 *num_platforms) {
  return pi2ur::piPlatformsGet(num_entries, platforms, num_platforms);
}

__SYCL_EXPORT pi_result piPlatformGetInfo(pi_platform Platform,
                                          pi_platform_info ParamName,
                                          size_t ParamValueSize,
                                          void *ParamValue,
                                          size_t *ParamValueSizeRet) {
  return pi2ur::piPlatformGetInfo(Platform, ParamName, ParamValueSize,
                                  ParamValue, ParamValueSizeRet);
}

__SYCL_EXPORT pi_result piDevicesGet(pi_platform Platform,
                                     pi_device_type DeviceType,
                                     pi_uint32 NumEntries, pi_device *Devices,
                                     pi_uint32 *NumDevices) {
  return pi2ur::piDevicesGet(Platform, DeviceType, NumEntries, Devices,
                             NumDevices);
}

__SYCL_EXPORT pi_result piDeviceRetain(pi_device Device) {
  return pi2ur::piDeviceRetain(Device);
}

__SYCL_EXPORT pi_result piDeviceRelease(pi_device Device) {
  return pi2ur::piDeviceRelease(Device);
}

__SYCL_EXPORT pi_result piDeviceGetInfo(pi_device Device,
                                        pi_device_info ParamName,
                                        size_t ParamValueSize, void *ParamValue,
                                        size_t *ParamValueSizeRet) {
  return pi2ur::piDeviceGetInfo(Device, ParamName, ParamValueSize, ParamValue,
                                ParamValueSizeRet);
}

__SYCL_EXPORT pi_result piDevicePartition(
    pi_device Device, const pi_device_partition_property *Properties,
    pi_uint32 NumDevices, pi_device *OutDevices, pi_uint32 *OutNumDevices) {
  return pi2ur::piDevicePartition(Device, Properties, NumDevices, OutDevices,
                                  OutNumDevices);
}

// Stub for the not yet supported API
__SYCL_EXPORT pi_result piextDeviceSelectBinary(pi_device Device,
                        pi_device_binary *Binaries, pi_uint32 NumBinaries,
                        pi_uint32 *SelectedBinaryInd) {
  return pi2ur::piextDeviceSelectBinary(Device, Binaries, NumBinaries, SelectedBinaryInd);
}

__SYCL_EXPORT pi_result piContextCreate(const pi_context_properties *Properties,
                          pi_uint32 NumDevices, const pi_device *Devices,
                          void (*PFnNotify)(const char *ErrInfo,
                                            const void *PrivateInfo, size_t CB,
                                            void *UserData),
                          void *UserData, pi_context *RetContext) {
  return pi2ur::piContextCreate(Properties,
                                NumDevices,
                                Devices,
                                PFnNotify,
                                UserData,
                                RetContext);
}

__SYCL_EXPORT pi_result piContextRelease(pi_context Context) {
  return pi2ur::piContextRelease(Context);
}

__SYCL_EXPORT pi_result piQueueCreate(pi_context Context, pi_device Device,
                        pi_queue_properties Flags, pi_queue *Queue) {
  return pi2ur::piQueueCreate(Context,
                              Device,
                              Flags,
                              Queue);
}

__SYCL_EXPORT pi_result piextQueueCreate(pi_context Context, pi_device Device,
                           pi_queue_properties *Properties, pi_queue *Queue) {
  return pi2ur::piextQueueCreate(Context,
                              Device,
                              Properties,
                              Queue);
}

__SYCL_EXPORT pi_result piQueueRelease(pi_queue Queue) {
  return pi2ur::piQueueRelease(Queue);
}

__SYCL_EXPORT pi_result piProgramCreate(pi_context Context, const void *ILBytes,
                          size_t Length, pi_program *Program) {
  return pi2ur::piProgramCreate(Context, ILBytes,
                                Length, Program);
}

__SYCL_EXPORT pi_result piProgramBuild(pi_program Program, pi_uint32 NumDevices,
                         const pi_device *DeviceList, const char *Options,
                         void (*PFnNotify)(pi_program Program, void *UserData),
                         void *UserData) {
  return pi2ur::piProgramBuild(Program,
                               NumDevices,
                               DeviceList,
                               Options,
                               PFnNotify,
                               UserData);
}

__SYCL_EXPORT pi_result piextProgramSetSpecializationConstant(pi_program Prog,
                                                pi_uint32 SpecID, size_t Size,
                                                const void *SpecValue) {

  return pi2ur::piextProgramSetSpecializationConstant(Prog, SpecID, Size, SpecValue);
}

__SYCL_EXPORT pi_result piProgramLink(pi_context Context,
                               pi_uint32 NumDevices,
                               const pi_device *DeviceList,
                               const char *Options,
                               pi_uint32 NumInputPrograms,
                               const pi_program *InputPrograms,
                               void (*PFnNotify)(pi_program Program, void *UserData),
                               void *UserData,
                               pi_program *RetProgram) {
  return pi2ur::piProgramLink(Context,
                              NumDevices,
                              DeviceList,
                              Options,
                              NumInputPrograms,
                              InputPrograms,
                              PFnNotify,
                              UserData,
                              RetProgram);
}

__SYCL_EXPORT pi_result piKernelCreate(pi_program Program, const char *KernelName,
                         pi_kernel *RetKernel) {
  return pi2ur::piKernelCreate(Program,
                               KernelName,
                               RetKernel);
}

// Special version of piKernelSetArg to accept pi_mem.
__SYCL_EXPORT pi_result
piextKernelSetArgMemObj(pi_kernel Kernel,
                        pi_uint32 ArgIndex,
                        const pi_mem *ArgValue) {

  return pi2ur::piextKernelSetArgMemObj(Kernel,
                                        ArgIndex,
                                        ArgValue);
}

__SYCL_EXPORT pi_result 
piKernelSetArg(pi_kernel Kernel,
               pi_uint32 ArgIndex,
               size_t ArgSize,
               const void *ArgValue) {

  return pi2ur::piKernelSetArg(Kernel,
                               ArgIndex,
                               ArgSize,
                               ArgValue);
}

__SYCL_EXPORT pi_result 
piKernelGetGroupInfo(pi_kernel Kernel,
                     pi_device Device,
                     pi_kernel_group_info ParamName,
                     size_t ParamValueSize,
                     void *ParamValue,
                     size_t *ParamValueSizeRet) {
  return pi2ur::piKernelGetGroupInfo(Kernel,
                                     Device,
                                     ParamName,
                                     ParamValueSize,
                                     ParamValue,
                                     ParamValueSizeRet);
}

__SYCL_EXPORT pi_result
piMemBufferCreate(pi_context Context,
                  pi_mem_flags Flags,
                  size_t Size,
                  void *HostPtr,
                  pi_mem *RetMem,
                  const pi_mem_properties *properties) {

  return pi2ur::piMemBufferCreate(Context,
                                  Flags,
                                  Size,
                                  HostPtr,
                                  RetMem,
                                  properties);
}

__SYCL_EXPORT pi_result piextUSMHostAlloc(void **ResultPtr, pi_context Context,
                            pi_usm_mem_properties *Properties, size_t Size,
                            pi_uint32 Alignment) {

  return pi2ur::piextUSMHostAlloc(ResultPtr,
                                  Context,
                                  Properties,
                                  Size,
                                  Alignment);
}

__SYCL_EXPORT pi_result piMemGetInfo(pi_mem Mem, pi_mem_info ParamName, size_t ParamValueSize,
                       void *ParamValue, size_t *ParamValueSizeRet) {

  return pi2ur::piMemGetInfo(Mem,
                             ParamName,
                             ParamValueSize,
                             ParamValue,
                             ParamValueSizeRet);
}

__SYCL_EXPORT pi_result piMemImageCreate(pi_context Context, pi_mem_flags Flags,
                           const pi_image_format *ImageFormat,
                           const pi_image_desc *ImageDesc, void *HostPtr,
                           pi_mem *RetImage) {

  return pi2ur::piMemImageCreate(Context,
                                 Flags,
                                 ImageFormat,
                                 ImageDesc,
                                 HostPtr,
                                 RetImage);
}

__SYCL_EXPORT pi_result piMemBufferPartition(pi_mem Buffer, pi_mem_flags Flags,
                               pi_buffer_create_type BufferCreateType,
                               void *BufferCreateInfo, pi_mem *RetMem) {
  return pi2ur::piMemBufferPartition(Buffer,
                                     Flags,
                                     BufferCreateType,
                                     BufferCreateInfo,
                                     RetMem);
}

__SYCL_EXPORT pi_result piextMemGetNativeHandle(pi_mem Mem, pi_native_handle *NativeHandle) {
  return pi2ur::piextMemGetNativeHandle(Mem,
                                        NativeHandle);
}

__SYCL_EXPORT pi_result
piEnqueueMemImageCopy(pi_queue Queue, pi_mem SrcImage, pi_mem DstImage,
                      pi_image_offset SrcOrigin, pi_image_offset DstOrigin,
                      pi_image_region Region, pi_uint32 NumEventsInWaitList,
                      const pi_event *EventWaitList, pi_event *Event) {
  return pi2ur::piEnqueueMemImageCopy(Queue,
                                      SrcImage,
                                      DstImage,
                                      SrcOrigin,
                                      DstOrigin,
                                      Region,
                                      NumEventsInWaitList,
                                      EventWaitList,
                                      Event);
}

__SYCL_EXPORT pi_result
piextMemCreateWithNativeHandle(pi_native_handle NativeHandle,
                               pi_context Context,
                               bool ownNativeHandle,
                               pi_mem *Mem) {
  return pi2ur::piextMemCreateWithNativeHandle(NativeHandle,
                                               Context,
                                               ownNativeHandle,
                                               Mem);
}

__SYCL_EXPORT pi_result
piEnqueueKernelLaunch(pi_queue Queue, pi_kernel Kernel, pi_uint32 WorkDim,
                      const size_t *GlobalWorkOffset,
                      const size_t *GlobalWorkSize, const size_t *LocalWorkSize,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventWaitList, pi_event *OutEvent) {

  return pi2ur::piEnqueueKernelLaunch(Queue,
                                      Kernel,
                                      WorkDim,
                                      GlobalWorkOffset,
                                      GlobalWorkSize,
                                      LocalWorkSize,
                                      NumEventsInWaitList,
                                      EventWaitList,
                                      OutEvent);
}

__SYCL_EXPORT pi_result
piEnqueueMemImageWrite(pi_queue Queue, pi_mem Image,
                       pi_bool BlockingWrite, pi_image_offset Origin,
                       pi_image_region Region, size_t InputRowPitch,
                       size_t InputSlicePitch, const void *Ptr,
                       pi_uint32 NumEventsInWaitList,
                       const pi_event *EventWaitList,
                       pi_event *Event) {

  return pi2ur::piEnqueueMemImageWrite(Queue,
                                       Image,
                                       BlockingWrite,
                                       Origin,
                                       Region,
                                       InputRowPitch,
                                       InputSlicePitch,
                                       Ptr,
                                       NumEventsInWaitList,
                                       EventWaitList,
                                       Event);
}

__SYCL_EXPORT pi_result
piEnqueueMemImageRead(pi_queue Queue, pi_mem Image,
                      pi_bool BlockingRead, pi_image_offset Origin,
                      pi_image_region Region, size_t RowPitch,
                      size_t SlicePitch, void *Ptr,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventWaitList,
                      pi_event *Event) {
  return pi2ur::piEnqueueMemImageRead(Queue,
                                      Image,
                                      BlockingRead,
                                      Origin,
                                      Region,
                                      RowPitch,
                                      SlicePitch,
                                      Ptr,
                                      NumEventsInWaitList,
                                      EventWaitList,
                                      Event);
}

__SYCL_EXPORT pi_result piextKernelCreateWithNativeHandle(pi_native_handle NativeHandle,
                                            pi_context Context,
                                            pi_program Program,
                                            bool OwnNativeHandle,
                                            pi_kernel *Kernel) {

  return pi2ur::piextKernelCreateWithNativeHandle(NativeHandle,
                                                  Context,
                                                  Program,
                                                  OwnNativeHandle,
                                                  Kernel);
}

__SYCL_EXPORT pi_result piEnqueueMemUnmap(pi_queue Queue, pi_mem Mem, void *MappedPtr,
                            pi_uint32 NumEventsInWaitList,
                            const pi_event *EventWaitList, pi_event *OutEvent) {
  
  return pi2ur::piEnqueueMemUnmap(Queue,
                                  Mem,
                                  MappedPtr,
                                  NumEventsInWaitList,
                                  EventWaitList,
                                  OutEvent);
}

__SYCL_EXPORT pi_result piEventsWait(pi_uint32 NumEvents, const pi_event *EventList) {

  return pi2ur::piEventsWait(NumEvents,
                             EventList);
}

__SYCL_EXPORT pi_result piQueueFinish(pi_queue Queue) {
  return pi2ur::piQueueFinish(Queue);
}

__SYCL_EXPORT pi_result piEventGetInfo(pi_event Event, pi_event_info ParamName,
                         size_t ParamValueSize, void *ParamValue,
                         size_t *ParamValueSizeRet) {
  return pi2ur::piEventGetInfo(Event,
                               ParamName,
                               ParamValueSize,
                               ParamValue,
                               ParamValueSizeRet);
}

__SYCL_EXPORT pi_result piEnqueueMemBufferMap(pi_queue Queue, pi_mem Mem, pi_bool BlockingMap,
                                pi_map_flags MapFlags, size_t Offset,
                                size_t Size, pi_uint32 NumEventsInWaitList,
                                const pi_event *EventWaitList,
                                pi_event *OutEvent, void **RetMap) {

  return pi2ur::piEnqueueMemBufferMap(Queue,
                                      Mem,
                                      BlockingMap,
                                      MapFlags,
                                      Offset,
                                      Size,
                                      NumEventsInWaitList,
                                      EventWaitList,
                                      OutEvent,
                                      RetMap);
}

__SYCL_EXPORT pi_result
piEnqueueMemBufferFill(pi_queue Queue, pi_mem Buffer,
                       const void *Pattern, size_t PatternSize,
                       size_t Offset, size_t Size,
                       pi_uint32 NumEventsInWaitList,
                       const pi_event *EventWaitList,
                       pi_event *Event) {
  return pi2ur::piEnqueueMemBufferFill(Queue,
                                       Buffer,
                                       Pattern,
                                       PatternSize,
                                       Offset,
                                       Size,
                                       NumEventsInWaitList,
                                       EventWaitList,
                                       Event);
}

__SYCL_EXPORT pi_result
piextUSMDeviceAlloc(void **ResultPtr,
                    pi_context Context,
                    pi_device Device,
                    pi_usm_mem_properties *Properties,
                    size_t Size,
                    pi_uint32 Alignment) {

  return pi2ur::piextUSMDeviceAlloc(ResultPtr,
                                    Context,
                                    Device,
                                    Properties,
                                    Size,
                                    Alignment);
}

__SYCL_EXPORT pi_result
piKernelRetain(pi_kernel Kernel) {
  return pi2ur::piKernelRetain(Kernel);
}

/// USM Memset API
///
/// @param Queue is the queue to submit to
/// @param Ptr is the ptr to memset
/// @param Value is value to set.  It is interpreted as an 8-bit value and the
/// upper
///        24 bits are ignored
/// @param Count is the size in bytes to memset
/// @param NumEventsInWaitlist is the number of events to wait on
/// @param EventsWaitlist is an array of events to wait on
/// @param Event is the event that represents this operation
__SYCL_EXPORT pi_result
piextUSMEnqueueMemset(pi_queue Queue, void *Ptr, pi_int32 Value,
                      size_t Count, pi_uint32 NumEventsInWaitlist,
                      const pi_event *EventsWaitlist,
                      pi_event *Event) {
  return pi2ur::piextUSMEnqueueMemset(Queue,
                                      Ptr,
                                      Value,
                                      Count,
                                      NumEventsInWaitlist,
                                      EventsWaitlist,
                                      Event);
}

__SYCL_EXPORT pi_result
piEnqueueMemBufferCopyRect(
    pi_queue Queue, pi_mem SrcMem, pi_mem DstMem, pi_buff_rect_offset SrcOrigin,
    pi_buff_rect_offset DstOrigin, pi_buff_rect_region Region,
    size_t SrcRowPitch, size_t SrcSlicePitch, size_t DstRowPitch,
    size_t DstSlicePitch, pi_uint32 NumEventsInWaitList,
    const pi_event *EventWaitList, pi_event *Event) {

  return pi2ur::piEnqueueMemBufferCopyRect(Queue,
                                           SrcMem,
                                           DstMem,
                                           SrcOrigin,
                                           DstOrigin,
                                           Region,
                                           SrcRowPitch,
                                           SrcSlicePitch,
                                           DstRowPitch,
                                           DstSlicePitch,
                                           NumEventsInWaitList,
                                           EventWaitList,
                                           Event);
}

__SYCL_EXPORT pi_result 
piEnqueueMemBufferCopy(pi_queue Queue, pi_mem SrcMem, pi_mem DstMem,
                                 size_t SrcOffset, size_t DstOffset,
                                 size_t Size, pi_uint32 NumEventsInWaitList,
                                 const pi_event *EventWaitList,
                                 pi_event *Event) {
  return pi2ur::piEnqueueMemBufferCopy(Queue,
                                       SrcMem,
                                       DstMem,
                                       SrcOffset,
                                       DstOffset,
                                       Size,
                                       NumEventsInWaitList,
                                       EventWaitList,
                                       Event);
}

__SYCL_EXPORT pi_result 
piextUSMEnqueueMemcpy(pi_queue Queue,
                      pi_bool Blocking,
                      void *DstPtr,
                      const void *SrcPtr,
                      size_t Size,
                      pi_uint32 NumEventsInWaitlist,
                      const pi_event *EventsWaitlist,
                      pi_event *Event) {

  return pi2ur::piextUSMEnqueueMemcpy(Queue,
                                      Blocking,
                                      DstPtr,
                                      SrcPtr,
                                      Size,
                                      NumEventsInWaitlist,
                                      EventsWaitlist,
                                      Event);
}

__SYCL_EXPORT pi_result
piEnqueueMemBufferWriteRect(
    pi_queue Queue, pi_mem Buffer, pi_bool BlockingWrite,
    pi_buff_rect_offset BufferOffset, pi_buff_rect_offset HostOffset,
    pi_buff_rect_region Region, size_t BufferRowPitch, size_t BufferSlicePitch,
    size_t HostRowPitch, size_t HostSlicePitch, const void *Ptr,
    pi_uint32 NumEventsInWaitList, const pi_event *EventWaitList,
    pi_event *Event) {

  return pi2ur::piEnqueueMemBufferWriteRect(Queue,
                                            Buffer,
                                            BlockingWrite,
                                            BufferOffset,
                                            HostOffset,
                                            Region,
                                            BufferRowPitch,
                                            BufferSlicePitch,
                                            HostRowPitch,
                                            HostSlicePitch,
                                            Ptr,
                                            NumEventsInWaitList,
                                            EventWaitList,
                                            Event);
}

__SYCL_EXPORT pi_result
piEnqueueMemBufferWrite(pi_queue Queue, pi_mem Buffer,
                        pi_bool BlockingWrite, size_t Offset,
                        size_t Size, const void *Ptr,
                        pi_uint32 NumEventsInWaitList,
                        const pi_event *EventWaitList,
                        pi_event *Event) {

  return pi2ur::piEnqueueMemBufferWrite(Queue,
                                        Buffer,
                                        BlockingWrite,
                                        Offset,
                                        Size,
                                        Ptr,
                                        NumEventsInWaitList,
                                        EventWaitList,
                                        Event);
}

__SYCL_EXPORT pi_result
piEnqueueMemBufferReadRect(
    pi_queue Queue, pi_mem Buffer, pi_bool BlockingRead,
    pi_buff_rect_offset BufferOffset, pi_buff_rect_offset HostOffset,
    pi_buff_rect_region Region, size_t BufferRowPitch, size_t BufferSlicePitch,
    size_t HostRowPitch, size_t HostSlicePitch, void *Ptr,
    pi_uint32 NumEventsInWaitList, const pi_event *EventWaitList,
    pi_event *Event) {

  return pi2ur::piEnqueueMemBufferReadRect(Queue,
                                           Buffer,
                                           BlockingRead,
                                           BufferOffset,
                                           HostOffset,
                                           Region,
                                           BufferRowPitch,
                                           BufferSlicePitch,
                                           HostRowPitch,
                                           HostSlicePitch,
                                           Ptr,
                                           NumEventsInWaitList,
                                           EventWaitList,
                                           Event);
}

__SYCL_EXPORT pi_result
piEnqueueMemBufferRead(pi_queue Queue, pi_mem Src,
                        pi_bool BlockingRead, size_t Offset,
                        size_t Size, void *Dst,
                        pi_uint32 NumEventsInWaitList,
                        const pi_event *EventWaitList,
                        pi_event *Event) {
  
  return pi2ur::piEnqueueMemBufferRead(Queue,
                                       Src,
                                       BlockingRead,
                                       Offset,
                                       Size,
                                       Dst,
                                       NumEventsInWaitList,
                                       EventWaitList,
                                       Event);
}

__SYCL_EXPORT pi_result
piEnqueueEventsWaitWithBarrier(pi_queue Queue,
                               pi_uint32 NumEventsInWaitList,
                               const pi_event *EventWaitList,
                               pi_event *OutEvent) {
  
  return pi2ur::piEnqueueEventsWaitWithBarrier(Queue,
                                               NumEventsInWaitList,
                                               EventWaitList,
                                               OutEvent);
}

__SYCL_EXPORT pi_result
piEnqueueEventsWait(pi_queue Queue,
                    pi_uint32 NumEventsInWaitList,
                    const pi_event *EventWaitList,
                    pi_event *OutEvent) {

  return pi2ur::piEnqueueEventsWait(Queue,
                                    NumEventsInWaitList,
                                    EventWaitList,
                                    OutEvent);
}

__SYCL_EXPORT pi_result
piextEventGetNativeHandle(pi_event Event,
                          pi_native_handle *NativeHandle) {

  return pi2ur::piextEventGetNativeHandle(Event,
                                          NativeHandle);
}

__SYCL_EXPORT pi_result
piEventGetProfilingInfo(pi_event Event,
                        pi_profiling_info ParamName,
                        size_t ParamValueSize,
                        void *ParamValue,
                        size_t *ParamValueSizeRet) {

  return pi2ur::piEventGetProfilingInfo(Event,
                                        ParamName,
                                        ParamValueSize,
                                        ParamValue,
                                        ParamValueSizeRet);
}

__SYCL_EXPORT pi_result
piProgramRetain(pi_program Program) {
  return pi2ur::piProgramRetain(Program);
}

__SYCL_EXPORT pi_result
piKernelSetExecInfo(pi_kernel Kernel,
                    pi_kernel_exec_info ParamName,
                    size_t ParamValueSize,
                    const void *ParamValue) {

  return pi2ur::piKernelSetExecInfo(Kernel,
                                    ParamName,
                                    ParamValueSize,
                                    ParamValue);
}

__SYCL_EXPORT pi_result
piKernelGetInfo(pi_kernel Kernel,
                pi_kernel_info ParamName,
                size_t ParamValueSize,
                void *ParamValue,
                size_t *ParamValueSizeRet) {
  return pi2ur::piKernelGetInfo(Kernel,
                                ParamName,
                                ParamValueSize,
                                ParamValue,
                                ParamValueSizeRet);
}

__SYCL_EXPORT pi_result piGetDeviceAndHostTimer(pi_device Device, uint64_t *DeviceTime,
                                  uint64_t *HostTime) {
  return pi2ur::piGetDeviceAndHostTimer(Device, DeviceTime, HostTime);
}

// This interface is not in Unified Runtime currently
__SYCL_EXPORT pi_result piTearDown(void *) { return PI_SUCCESS; }

// This interface is not in Unified Runtime currently
__SYCL_EXPORT pi_result piPluginInit(pi_plugin *PluginInit) {
  PI_ASSERT(PluginInit, PI_ERROR_INVALID_VALUE);

  const char SupportedVersion[] = _PI_UNIFIED_RUNTIME_PLUGIN_VERSION_STRING;

  // Check that the major version matches in PiVersion and SupportedVersion
  _PI_PLUGIN_VERSION_CHECK(PluginInit->PiVersion, SupportedVersion);

  // TODO: handle versioning/targets properly.
  size_t PluginVersionSize = sizeof(PluginInit->PluginVersion);

  PI_ASSERT(strlen(_PI_UNIFIED_RUNTIME_PLUGIN_VERSION_STRING) <
                PluginVersionSize,
            PI_ERROR_INVALID_VALUE);

  strncpy(PluginInit->PluginVersion, SupportedVersion, PluginVersionSize);

  // Bind interfaces that are already supported and "die" for unsupported ones
#define _PI_API(api)                                                           \
  (PluginInit->PiFunctionTable).api = (decltype(&::api))(&DieUnsupported);
#include <sycl/detail/pi.def>

#define _PI_API(api)                                                           \
  (PluginInit->PiFunctionTable).api = (decltype(&::api))(&api);

  _PI_API(piPlatformsGet)
  _PI_API(piPlatformGetInfo)
  _PI_API(piDevicesGet)
  _PI_API(piDeviceRetain)
  _PI_API(piDeviceRelease)
  _PI_API(piDeviceGetInfo)
  _PI_API(piDevicePartition)
  _PI_API(piextDeviceSelectBinary)
  _PI_API(piGetDeviceAndHostTimer)

  _PI_API(piContextCreate)
  _PI_API(piContextRelease)

  _PI_API(piQueueCreate)
  _PI_API(piQueueRelease)
  _PI_API(piextQueueCreate)
  _PI_API(piQueueFinish)

  _PI_API(piProgramCreate)
  _PI_API(piProgramBuild)
  _PI_API(piextProgramSetSpecializationConstant)
  _PI_API(piProgramLink)
  _PI_API(piKernelCreate)
  _PI_API(piextKernelSetArgMemObj)
  _PI_API(piextKernelCreateWithNativeHandle)
  _PI_API(piProgramRetain)
  _PI_API(piKernelSetExecInfo)
  _PI_API(piKernelGetInfo)
  _PI_API(piKernelSetArg)
  _PI_API(piKernelGetGroupInfo)
  _PI_API(piKernelRetain)

  _PI_API(piMemBufferCreate)
  _PI_API(piextUSMHostAlloc)
  _PI_API(piMemGetInfo)
  _PI_API(piMemBufferPartition)
  _PI_API(piEnqueueMemImageCopy)
  _PI_API(piextMemGetNativeHandle)
  _PI_API(piextMemCreateWithNativeHandle)
  _PI_API(piextUSMDeviceAlloc)

  _PI_API(piEnqueueKernelLaunch)
  _PI_API(piEnqueueMemImageWrite)
  _PI_API(piEnqueueMemImageRead)
  _PI_API(piEnqueueMemBufferMap)
  _PI_API(piEnqueueMemUnmap)
  _PI_API(piEnqueueMemBufferFill)
  _PI_API(piextUSMEnqueueMemset)
  _PI_API(piEnqueueMemBufferCopyRect)
  _PI_API(piEnqueueMemBufferCopy)
  _PI_API(piextUSMEnqueueMemcpy)
  _PI_API(piEnqueueMemBufferWriteRect)
  _PI_API(piEnqueueMemBufferWrite)
  _PI_API(piEnqueueMemBufferReadRect)
  _PI_API(piEnqueueMemBufferRead)
  _PI_API(piEnqueueEventsWaitWithBarrier)
  _PI_API(piEnqueueEventsWait)

  _PI_API(piEventsWait)
  _PI_API(piEventGetInfo)
  _PI_API(piextEventGetNativeHandle)
  _PI_API(piEventGetProfilingInfo)

  _PI_API(piTearDown)

  return PI_SUCCESS;
}

} // extern "C
