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
#include "ur_level_zero_event.hpp"
#include "ur_level_zero_context.hpp"
#include <ur_bindings.hpp>

// Default to using compute engine for fill operation, but allow to
// override this with an environment variable.
static bool PreferCopyEngine = [] {
  const char *Env = std::getenv("SYCL_PI_LEVEL_ZERO_USE_COPY_ENGINE_FOR_FILL");
  return Env ? std::stoi(Env) != 0 : false;
}();

// This is an experimental option to test performance of device to device copy
// operations on copy engines (versus compute engine)
static const bool UseCopyEngineForD2DCopy = [] {
  const char *CopyEngineForD2DCopy =
      std::getenv("SYCL_PI_LEVEL_ZERO_USE_COPY_ENGINE_FOR_D2D_COPY");
  return (CopyEngineForD2DCopy && (std::stoi(CopyEngineForD2DCopy) != 0));
}();

// Helper function to check if a pointer is a device pointer.
static bool IsDevicePointer(ur_context_handle_t Context, const void *Ptr) {
  ze_device_handle_t ZeDeviceHandle;
  ZeStruct<ze_memory_allocation_properties_t> ZeMemoryAllocationProperties;

  // Query memory type of the pointer
  ZE2UR_CALL(zeMemGetAllocProperties, (Context->ZeContext,
                                       Ptr,
                                      &ZeMemoryAllocationProperties,
                                       &ZeDeviceHandle));

  return (ZeMemoryAllocationProperties.type == ZE_MEMORY_TYPE_DEVICE);
}


// Shared by all memory read/write/copy PI interfaces.
// PI interfaces must have queue's and destination buffer's mutexes locked for
// exclusive use and source buffer's mutex locked for shared use on entry.
static ur_result_t
enqueueMemCopyHelper(pi_command_type CommandType,
                     ur_queue_handle_t Queue,
                     void *Dst,
                     pi_bool BlockingWrite,
                     size_t Size,
                     const void *Src,
                     uint32_t NumEventsInWaitList,
                     const ur_event_handle_t *EventWaitList,
                     ur_event_handle_t *OutEvent,
                     bool PreferCopyEngine) {
#if 0
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);
#endif
  bool UseCopyEngine = Queue->useCopyEngine(PreferCopyEngine);

  _pi_ze_event_list_t TmpWaitList;
  UR_CALL(TmpWaitList.createAndRetainPiZeEventList(NumEventsInWaitList,
                                                   EventWaitList,
                                                   Queue,
                                                   UseCopyEngine));

  // We want to batch these commands to avoid extra submissions (costly)
  bool OkToBatch = true;

  // Get a new command list to be used on this call
  pi_command_list_ptr_t CommandList{};
  UR_CALL(Queue->Context->getAvailableCommandList(Queue,
                                                  CommandList,
                                                  UseCopyEngine,
                                                  OkToBatch));

  ze_event_handle_t ZeEvent = nullptr;
  ur_event_handle_t InternalEvent;
  bool IsInternal = OutEvent == nullptr;
  ur_event_handle_t *Event = OutEvent ? OutEvent : &InternalEvent;
  UR_CALL(createEventAndAssociateQueue(Queue,
                                       Event,
                                       CommandType,
                                       CommandList,
                                       IsInternal));
  ZeEvent = (*Event)->ZeEvent;
  (*Event)->WaitList = TmpWaitList;

  const auto &ZeCommandList = CommandList->first;
  const auto &WaitList = (*Event)->WaitList;

  zePrint("calling zeCommandListAppendMemoryCopy() with\n"
          "  ZeEvent %#llx\n",
          ur_cast<std::uintptr_t>(ZeEvent));
  printZeEventList(WaitList);

  ZE2UR_CALL(zeCommandListAppendMemoryCopy, (ZeCommandList,
                                             Dst,
                                             Src,
                                             Size,
                                             ZeEvent,
                                             WaitList.Length,
                                             WaitList.ZeEventList));

  UR_CALL(Queue->executeCommandList(CommandList, BlockingWrite, OkToBatch));

  return UR_RESULT_SUCCESS;
}

// Shared by all memory read/write/copy rect PI interfaces.
// PI interfaces must have queue's and destination buffer's mutexes locked for
// exclusive use and source buffer's mutex locked for shared use on entry.
ur_result_t enqueueMemCopyRectHelper(pi_command_type CommandType,
                                     ur_queue_handle_t Queue,
                                     const void *SrcBuffer,
                                     void *DstBuffer, 
                                     ur_rect_offset_t SrcOrigin,
                                     ur_rect_offset_t DstOrigin,
                                     ur_rect_region_t Region,
                                     size_t SrcRowPitch,
                                     size_t DstRowPitch,
                                     size_t SrcSlicePitch,
                                     size_t DstSlicePitch,
                                     pi_bool Blocking,
                                     uint32_t NumEventsInWaitList,
                                     const ur_event_handle_t *EventWaitList,
                                     ur_event_handle_t *OutEvent,
                                     bool PreferCopyEngine) {
#if 0
  PI_ASSERT(Region && SrcOrigin && DstOrigin && Queue, PI_ERROR_INVALID_VALUE);
#endif
  bool UseCopyEngine = Queue->useCopyEngine(PreferCopyEngine);

  _pi_ze_event_list_t TmpWaitList;
  UR_CALL(TmpWaitList.createAndRetainPiZeEventList(NumEventsInWaitList,
                                                   EventWaitList,
                                                   Queue,
                                                   UseCopyEngine));

  // We want to batch these commands to avoid extra submissions (costly)
  bool OkToBatch = true;

  // Get a new command list to be used on this call
  pi_command_list_ptr_t CommandList{};
  UR_CALL(Queue->Context->getAvailableCommandList(Queue,
                                                  CommandList,
                                                  UseCopyEngine,
                                                  OkToBatch));

  ze_event_handle_t ZeEvent = nullptr;
  ur_event_handle_t InternalEvent;
  bool IsInternal = OutEvent == nullptr;
  ur_event_handle_t *Event = OutEvent ? OutEvent : &InternalEvent;
  UR_CALL(createEventAndAssociateQueue(reinterpret_cast<ur_queue_handle_t>(Queue),
                                                      Event,
                                                      CommandType,
                                                      CommandList,
                                                      IsInternal));

  ZeEvent = (*Event)->ZeEvent;
  (*Event)->WaitList = TmpWaitList;

  const auto &ZeCommandList = CommandList->first;
  const auto &WaitList = (*Event)->WaitList;

  zePrint("calling zeCommandListAppendMemoryCopy() with\n"
          "  ZeEvent %#llx\n",
          ur_cast<std::uintptr_t>(ZeEvent));
  printZeEventList(WaitList);

  uint32_t SrcOriginX = ur_cast<uint32_t>(SrcOrigin.x);
  uint32_t SrcOriginY = ur_cast<uint32_t>(SrcOrigin.y);
  uint32_t SrcOriginZ = ur_cast<uint32_t>(SrcOrigin.z);

  uint32_t SrcPitch = SrcRowPitch;
  if (SrcPitch == 0)
    SrcPitch = ur_cast<uint32_t>(Region.width);

  if (SrcSlicePitch == 0)
    SrcSlicePitch = ur_cast<uint32_t>(Region.height) * SrcPitch;

  uint32_t DstOriginX = ur_cast<uint32_t>(DstOrigin.x);
  uint32_t DstOriginY = ur_cast<uint32_t>(DstOrigin.y);
  uint32_t DstOriginZ = ur_cast<uint32_t>(DstOrigin.z);

  uint32_t DstPitch = DstRowPitch;
  if (DstPitch == 0)
    DstPitch = ur_cast<uint32_t>(Region.width);

  if (DstSlicePitch == 0)
    DstSlicePitch = ur_cast<uint32_t>(Region.height) * DstPitch;

  uint32_t Width = ur_cast<uint32_t>(Region.width);
  uint32_t Height = ur_cast<uint32_t>(Region.height);
  uint32_t Depth = ur_cast<uint32_t>(Region.depth);

  const ze_copy_region_t ZeSrcRegion = {SrcOriginX, SrcOriginY, SrcOriginZ,
                                        Width,      Height,     Depth};
  const ze_copy_region_t ZeDstRegion = {DstOriginX, DstOriginY, DstOriginZ,
                                        Width,      Height,     Depth};

  ZE2UR_CALL(zeCommandListAppendMemoryCopyRegion, (ZeCommandList,
                                                   DstBuffer,
                                                   &ZeDstRegion,
                                                   DstPitch,
                                                   DstSlicePitch,
                                                   SrcBuffer,
                                                   &ZeSrcRegion,
                                                   SrcPitch,
                                                   SrcSlicePitch,
                                                   nullptr,
                                                   WaitList.Length,
                                                   WaitList.ZeEventList));

  zePrint("calling zeCommandListAppendMemoryCopyRegion()\n");

  ZE2UR_CALL(zeCommandListAppendBarrier, (ZeCommandList,
                                          ZeEvent,
                                          0,
                                          nullptr));

  zePrint("calling zeCommandListAppendBarrier() with Event %#llx\n",
          ur_cast<std::uintptr_t>(ZeEvent));

  UR_CALL(Queue->executeCommandList(CommandList, Blocking, OkToBatch));

  return UR_RESULT_SUCCESS;
}

// PI interfaces must have queue's and buffer's mutexes locked on entry.
static ur_result_t
enqueueMemFillHelper(pi_command_type CommandType,
                     ur_queue_handle_t Queue,
                     void *Ptr,
                     const void *Pattern,
                     size_t PatternSize,
                     size_t Size,
                     uint32_t NumEventsInWaitList,
                     const ur_event_handle_t *EventWaitList,
                     ur_event_handle_t *OutEvent) {
#if 0 
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);
  // Pattern size must be a power of two.
  PI_ASSERT((PatternSize > 0) && ((PatternSize & (PatternSize - 1)) == 0),
            PI_ERROR_INVALID_VALUE);
#endif
  _ur_queue_handle_t *UrQueue = ur_cast<_ur_queue_handle_t *>(Queue);
  auto &Device = UrQueue->Device;

  // Make sure that pattern size matches the capability of the copy queues.
  // Check both main and link groups as we don't known which one will be used.
  //
  if (PreferCopyEngine && Device->hasCopyEngine()) {
    if (Device->hasMainCopyEngine() &&
        Device->QueueGroup[_ur_device_handle_t::queue_group_info_t::MainCopy]
                .ZeProperties.maxMemoryFillPatternSize < PatternSize) {
      PreferCopyEngine = false;
    }
    if (Device->hasLinkCopyEngine() &&
        Device->QueueGroup[_ur_device_handle_t::queue_group_info_t::LinkCopy]
                .ZeProperties.maxMemoryFillPatternSize < PatternSize) {
      PreferCopyEngine = false;
    }
  }

  bool UseCopyEngine = UrQueue->useCopyEngine(PreferCopyEngine);
  if (!UseCopyEngine) {
#if 0
    // Pattern size must fit the compute queue capabilities.
    PI_ASSERT(PatternSize <=
                  Device->QueueGroup[_pi_device::queue_group_info_t::Compute]
                      .ZeProperties.maxMemoryFillPatternSize,
              PI_ERROR_INVALID_VALUE);
#endif
  }

  _pi_ze_event_list_t TmpWaitList;
  UR_CALL(TmpWaitList.createAndRetainPiZeEventList(NumEventsInWaitList,
                                                   EventWaitList,
                                                   Queue,
                                                   UseCopyEngine));

  pi_command_list_ptr_t CommandList{};
  // We want to batch these commands to avoid extra submissions (costly)
  bool OkToBatch = true;
  UR_CALL(Queue->Context->getAvailableCommandList(Queue,
                                                  CommandList,
                                                  UseCopyEngine,
                                                  OkToBatch));

  ze_event_handle_t ZeEvent = nullptr;
  ur_event_handle_t InternalEvent;
  bool IsInternal = OutEvent == nullptr;
  ur_event_handle_t *Event = OutEvent ? OutEvent : &InternalEvent;
  UR_CALL(createEventAndAssociateQueue(Queue,
                                       Event,
                                       CommandType,
                                       CommandList,
                                       IsInternal));

  ZeEvent = (*Event)->ZeEvent;
  (*Event)->WaitList = TmpWaitList;

  const auto &ZeCommandList = CommandList->first;
  const auto &WaitList = (*Event)->WaitList;

  ZE2UR_CALL(zeCommandListAppendMemoryFill, (ZeCommandList,
                                             Ptr,
                                             Pattern,
                                             PatternSize,
                                             Size,
                                             ZeEvent,
                                             WaitList.Length,
                                             WaitList.ZeEventList));

  zePrint("calling zeCommandListAppendMemoryFill() with\n"
          "  ZeEvent %#llx\n",
          ur_cast<uint64_t>(ZeEvent));
  printZeEventList(WaitList);

  // Execute command list asynchronously, as the event will be used
  // to track down its completion.
  UR_CALL(UrQueue->executeCommandList(CommandList, false, OkToBatch));

  return UR_RESULT_SUCCESS;
}


// If indirect access tracking is enabled then performs reference counting,
// otherwise just calls zeMemAllocHost.
static ur_result_t ZeHostMemAllocHelper(void **ResultPtr, ur_context_handle_t UrContext,
                                      size_t Size) {
  ur_platform_handle_t Plt = UrContext->getPlatform();
  std::unique_lock<pi_shared_mutex> ContextsLock(Plt->ContextsMutex,
                                                 std::defer_lock);
  if (IndirectAccessTrackingEnabled) {
    // Lock the mutex which is guarding contexts container in the platform.
    // This prevents new kernels from being submitted in any context while
    // we are in the process of allocating a memory, this is needed to
    // properly capture allocations by kernels with indirect access.
    ContextsLock.lock();
    // We are going to defer memory release if there are kernels with
    // indirect access, that is why explicitly retain context to be sure
    // that it is released after all memory allocations in this context are
    // released.
#if 0
    UR_CALL(piContextRetain(UrContext));
#endif
  }

  ZeStruct<ze_host_mem_alloc_desc_t> ZeDesc;
  ZeDesc.flags = 0;
  ZE2UR_CALL(zeMemAllocHost, (UrContext->ZeContext, &ZeDesc, Size, 1, ResultPtr));

  if (IndirectAccessTrackingEnabled) {
    // Keep track of all memory allocations in the context
    UrContext->MemAllocs.emplace(std::piecewise_construct,
                               std::forward_as_tuple(*ResultPtr),
                               std::forward_as_tuple(reinterpret_cast<ur_context_handle_t>(UrContext)));
  }
  return UR_RESULT_SUCCESS;
}

static ur_result_t getImageRegionHelper(_ur_image *Mem,
                                        ur_rect_offset_t *Origin,
                                        ur_rect_region_t *Region,
                                        ze_image_region_t &ZeRegion) {
#if 0
  PI_ASSERT(Mem, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Origin, PI_ERROR_INVALID_VALUE);
#endif

#ifndef NDEBUG
#if 0
  PI_ASSERT(Mem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
#endif

#if 0
  auto UrImage = static_cast<_ur_image *>(Mem);
  ze_image_desc_t &ZeImageDesc = UrImage->ZeImageDesc;

  PI_ASSERT((ZeImageDesc.type == ZE_IMAGE_TYPE_1D && Origin->y == 0 &&
             Origin->z == 0) ||
                (ZeImageDesc.type == ZE_IMAGE_TYPE_1DARRAY && Origin->z == 0) ||
                (ZeImageDesc.type == ZE_IMAGE_TYPE_2D && Origin->z == 0) ||
                (ZeImageDesc.type == ZE_IMAGE_TYPE_3D),
            PI_ERROR_INVALID_VALUE);

  PI_ASSERT(Region->width && Region->height && Region->depth,
            PI_ERROR_INVALID_VALUE);
  PI_ASSERT(
      (ZeImageDesc.type == ZE_IMAGE_TYPE_1D && Region->height == 1 &&
       Region->depth == 1) ||
          (ZeImageDesc.type == ZE_IMAGE_TYPE_1DARRAY && Region->depth == 1) ||
          (ZeImageDesc.type == ZE_IMAGE_TYPE_2D && Region->depth == 1) ||
          (ZeImageDesc.type == ZE_IMAGE_TYPE_3D),
      PI_ERROR_INVALID_VALUE);
#endif
#endif // !NDEBUG

  uint32_t OriginX = ur_cast<uint32_t>(Origin->x);
  uint32_t OriginY = ur_cast<uint32_t>(Origin->y);
  uint32_t OriginZ = ur_cast<uint32_t>(Origin->z);

  uint32_t Width = ur_cast<uint32_t>(Region->width);
  uint32_t Height = ur_cast<uint32_t>(Region->height);
  uint32_t Depth = ur_cast<uint32_t>(Region->depth);

  ZeRegion = {OriginX, OriginY, OriginZ, Width, Height, Depth};

  return UR_RESULT_SUCCESS;
}

// Helper function to implement image read/write/copy.
// PI interfaces must have queue's and destination image's mutexes locked for
// exclusive use and source image's mutex locked for shared use on entry.
static ur_result_t enqueueMemImageCommandHelper(pi_command_type CommandType,
                                                _ur_queue_handle_t *UrQueue,
                                                const void *Src, // image or ptr
                                                void *Dst,       // image or ptr
                                                pi_bool IsBlocking,
                                                ur_rect_offset_t *SrcOrigin,
                                                ur_rect_offset_t *DstOrigin,
                                                ur_rect_region_t *Region,
                                                size_t RowPitch,
                                                size_t SlicePitch,
                                                pi_uint32 NumEventsInWaitList,
                                                const ur_event_handle_t *EventWaitList,
                                                ur_event_handle_t *OutEvent,
                                                bool PreferCopyEngine = false) {
#if 0
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);
#endif
  bool UseCopyEngine = UrQueue->useCopyEngine(PreferCopyEngine);

  _pi_ze_event_list_t TmpWaitList;
  if (auto Res = TmpWaitList.createAndRetainPiZeEventList(NumEventsInWaitList,
                                                          reinterpret_cast<const ur_event_handle_t *>(EventWaitList),
                                                          reinterpret_cast<ur_queue_handle_t>(UrQueue),
                                                          UseCopyEngine))
    return Res;

  // We want to batch these commands to avoid extra submissions (costly)
  bool OkToBatch = true;

  // Get a new command list to be used on this call
  pi_command_list_ptr_t CommandList{};
  if (auto Res = UrQueue->Context->getAvailableCommandList(reinterpret_cast<ur_queue_handle_t>(UrQueue),
                                                            CommandList,
                                                            UseCopyEngine,
                                                            OkToBatch))
    return Res;

  ze_event_handle_t ZeEvent = nullptr;
  ur_event_handle_t InternalEvent;
  bool IsInternal = OutEvent == nullptr;
  ur_event_handle_t *Event = OutEvent ? OutEvent : &InternalEvent;
  auto Res =  createEventAndAssociateQueue(reinterpret_cast<ur_queue_handle_t>(UrQueue),
                                           Event,
                                           CommandType,
                                           CommandList,
                                           IsInternal);
  if (Res != UR_RESULT_SUCCESS)
    return Res;
  ZeEvent = (*Event)->ZeEvent;
  (*Event)->WaitList = TmpWaitList;

  const auto &ZeCommandList = CommandList->first;
  const auto &WaitList = (*Event)->WaitList;

  if (CommandType == PI_COMMAND_TYPE_IMAGE_READ) {
    _ur_image *SrcMem = ur_cast<_ur_image *>(const_cast<void *>(Src));

    ze_image_region_t ZeSrcRegion;
    auto Result = getImageRegionHelper(SrcMem, SrcOrigin, Region, ZeSrcRegion);
    if (Result != UR_RESULT_SUCCESS)
      return Result;

    // TODO: Level Zero does not support row_pitch/slice_pitch for images yet.
    // Check that SYCL RT did not want pitch larger than default.
    (void)RowPitch;
    (void)SlicePitch;
#ifndef NDEBUG
#if 0
    PI_ASSERT(SrcMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);

    auto SrcImage = SrcMem;
    const ze_image_desc_t &ZeImageDesc = SrcImage->ZeImageDesc;
    PI_ASSERT(
        RowPitch == 0 ||
            // special case RGBA image pitch equal to region's width
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32 &&
             RowPitch == 4 * 4 * ZeSrcRegion.width) ||
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16 &&
             RowPitch == 4 * 2 * ZeSrcRegion.width) ||
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8 &&
             RowPitch == 4 * ZeSrcRegion.width),
        PI_ERROR_INVALID_IMAGE_SIZE);
    PI_ASSERT(SlicePitch == 0 || SlicePitch == RowPitch * ZeSrcRegion.height,
              PI_ERROR_INVALID_IMAGE_SIZE);
#endif
#endif // !NDEBUG

    char *ZeHandleSrc = nullptr;
    UR_CALL(
        SrcMem->getZeHandle(ZeHandleSrc, _ur_mem_handle_t::read_only, UrQueue->Device));
    ZE2UR_CALL(zeCommandListAppendImageCopyToMemory, (ZeCommandList,
                                                      Dst,
                                                      ur_cast<ze_image_handle_t>(ZeHandleSrc),
                                                      &ZeSrcRegion,
                                                      ZeEvent,
                                                      WaitList.Length,
                                                      WaitList.ZeEventList));
  } else if (CommandType == PI_COMMAND_TYPE_IMAGE_WRITE) {
    _ur_image *DstMem = ur_cast<_ur_image *>(Dst);
    ze_image_region_t ZeDstRegion;
    auto Result = getImageRegionHelper(DstMem, DstOrigin, Region, ZeDstRegion);
    if (Result != UR_RESULT_SUCCESS)
      return Result;

      // TODO: Level Zero does not support row_pitch/slice_pitch for images yet.
      // Check that SYCL RT did not want pitch larger than default.
#if 0
#ifndef NDEBUG
    PI_ASSERT(DstMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);

    auto DstImage = static_cast<_pi_image *>(DstMem);
    const ze_image_desc_t &ZeImageDesc = DstImage->ZeImageDesc;
    PI_ASSERT(
        RowPitch == 0 ||
            // special case RGBA image pitch equal to region's width
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32 &&
             RowPitch == 4 * 4 * ZeDstRegion.width) ||
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16 &&
             RowPitch == 4 * 2 * ZeDstRegion.width) ||
            (ZeImageDesc.format.layout == ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8 &&
             RowPitch == 4 * ZeDstRegion.width),
        PI_ERROR_INVALID_IMAGE_SIZE);
    PI_ASSERT(SlicePitch == 0 || SlicePitch == RowPitch * ZeDstRegion.height,
              PI_ERROR_INVALID_IMAGE_SIZE);
#endif // !NDEBUG
#endif

    char *ZeHandleDst = nullptr;
    UR_CALL(DstMem->getZeHandle(ZeHandleDst,
                                _ur_mem_handle_t::write_only,
                                UrQueue->Device));
    ZE2UR_CALL(zeCommandListAppendImageCopyFromMemory, (ZeCommandList,
                                                        ur_cast<ze_image_handle_t>(ZeHandleDst),
                                                        Src,
                                                        &ZeDstRegion,
                                                        ZeEvent,
                                                        WaitList.Length,
                                                        WaitList.ZeEventList));
  } else if (CommandType == PI_COMMAND_TYPE_IMAGE_COPY) {
    _ur_image * SrcImage = ur_cast<_ur_image *>(const_cast<void *>(Src));
    _ur_image * DstImage = ur_cast<_ur_image *>(Dst);

    ze_image_region_t ZeSrcRegion;
    auto Result = getImageRegionHelper(SrcImage,
                                       SrcOrigin,
                                       Region,
                                       ZeSrcRegion);
    if (Result != UR_RESULT_SUCCESS)
      return Result;
    ze_image_region_t ZeDstRegion;
    Result = getImageRegionHelper(DstImage,
                                  DstOrigin,
                                  Region,
                                  ZeDstRegion);
    if (Result != UR_RESULT_SUCCESS)
      return Result;

    char *ZeHandleSrc = nullptr;
    char *ZeHandleDst = nullptr;
    UR_CALL(SrcImage->getZeHandle(ZeHandleSrc,
                                  _ur_mem_handle_t::read_only,
                                  UrQueue->Device));
    UR_CALL(DstImage->getZeHandle(ZeHandleDst,
                                  _ur_mem_handle_t::write_only,
                                  UrQueue->Device));
    ZE2UR_CALL(zeCommandListAppendImageCopyRegion, (ZeCommandList,
                                                    ur_cast<ze_image_handle_t>(ZeHandleDst),
                                                    ur_cast<ze_image_handle_t>(ZeHandleSrc),
                                                    &ZeDstRegion,
                                                    &ZeSrcRegion,
                                                    ZeEvent,
                                                    0,
                                                    nullptr));
  } else {
    zePrint("enqueueMemImageUpdate: unsupported image command type\n");
    return UR_RESULT_ERROR_INVALID_OPERATION;
  }

  if (auto Res = UrQueue->executeCommandList(CommandList,
                                             IsBlocking,
                                             OkToBatch))
    return Res;

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferRead(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_mem_handle_t hBuffer,  ///< [in] handle of the buffer object
    bool blockingRead, ///< [in] indicates blocking (true), non-blocking (false)
    size_t offset,     ///< [in] offset in bytes in the buffer object
    size_t size,       ///< [in] size in bytes of data being read
    void *pDst, ///< [in] pointer to host memory where data is to be read into
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  _ur_queue_handle_t *Queue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _ur_mem_handle_t *Src = ur_cast<_ur_mem_handle_t *>(hBuffer);

  std::shared_lock<pi_shared_mutex> SrcLock(Src->Mutex, std::defer_lock);
  std::scoped_lock<std::shared_lock<pi_shared_mutex>, pi_shared_mutex> LockAll(
      SrcLock, Queue->Mutex);

  char *ZeHandleSrc = nullptr;
  UR_CALL(Src->getZeHandle(ZeHandleSrc,
                          _ur_mem_handle_t::read_only,
                          Queue->Device));
  return enqueueMemCopyHelper(PI_COMMAND_TYPE_MEM_BUFFER_READ,
                              hQueue,
                              pDst,
                              blockingRead,
                              size, 
                              ZeHandleSrc + offset,
                              numEventsInWaitList,
                              phEventWaitList,
                              phEvent,
                              true /* PreferCopyEngine */);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferWrite(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_mem_handle_t hBuffer,  ///< [in] handle of the buffer object
    bool
        blockingWrite, ///< [in] indicates blocking (true), non-blocking (false)
    size_t offset,     ///< [in] offset in bytes in the buffer object
    size_t size,       ///< [in] size in bytes of data being written
    const void
        *pSrc, ///< [in] pointer to host memory where data is to be written from
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  _ur_queue_handle_t *Queue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _ur_mem_handle_t *Buffer = ur_cast<_ur_mem_handle_t *>(hBuffer);

  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(Queue->Mutex,
                                                          Buffer->Mutex);

  char *ZeHandleDst = nullptr;
  UR_CALL(Buffer->getZeHandle(ZeHandleDst,
                              _ur_mem_handle_t::write_only,
                              Queue->Device));
  return enqueueMemCopyHelper(PI_COMMAND_TYPE_MEM_BUFFER_WRITE,
                              hQueue,
                              ZeHandleDst + offset, // dst
                              blockingWrite,
                              size,
                              pSrc, // src
                              numEventsInWaitList,
                              phEventWaitList,
                              phEvent,
                              true /* PreferCopyEngine */);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferReadRect(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_mem_handle_t hBuffer,  ///< [in] handle of the buffer object
    bool blockingRead, ///< [in] indicates blocking (true), non-blocking (false)
    ur_rect_offset_t bufferOffset, ///< [in] 3D offset in the buffer
    ur_rect_offset_t hostOffset,   ///< [in] 3D offset in the host region
    ur_rect_region_t
        region, ///< [in] 3D rectangular region descriptor: width, height, depth
    size_t bufferRowPitch,   ///< [in] length of each row in bytes in the buffer
                             ///< object
    size_t bufferSlicePitch, ///< [in] length of each 2D slice in bytes in the
                             ///< buffer object being read
    size_t hostRowPitch,     ///< [in] length of each row in bytes in the host
                             ///< memory region pointed by dst
    size_t hostSlicePitch,   ///< [in] length of each 2D slice in bytes in the
                             ///< host memory region pointed by dst
    void *pDst, ///< [in] pointer to host memory where data is to be read into
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  _ur_queue_handle_t *Queue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _ur_mem_handle_t *Buffer = ur_cast<_ur_mem_handle_t *>(hBuffer);

  std::shared_lock<pi_shared_mutex> SrcLock(Buffer->Mutex, std::defer_lock);
  std::scoped_lock<std::shared_lock<pi_shared_mutex>, pi_shared_mutex> LockAll(
      SrcLock, Queue->Mutex);

  char *ZeHandleSrc;
  UR_CALL(Buffer->getZeHandle(ZeHandleSrc,
                              _ur_mem_handle_t::read_only,
                              Queue->Device));
  return enqueueMemCopyRectHelper(PI_COMMAND_TYPE_MEM_BUFFER_READ_RECT,
                                  hQueue,
                                  ZeHandleSrc,
                                  pDst,
                                  bufferOffset,
                                  hostOffset,
                                  region,
                                  bufferRowPitch,
                                  hostRowPitch,
                                  bufferSlicePitch,
                                  hostSlicePitch,
                                  blockingRead,
                                  numEventsInWaitList,
                                  phEventWaitList,
                                  phEvent);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferWriteRect(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_mem_handle_t hBuffer,  ///< [in] handle of the buffer object
    bool
        blockingWrite, ///< [in] indicates blocking (true), non-blocking (false)
    ur_rect_offset_t bufferOffset, ///< [in] 3D offset in the buffer
    ur_rect_offset_t hostOffset,   ///< [in] 3D offset in the host region
    ur_rect_region_t
        region, ///< [in] 3D rectangular region descriptor: width, height, depth
    size_t bufferRowPitch,   ///< [in] length of each row in bytes in the buffer
                             ///< object
    size_t bufferSlicePitch, ///< [in] length of each 2D slice in bytes in the
                             ///< buffer object being written
    size_t hostRowPitch,     ///< [in] length of each row in bytes in the host
                             ///< memory region pointed by src
    size_t hostSlicePitch,   ///< [in] length of each 2D slice in bytes in the
                             ///< host memory region pointed by src
    void
        *pSrc, ///< [in] pointer to host memory where data is to be written from
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< points to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  _ur_queue_handle_t *Queue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _ur_mem_handle_t *Buffer = ur_cast<_ur_mem_handle_t *>(hBuffer);

  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(Queue->Mutex,
                                                          Buffer->Mutex);

  char *ZeHandleDst = nullptr;
  UR_CALL(Buffer->getZeHandle(ZeHandleDst,
                              _ur_mem_handle_t::write_only,
                              Queue->Device));
  return enqueueMemCopyRectHelper(PI_COMMAND_TYPE_MEM_BUFFER_WRITE_RECT,
                                  hQueue,
                                  const_cast<char *>(static_cast<const char *>(pSrc)),
                                  ZeHandleDst,
                                  hostOffset,
                                  bufferOffset,
                                  region,
                                  hostRowPitch,
                                  bufferRowPitch,
                                  hostSlicePitch,
                                  bufferSlicePitch,
                                  blockingWrite,
                                  numEventsInWaitList,
                                  phEventWaitList,
                                  phEvent);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferCopy(
    ur_queue_handle_t hQueue,   ///< [in] handle of the queue object
    ur_mem_handle_t hBufferSrc, ///< [in] handle of the src buffer object
    ur_mem_handle_t hBufferDst, ///< [in] handle of the dest buffer object
    size_t srcOffset, ///< [in] offset into hBufferSrc to begin copying from
    size_t dstOffset, ///< [in] offset info hBufferDst to begin copying into
    size_t size,      ///< [in] size in bytes of data being copied
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
#if 0
  PI_ASSERT(!SrcMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(!DstMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
#endif
  _ur_queue_handle_t *Queue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _pi_buffer *SrcBuffer = ur_cast<_pi_buffer *>(hBufferSrc);
  _pi_buffer *DstBuffer = ur_cast<_pi_buffer *>(hBufferDst);

  std::shared_lock<pi_shared_mutex> SrcLock(SrcBuffer->Mutex, std::defer_lock);
  std::scoped_lock<std::shared_lock<pi_shared_mutex>, pi_shared_mutex,
                   pi_shared_mutex>
      LockAll(SrcLock, DstBuffer->Mutex, Queue->Mutex);

  // Copy engine is preferred only for host to device transfer.
  // Device to device transfers run faster on compute engines.
  bool PreferCopyEngine = (SrcBuffer->OnHost || DstBuffer->OnHost);

  // Temporary option added to use copy engine for D2D copy
  PreferCopyEngine |= UseCopyEngineForD2DCopy;

  char *ZeHandleSrc = nullptr;
  UR_CALL(SrcBuffer->getZeHandle(ZeHandleSrc,
                                 _ur_mem_handle_t::read_only,
                                 Queue->Device));
  char *ZeHandleDst = nullptr;
  UR_CALL(DstBuffer->getZeHandle(ZeHandleDst,
                                 _ur_mem_handle_t::write_only,
                                 Queue->Device));

  return enqueueMemCopyHelper(PI_COMMAND_TYPE_MEM_BUFFER_COPY,
                              hQueue,
                              ZeHandleDst + dstOffset,
                              false, // blocking
                              size,
                              ZeHandleSrc + srcOffset,
                              numEventsInWaitList,
                              phEventWaitList,
                              phEvent,
                              PreferCopyEngine);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferCopyRect(
    ur_queue_handle_t hQueue,   ///< [in] handle of the queue object
    ur_mem_handle_t hBufferSrc, ///< [in] handle of the source buffer object
    ur_mem_handle_t hBufferDst, ///< [in] handle of the dest buffer object
    ur_rect_offset_t srcOrigin, ///< [in] 3D offset in the source buffer
    ur_rect_offset_t dstOrigin, ///< [in] 3D offset in the destination buffer
    ur_rect_region_t srcRegion, ///< [in] source 3D rectangular region
                                ///< descriptor: width, height, depth
    size_t srcRowPitch,   ///< [in] length of each row in bytes in the source
                          ///< buffer object
    size_t srcSlicePitch, ///< [in] length of each 2D slice in bytes in the
                          ///< source buffer object
    size_t dstRowPitch, ///< [in] length of each row in bytes in the destination
                        ///< buffer object
    size_t dstSlicePitch, ///< [in] length of each 2D slice in bytes in the
                          ///< destination buffer object
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
#if 0 
  PI_ASSERT(!SrcMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(!DstMem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
#endif
  _ur_queue_handle_t *Queue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _pi_buffer *SrcBuffer = ur_cast<_pi_buffer *>(hBufferSrc);
  _pi_buffer *DstBuffer = ur_cast<_pi_buffer *>(hBufferDst);
  
  std::shared_lock<pi_shared_mutex> SrcLock(SrcBuffer->Mutex, std::defer_lock);
  std::scoped_lock<std::shared_lock<pi_shared_mutex>, pi_shared_mutex,
                   pi_shared_mutex>
      LockAll(SrcLock, DstBuffer->Mutex, Queue->Mutex);

  // Copy engine is preferred only for host to device transfer.
  // Device to device transfers run faster on compute engines.
  bool PreferCopyEngine = (SrcBuffer->OnHost || DstBuffer->OnHost);

  char *ZeHandleSrc = nullptr;
  UR_CALL(SrcBuffer->getZeHandle(ZeHandleSrc,
                                 _ur_mem_handle_t::read_only,
                                 Queue->Device));
  char *ZeHandleDst = nullptr;
  UR_CALL(DstBuffer->getZeHandle(ZeHandleDst,
                                 _ur_mem_handle_t::write_only,
                                 Queue->Device));

  return enqueueMemCopyRectHelper(PI_COMMAND_TYPE_MEM_BUFFER_COPY_RECT,
                                  hQueue,
                                  ZeHandleSrc,
                                  ZeHandleDst,
                                  srcOrigin,
                                  dstOrigin,
                                  srcRegion,
                                  srcRowPitch,
                                  dstRowPitch,
                                  srcSlicePitch,
                                  dstSlicePitch,
                                  false, // blocking
                                  numEventsInWaitList,
                                  phEventWaitList,
                                  phEvent,
                                  PreferCopyEngine);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferFill(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_mem_handle_t hBuffer,  ///< [in] handle of the buffer object
    const void *pPattern,     ///< [in] pointer to the fill pattern
    size_t patternSize,       ///< [in] size in bytes of the pattern
    size_t offset,            ///< [in] offset into the buffer
    size_t size, ///< [in] fill size in bytes, must be a multiple of patternSize
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {

  _ur_queue_handle_t *UrQueue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _ur_mem_handle_t *UrBuffer = ur_cast<_ur_mem_handle_t *>(hBuffer);
  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(UrQueue->Mutex,
                                                          UrBuffer->Mutex);

  char *ZeHandleDst = nullptr;
  UR_CALL(UrBuffer->getZeHandle(ZeHandleDst,
                                _ur_mem_handle_t::write_only,
                                UrQueue->Device));
  return enqueueMemFillHelper(PI_COMMAND_TYPE_MEM_BUFFER_FILL,
                              hQueue,
                              ZeHandleDst + offset,
                              pPattern,
                              patternSize,
                              size,
                              numEventsInWaitList,
                              phEventWaitList,
                              phEvent);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemImageRead(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_mem_handle_t hImage,   ///< [in] handle of the image object
    bool blockingRead, ///< [in] indicates blocking (true), non-blocking (false)
    ur_rect_offset_t origin, ///< [in] defines the (x,y,z) offset in pixels in
                             ///< the 1D, 2D, or 3D image
    ur_rect_region_t region, ///< [in] defines the (width, height, depth) in
                             ///< pixels of the 1D, 2D, or 3D image
    size_t rowPitch,         ///< [in] length of each row in bytes
    size_t slicePitch,       ///< [in] length of each 2D slice of the 3D image
    void *pDst, ///< [in] pointer to host memory where image is to be read into
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  _ur_queue_handle_t *UrQueue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _ur_mem_handle_t *UrImage = ur_cast<_ur_mem_handle_t *>(hImage);
  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(UrQueue->Mutex,
                                                          UrImage->Mutex);
  return enqueueMemImageCommandHelper(PI_COMMAND_TYPE_IMAGE_READ,
                                      UrQueue,
                                      pDst,
                                      UrImage,
                                      blockingRead,
                                      nullptr,
                                      &origin,
                                      &region,
                                      rowPitch,
                                      slicePitch,
                                      numEventsInWaitList,
                                      phEventWaitList,
                                      phEvent);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemImageWrite(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_mem_handle_t hImage,   ///< [in] handle of the image object
    bool
        blockingWrite, ///< [in] indicates blocking (true), non-blocking (false)
    ur_rect_offset_t origin, ///< [in] defines the (x,y,z) offset in pixels in
                             ///< the 1D, 2D, or 3D image
    ur_rect_region_t region, ///< [in] defines the (width, height, depth) in
                             ///< pixels of the 1D, 2D, or 3D image
    size_t rowPitch,    ///< [in] length of each row in bytes
    size_t slicePitch,  ///< [in] length of each 2D slice of the 3D image
    void *pSrc, ///< [in] pointer to host memory where image is to be read into
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  _ur_queue_handle_t *UrQueue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _ur_mem_handle_t *UrImage = ur_cast<_ur_mem_handle_t *>(hImage);
  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(UrQueue->Mutex,
                                                          UrImage->Mutex);
  return enqueueMemImageCommandHelper(PI_COMMAND_TYPE_IMAGE_WRITE,
                                      UrQueue,
                                      pSrc,
                                      UrImage,
                                      blockingWrite,
                                      nullptr,
                                      &origin,
                                      &region,
                                      rowPitch,
                                      slicePitch,
                                      numEventsInWaitList,
                                      phEventWaitList,
                                      phEvent);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemImageCopy(
    ur_queue_handle_t hQueue,   ///< [in] handle of the queue object
    ur_mem_handle_t hImageSrc,  ///< [in] handle of the src image object
    ur_mem_handle_t hImageDst,  ///< [in] handle of the dest image object
    ur_rect_offset_t srcOrigin, ///< [in] defines the (x,y,z) offset in pixels
                                ///< in the source 1D, 2D, or 3D image
    ur_rect_offset_t dstOrigin, ///< [in] defines the (x,y,z) offset in pixels
                                ///< in the destination 1D, 2D, or 3D image
    ur_rect_region_t region,    ///< [in] defines the (width, height, depth) in
                                ///< pixels of the 1D, 2D, or 3D image
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  _ur_queue_handle_t *UrQueue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _ur_mem_handle_t *UrSrcImage = ur_cast<_ur_mem_handle_t *>(hImageSrc);
  _ur_mem_handle_t *UrDstImage = ur_cast<_ur_mem_handle_t *>(hImageDst);
  std::shared_lock<pi_shared_mutex> SrcLock(UrSrcImage->Mutex, std::defer_lock);
  std::scoped_lock<std::shared_lock<pi_shared_mutex>, pi_shared_mutex,
                   pi_shared_mutex>
      LockAll(SrcLock, UrDstImage->Mutex, UrQueue->Mutex);
  // Copy engine is preferred only for host to device transfer.
  // Device to device transfers run faster on compute engines.
  // Images are always allocated on device.
  bool PreferCopyEngine = false;
  return enqueueMemImageCommandHelper(PI_COMMAND_TYPE_IMAGE_COPY,
                                      UrQueue,
                                      UrSrcImage,
                                      UrDstImage,
                                      false, // is_blocking
                                      &srcOrigin,
                                      &dstOrigin,
                                      &region,
                                      0, // row pitch
                                      0, // slice pitch
                                      numEventsInWaitList,
                                      phEventWaitList,
                                      phEvent,
                                      PreferCopyEngine);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferMap(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_mem_handle_t hBuffer,  ///< [in] handle of the buffer object
    bool blockingMap, ///< [in] indicates blocking (true), non-blocking (false)
    ur_map_flags_t mapFlags, ///< [in] flags for read, write, readwrite mapping
    size_t offset, ///< [in] offset in bytes of the buffer region being mapped
    size_t size,   ///< [in] size in bytes of the buffer region being mapped
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent, ///< [in,out][optional] return an event object that identifies
                  ///< this particular command instance.
    void **ppRetMap ///< [in,out] return mapped pointer.  TODO: move it before
                    ///< numEventsInWaitList?
) {

#if 0
  PI_ASSERT(!Mem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
#endif
  _ur_queue_handle_t *UrQueue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _ur_mem_handle_t *UrMem = ur_cast<_ur_mem_handle_t *>(hBuffer);
  auto Buffer = ur_cast<_pi_buffer *>(UrMem);

  ur_event_handle_t InternalEvent;
  bool IsInternal = phEvent == nullptr;
  ur_event_handle_t *Event = phEvent ? phEvent : &InternalEvent;
  ze_event_handle_t ZeEvent = nullptr;

  bool UseCopyEngine = false;
  {
    // Lock automatically releases when this goes out of scope.
    std::scoped_lock<pi_shared_mutex> lock(UrQueue->Mutex);

    _pi_ze_event_list_t TmpWaitList;
    UR_CALL(TmpWaitList.createAndRetainPiZeEventList(numEventsInWaitList,
                                                     phEventWaitList,
                                                     hQueue,
                                                     UseCopyEngine));

    UR_CALL(createEventAndAssociateQueue(hQueue,
                                         Event,
                                         PI_COMMAND_TYPE_MEM_BUFFER_MAP,
                                         UrQueue->CommandListMap.end(),
                                         IsInternal));

    ZeEvent = (*Event)->ZeEvent;
    (*Event)->WaitList = TmpWaitList;
  }

  // Translate the host access mode info.
  _ur_mem_handle_t::access_mode_t AccessMode = _ur_mem_handle_t::unknown;
  if (mapFlags & UR_EXT_MAP_FLAG_WRITE_INVALIDATE_REGION)
    AccessMode = _ur_mem_handle_t::write_only;
  else {
    if (mapFlags & UR_MAP_FLAG_READ) {
      AccessMode = _ur_mem_handle_t::read_only;
      if (mapFlags & UR_MAP_FLAG_WRITE)
        AccessMode = _ur_mem_handle_t::read_write;
    } else if (mapFlags & UR_MAP_FLAG_WRITE)
      AccessMode = _ur_mem_handle_t::write_only;
  }
#if 0
  PI_ASSERT(AccessMode != _pi_mem::unknown, PI_ERROR_INVALID_VALUE);
#endif

  // TODO: Level Zero is missing the memory "mapping" capabilities, so we are
  // left to doing new memory allocation and a copy (read) on discrete devices.
  // For integrated devices, we have allocated the buffer in host memory so no
  // actions are needed here except for synchronizing on incoming events.
  // A host-to-host copy is done if a host pointer had been supplied during
  // buffer creation on integrated devices.
  //
  // TODO: for discrete, check if the input buffer is already allocated
  // in shared memory and thus is accessible from the host as is.
  // Can we get SYCL RT to predict/allocate in shared memory
  // from the beginning?

  // For integrated devices the buffer has been allocated in host memory.
  if (Buffer->OnHost) {
    // Wait on incoming events before doing the copy
    if (numEventsInWaitList > 0)
      UR_CALL(urEventWait(numEventsInWaitList, phEventWaitList));

    if (UrQueue->isInOrderQueue())
      UR_CALL(urQueueFinish(hQueue));

    // Lock automatically releases when this goes out of scope.
    std::scoped_lock<pi_shared_mutex> Guard(Buffer->Mutex);

    char *ZeHandleSrc;
    UR_CALL(Buffer->getZeHandle(ZeHandleSrc, AccessMode, UrQueue->Device));

    if (Buffer->MapHostPtr) {
      *ppRetMap = Buffer->MapHostPtr + offset;
      if (ZeHandleSrc != Buffer->MapHostPtr &&
          AccessMode != _ur_mem_handle_t::write_only) {
        memcpy(*ppRetMap, ZeHandleSrc + offset, size);
      }
    } else {
      *ppRetMap = ZeHandleSrc + offset;
    }

    auto Res = Buffer->Mappings.insert({*ppRetMap, {offset, size}});
    // False as the second value in pair means that mapping was not inserted
    // because mapping already exists.
    if (!Res.second) {
      zePrint("urEnqueueMemBufferMap: duplicate mapping detected\n");
      return UR_RESULT_ERROR_INVALID_VALUE;
    }

    // Signal this event
    ZE2UR_CALL(zeEventHostSignal, (ZeEvent));
    (*Event)->Completed = true;
    return UR_RESULT_SUCCESS;
  }

  // Lock automatically releases when this goes out of scope.
  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(UrQueue->Mutex,
                                                          Buffer->Mutex);

  if (Buffer->MapHostPtr) {
    *ppRetMap = Buffer->MapHostPtr + offset;
  } else {
    // TODO: use USM host allocator here
    // TODO: Do we even need every map to allocate new host memory?
    //       In the case when the buffer is "OnHost" we use single allocation.
    UR_CALL(ZeHostMemAllocHelper(ppRetMap, UrQueue->Context, size));
  }

  // Take a shortcut if the host is not going to read buffer's data.
  if (AccessMode == _ur_mem_handle_t::write_only) {
    (*Event)->Completed = true;
  } else {
    // For discrete devices we need a command list
    pi_command_list_ptr_t CommandList{};
    UR_CALL(UrQueue->Context->getAvailableCommandList(hQueue,
                                                      CommandList,
                                                      UseCopyEngine));

    // Add the event to the command list.
    CommandList->second.append(reinterpret_cast<ur_event_handle_t>(*Event));
    (*Event)->RefCount.increment();

    const auto &ZeCommandList = CommandList->first;
    const auto &WaitList = (*Event)->WaitList;

    char *ZeHandleSrc;
    UR_CALL(Buffer->getZeHandle(ZeHandleSrc,
                                AccessMode,
                                UrQueue->Device));

    ZE2UR_CALL(zeCommandListAppendMemoryCopy, (ZeCommandList,
                                               *ppRetMap,
                                               ZeHandleSrc + offset,
                                               size,
                                               ZeEvent,
                                               WaitList.Length,
                                               WaitList.ZeEventList));

    UR_CALL(UrQueue->executeCommandList(CommandList, blockingMap));
  }

  auto Res = Buffer->Mappings.insert({*ppRetMap, {offset, size}});
  // False as the second value in pair means that mapping was not inserted
  // because mapping already exists.
  if (!Res.second) {
    zePrint("urEnqueueMemBufferMap: duplicate mapping detected\n");
    return UR_RESULT_ERROR_INVALID_VALUE;
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemUnmap(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_mem_handle_t
        hMem,         ///< [in] handle of the memory (buffer or image) object
    void *pMappedPtr, ///< [in] mapped host address
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
#if 0
  PI_ASSERT(!Mem->isImage(), PI_ERROR_INVALID_MEM_OBJECT);
#endif
  _ur_queue_handle_t *UrQueue = ur_cast<_ur_queue_handle_t *>(hQueue);
  _ur_mem_handle_t *UrMem = ur_cast<_ur_mem_handle_t *>(hMem);
  auto Buffer = ur_cast<_pi_buffer *>(UrMem);

  bool UseCopyEngine = false;

  ze_event_handle_t ZeEvent = nullptr;
  ur_event_handle_t InternalEvent;
  bool IsInternal = phEvent == nullptr;
  ur_event_handle_t *Event = phEvent ? phEvent : &InternalEvent;
  {
    // Lock automatically releases when this goes out of scope.
    std::scoped_lock<pi_shared_mutex> lock(UrQueue->Mutex);

    _pi_ze_event_list_t TmpWaitList;
    UR_CALL(TmpWaitList.createAndRetainPiZeEventList(numEventsInWaitList,
                                                     phEventWaitList,
                                                     hQueue,
                                                     UseCopyEngine));

    UR_CALL(createEventAndAssociateQueue(hQueue,
                                         Event,
                                         PI_COMMAND_TYPE_MEM_BUFFER_UNMAP,
                                         UrQueue->CommandListMap.end(),
                                         IsInternal));
    ZeEvent = (*Event)->ZeEvent;
    (*Event)->WaitList = TmpWaitList;
  }

  _pi_buffer::Mapping MapInfo = {};
  {
    // Lock automatically releases when this goes out of scope.
    std::scoped_lock<pi_shared_mutex> Guard(Buffer->Mutex);
    auto It = Buffer->Mappings.find(pMappedPtr);
    if (It == Buffer->Mappings.end()) {
      zePrint("urEnqueueMemUnmap: unknown memory mapping\n");
      return UR_RESULT_ERROR_INVALID_VALUE;
    }
    MapInfo = It->second;
    Buffer->Mappings.erase(It);

    // NOTE: we still have to free the host memory allocated/returned by
    // piEnqueueMemBufferMap, but can only do so after the above copy
    // is completed. Instead of waiting for It here (blocking), we shall
    // do so in piEventRelease called for the pi_event tracking the unmap.
    // In the case of an integrated device, the map operation does not allocate
    // any memory, so there is nothing to free. This is indicated by a nullptr.
    (*Event)->CommandData =
        (Buffer->OnHost ? nullptr : (Buffer->MapHostPtr ? nullptr : pMappedPtr));
  }

  // For integrated devices the buffer is allocated in host memory.
  if (Buffer->OnHost) {
    // Wait on incoming events before doing the copy
    if (numEventsInWaitList > 0)
      UR_CALL(urEventWait(numEventsInWaitList, phEventWaitList));

    if (UrQueue->isInOrderQueue())
      UR_CALL(urQueueFinish(reinterpret_cast<ur_queue_handle_t>(UrQueue)));

    char *ZeHandleDst;
    UR_CALL(Buffer->getZeHandle(ZeHandleDst,
                                _ur_mem_handle_t::write_only,
                                UrQueue->Device));

    std::scoped_lock<pi_shared_mutex> Guard(Buffer->Mutex);
    if (Buffer->MapHostPtr)
      memcpy(ZeHandleDst + MapInfo.Offset, pMappedPtr, MapInfo.Size);

    // Signal this event
    ZE2UR_CALL(zeEventHostSignal, (ZeEvent));
    (*Event)->Completed = true;
    return UR_RESULT_SUCCESS;
  }

  // Lock automatically releases when this goes out of scope.
  std::scoped_lock<pi_shared_mutex, pi_shared_mutex> Lock(UrQueue->Mutex,
                                                          Buffer->Mutex);

  pi_command_list_ptr_t CommandList{};
  UR_CALL(UrQueue->Context->getAvailableCommandList(reinterpret_cast<ur_queue_handle_t>(UrQueue),
                                                    CommandList,
                                                    UseCopyEngine));

  CommandList->second.append(reinterpret_cast<ur_event_handle_t>(*Event));
  (*Event)->RefCount.increment();

  const auto &ZeCommandList = CommandList->first;

  // TODO: Level Zero is missing the memory "mapping" capabilities, so we are
  // left to doing copy (write back to the device).
  //
  // NOTE: Keep this in sync with the implementation of
  // piEnqueueMemBufferMap.

  char *ZeHandleDst;
  UR_CALL(Buffer->getZeHandle(ZeHandleDst,
                              _ur_mem_handle_t::write_only,
                              UrQueue->Device));

  ZE2UR_CALL(zeCommandListAppendMemoryCopy, (ZeCommandList,
                                             ZeHandleDst + MapInfo.Offset,
                                             pMappedPtr,
                                             MapInfo.Size,
                                             ZeEvent,
                                             (*Event)->WaitList.Length,
                                             (*Event)->WaitList.ZeEventList));

  // Execute command list asynchronously, as the event will be used
  // to track down its completion.
  UR_CALL(UrQueue->executeCommandList(CommandList));

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMMemset(
    ur_queue_handle_t hQueue,     ///< [in] handle of the queue object
    void *ptr,                    ///< [in] pointer to USM memory object
    int8_t byteValue,             ///< [in] byte value to fill
    size_t count,                 ///< [in] size in bytes to be set
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMMemcpy(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    bool blocking,            ///< [in] blocking or non-blocking copy
    void *pDst,       ///< [in] pointer to the destination USM memory object
    const void *pSrc, ///< [in] pointer to the source USM memory object
    size_t size,      ///< [in] size in bytes to be copied
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  _ur_queue_handle_t *Queue = ur_cast<_ur_queue_handle_t *>(hQueue);
  std::scoped_lock<pi_shared_mutex> lock(Queue->Mutex);

  // Device to Device copies are found to execute slower on copy engine
  // (versus compute engine).
  bool PreferCopyEngine = !IsDevicePointer(Queue->Context, pSrc) ||
                          !IsDevicePointer(Queue->Context, pDst);

  // Temporary option added to use copy engine for D2D copy
  PreferCopyEngine |= UseCopyEngineForD2DCopy;

  return enqueueMemCopyHelper(// TODO: do we need a new command type for this?
                              PI_COMMAND_TYPE_MEM_BUFFER_COPY,
                              hQueue,
                              pDst,
                              blocking,
                              size,
                              pSrc,
                              numEventsInWaitList,
                              phEventWaitList,
                              phEvent,
                              PreferCopyEngine);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMPrefetch(
    ur_queue_handle_t hQueue,       ///< [in] handle of the queue object
    const void *pMem,               ///< [in] pointer to the USM memory object
    size_t size,                    ///< [in] size in bytes to be fetched
    ur_usm_migration_flags_t flags, ///< [in] USM prefetch flags
    uint32_t numEventsInWaitList,   ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before this command can be executed. If nullptr,
                          ///< the numEventsInWaitList must be 0, indicating
                          ///< that this command does not wait on any event to
                          ///< complete.
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMMemAdvise(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    const void *pMem,         ///< [in] pointer to the USM memory object
    size_t size,              ///< [in] size in bytes to be advised
    ur_mem_advice_t advice,   ///< [in] USM memory advice
    ur_event_handle_t
        *phEvent ///< [in,out][optional] return an event object that identifies
                 ///< this particular command instance.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMFill2D(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue to submit to.
    void *pMem,               ///< [in] pointer to memory to be filled.
    size_t pitch, ///< [in] the total width of the destination memory including
                  ///< padding.
    size_t patternSize, ///< [in] the size in bytes of the pattern.
    const void
        *pPattern, ///< [in] pointer with the bytes of the pattern to set.
    size_t width,  ///< [in] the width in bytes of each row to fill.
    size_t height, ///< [in] the height of the columns to fill.
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

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMMemset2D(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue to submit to.
    void *pMem,               ///< [in] pointer to memory to be filled.
    size_t pitch,  ///< [in] the total width of the destination memory including
                   ///< padding.
    int value,     ///< [in] the value to fill into the region in pMem.
    size_t width,  ///< [in] the width in bytes of each row to set.
    size_t height, ///< [in] the height of the columns to set.
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

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMMemcpy2D(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue to submit to.
    bool blocking, ///< [in] indicates if this operation should block the host.
    void *pDst,    ///< [in] pointer to memory where data will be copied.
    size_t dstPitch,  ///< [in] the total width of the source memory including
                      ///< padding.
    const void *pSrc, ///< [in] pointer to memory to be copied.
    size_t srcPitch,  ///< [in] the total width of the source memory including
                      ///< padding.
    size_t width,     ///< [in] the width in bytes of each row to be copied.
    size_t height,    ///< [in] the height of columns to be copied.
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

UR_APIEXPORT ur_result_t UR_APICALL urMemImageCreate(
    ur_context_handle_t hContext, ///< [in] handle of the context object
    ur_mem_flags_t flags, ///< [in] allocation and usage information flags
    const ur_image_format_t
        *pImageFormat, ///< [in] pointer to image format specification
    const ur_image_desc_t *pImageDesc, ///< [in] pointer to image description
    void *pHost,                       ///< [in] pointer to the buffer data
    ur_mem_handle_t *phMem ///< [out] pointer to handle of image object created
) {

  _ur_context_handle_t *UrContext = reinterpret_cast<_ur_context_handle_t *>(hContext);

  ze_image_format_type_t ZeImageFormatType;
  size_t ZeImageFormatTypeSize;
  switch (pImageFormat->channelType) {
  case UR_IMAGE_CHANNEL_TYPE_FLOAT:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_FLOAT;
    ZeImageFormatTypeSize = 32;
    break;
  case UR_IMAGE_CHANNEL_TYPE_HALF_FLOAT:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_FLOAT;
    ZeImageFormatTypeSize = 16;
    break;
  case UR_IMAGE_CHANNEL_TYPE_UNSIGNED_INT32:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_UINT;
    ZeImageFormatTypeSize = 32;
    break;
  case UR_IMAGE_CHANNEL_TYPE_UNSIGNED_INT16:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_UINT;
    ZeImageFormatTypeSize = 16;
    break;
  case UR_IMAGE_CHANNEL_TYPE_UNSIGNED_INT8:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_UINT;
    ZeImageFormatTypeSize = 8;
    break;
  case UR_IMAGE_CHANNEL_TYPE_UNORM_INT16:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_UNORM;
    ZeImageFormatTypeSize = 16;
    break;
  case UR_IMAGE_CHANNEL_TYPE_UNORM_INT8:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_UNORM;
    ZeImageFormatTypeSize = 8;
    break;
  case UR_IMAGE_CHANNEL_TYPE_SIGNED_INT32:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_SINT;
    ZeImageFormatTypeSize = 32;
    break;
  case UR_IMAGE_CHANNEL_TYPE_SIGNED_INT16:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_SINT;
    ZeImageFormatTypeSize = 16;
    break;
  case UR_IMAGE_CHANNEL_TYPE_SIGNED_INT8:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_SINT;
    ZeImageFormatTypeSize = 8;
    break;
  case UR_IMAGE_CHANNEL_TYPE_SNORM_INT16:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_SNORM;
    ZeImageFormatTypeSize = 16;
    break;
  case UR_IMAGE_CHANNEL_TYPE_SNORM_INT8:
    ZeImageFormatType = ZE_IMAGE_FORMAT_TYPE_SNORM;
    ZeImageFormatTypeSize = 8;
    break;
  default:
    zePrint("piMemImageCreate: unsupported image data type: data type = %d\n",
            pImageFormat->channelType);
    return UR_RESULT_ERROR_INVALID_VALUE;
  }

  // TODO: populate the layout mapping
  ze_image_format_layout_t ZeImageFormatLayout;
  switch (pImageFormat->channelOrder) {
  case UR_IMAGE_CHANNEL_ORDER_RGBA:
    switch (ZeImageFormatTypeSize) {
    case 8:
      ZeImageFormatLayout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
      break;
    case 16:
      ZeImageFormatLayout = ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16;
      break;
    case 32:
      ZeImageFormatLayout = ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32;
      break;
    default:
      zePrint("piMemImageCreate: unexpected data type Size\n");
      return UR_RESULT_ERROR_INVALID_VALUE;
    }
    break;
  default:
    zePrint("format layout = %d\n", pImageFormat->channelOrder);
    die("piMemImageCreate: unsupported image format layout\n");
    break;
  }

  ze_image_format_t ZeFormatDesc = {
      ZeImageFormatLayout, ZeImageFormatType,
      // TODO: are swizzles deducted from image_format->image_channel_order?
      ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
      ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A};

  ze_image_type_t ZeImageType;
  switch (pImageDesc->type) {
  case UR_MEM_TYPE_IMAGE1D:
    ZeImageType = ZE_IMAGE_TYPE_1D;
    break;
  case UR_MEM_TYPE_IMAGE2D:
    ZeImageType = ZE_IMAGE_TYPE_2D;
    break;
  case UR_MEM_TYPE_IMAGE3D:
    ZeImageType = ZE_IMAGE_TYPE_3D;
    break;
  case UR_MEM_TYPE_IMAGE1D_ARRAY:
    ZeImageType = ZE_IMAGE_TYPE_1DARRAY;
    break;
  case UR_MEM_TYPE_IMAGE2D_ARRAY:
    ZeImageType = ZE_IMAGE_TYPE_2DARRAY;
    break;
  default:
    zePrint("piMemImageCreate: unsupported image type\n");
    return UR_RESULT_ERROR_INVALID_VALUE;
  }

  ZeStruct<ze_image_desc_t> ZeImageDesc;
  ZeImageDesc.arraylevels = ZeImageDesc.flags = 0;
  ZeImageDesc.type = ZeImageType;
  ZeImageDesc.format = ZeFormatDesc;
  ZeImageDesc.width = ur_cast<uint64_t>(pImageDesc->width);
  ZeImageDesc.height = ur_cast<uint64_t>(pImageDesc->height);
  ZeImageDesc.depth = ur_cast<uint64_t>(pImageDesc->depth);
  ZeImageDesc.arraylevels = ur_cast<uint32_t>(pImageDesc->arraySize);
  ZeImageDesc.miplevels = pImageDesc->numMipLevel;

  std::shared_lock<pi_shared_mutex> Lock(UrContext->Mutex);

  // Currently we have the "0" device in context with mutliple root devices to
  // own the image.
  // TODO: Implement explicit copying for acessing the image from other devices
  // in the context.
  _ur_device_handle_t *Device = UrContext->SingleRootDevice ? UrContext->SingleRootDevice
                                               : UrContext->Devices[0];
  ze_image_handle_t ZeImage;
  ZE2UR_CALL(zeImageCreate, (UrContext->ZeContext,
                          Device->ZeDevice,
                          &ZeImageDesc,
                          &ZeImage));

  try {
    auto UrImage = new _ur_image(ur_cast<ur_context_handle_t>(UrContext), ZeImage);
    *phMem = reinterpret_cast<ur_mem_handle_t>(UrImage);

#ifndef NDEBUG
    UrImage->ZeImageDesc = ZeImageDesc;
#endif // !NDEBUG

    if ((flags & UR_MEM_FLAG_USE_HOST_POINTER) != 0 ||
        (flags & UR_MEM_FLAG_ALLOC_COPY_HOST_POINTER) != 0) {
      // Initialize image synchronously with immediate offload.
      // zeCommandListAppendImageCopyFromMemory must not be called from
      // simultaneous threads with the same command list handle, so we need
      // exclusive lock.
      std::scoped_lock<pi_mutex> Lock(UrContext->ImmediateCommandListMutex);
      ZE2UR_CALL(zeCommandListAppendImageCopyFromMemory, (UrContext->ZeCommandListInit,
                                                          ZeImage,
                                                          pHost,
                                                          nullptr,
                                                          nullptr,
               0, nullptr));
    }
  } catch (const std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemBufferCreate(
    ur_context_handle_t hContext, ///< [in] handle of the context object
    ur_mem_flags_t flags, ///< [in] allocation and usage information flags
    size_t size, ///< [in] size in bytes of the memory object to be allocated
    void *pHost, ///< [in][optional] pointer to the buffer data
    ur_mem_handle_t
        *phBuffer ///< [out] pointer to handle of the memory buffer created
) {
#if 0
  // TODO: implement support for more access modes
  if (!((flags & PI_MEM_FLAGS_ACCESS_RW) ||
        (flags & PI_MEM_ACCESS_READ_ONLY))) {
    die("piMemBufferCreate: Level-Zero supports read-write and read-only "
        "buffer,"
        "but not other accesses (such as write-only) yet.");
  }

  if (properties != nullptr) {
    die("piMemBufferCreate: no mem properties goes to Level-Zero RT yet");
  }

#endif

  _ur_context_handle_t *UrContext = reinterpret_cast<_ur_context_handle_t *>(hContext);

  if (flags & UR_MEM_FLAG_ALLOC_HOST_POINTER) {
    // Having PI_MEM_FLAGS_HOST_PTR_ALLOC for buffer requires allocation of
    // pinned host memory, see:
    // sycl/doc/extensions/supported/sycl_ext_oneapi_use_pinned_host_memory_property.asciidoc
    // We are however missing such functionality in Level Zero, so we just
    // ignore the flag for now.
    //
  }

  // If USM Import feature is enabled and hostptr is supplied,
  // import the hostptr if not already imported into USM.
  // Data transfer rate is maximized when both source and destination
  // are USM pointers. Promotion of the host pointer to USM thus
  // optimizes data transfer performance.
  bool HostPtrImported = false;
  if (ZeUSMImport.Enabled && pHost != nullptr &&
      (flags & UR_MEM_FLAG_USE_HOST_POINTER) != 0) {
    // Query memory type of the host pointer
    ze_device_handle_t ZeDeviceHandle;
    ZeStruct<ze_memory_allocation_properties_t> ZeMemoryAllocationProperties;
    ZE2UR_CALL(zeMemGetAllocProperties,
            (UrContext->ZeContext, pHost, &ZeMemoryAllocationProperties,
             &ZeDeviceHandle));

    // If not shared of any type, we can import the ptr
    if (ZeMemoryAllocationProperties.type == ZE_MEMORY_TYPE_UNKNOWN) {
      // Promote the host ptr to USM host memory
      ze_driver_handle_t driverHandle = UrContext->getPlatform()->ZeDriver;
      ZeUSMImport.doZeUSMImport(driverHandle, pHost, size);
      HostPtrImported = true;
    }
  }

  _pi_buffer *Buffer = nullptr;
  auto HostPtrOrNull =
      (flags & UR_MEM_FLAG_USE_HOST_POINTER) ? reinterpret_cast<char *>(pHost) : nullptr;
  try {
    Buffer = new _pi_buffer(hContext, size, HostPtrOrNull, HostPtrImported);
  } catch (const std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  // Initialize the buffer with user data
  if (pHost) {
    if ((flags & UR_MEM_FLAG_USE_HOST_POINTER) != 0 ||
        (flags & UR_MEM_FLAG_ALLOC_COPY_HOST_POINTER) != 0) {

      // We don't yet know which device needs this buffer, so make the first
      // device in the context be the master, and hold the initial valid
      // allocation.
      char *ZeHandleDst;
      Buffer->getZeHandle(ZeHandleDst, _ur_mem_handle_t::write_only,
                          UrContext->Devices[0]);
      if (Buffer->OnHost) {
        // Do a host to host copy.
        // For an imported HostPtr the copy is unneeded.
        if (!HostPtrImported)
          memcpy(ZeHandleDst, pHost, size);
      } else {
        // Initialize the buffer synchronously with immediate offload
        // zeCommandListAppendMemoryCopy must not be called from simultaneous
        // threads with the same command list handle, so we need exclusive lock.
        std::scoped_lock<pi_mutex> Lock(UrContext->ImmediateCommandListMutex);
        ZE2UR_CALL(zeCommandListAppendMemoryCopy,
                   (UrContext->ZeCommandListInit, ZeHandleDst, pHost, size,
                   nullptr, 0, nullptr));
      }
    } else if (flags == 0 || (flags == UR_MEM_FLAG_READ_WRITE)) {
      // Nothing more to do.
    }
  }

  *phBuffer = reinterpret_cast<ur_mem_handle_t>(Buffer);
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemRetain(
    ur_mem_handle_t hMem ///< [in] handle of the memory object to get access
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemRelease(
    ur_mem_handle_t hMem ///< [in] handle of the memory object to release
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemBufferPartition(
    ur_mem_handle_t
        hBuffer,          ///< [in] handle of the buffer object to allocate from
    ur_mem_flags_t flags, ///< [in] allocation and usage information flags
    ur_buffer_create_type_t bufferCreateType, ///< [in] buffer creation type
    ur_buffer_region_t *
        pBufferCreateInfo, ///< [in] pointer to buffer create region information
    ur_mem_handle_t
        *phMem ///< [out] pointer to the handle of sub buffer created
) {
  _ur_mem_handle_t *UrBuffer = ur_cast<_ur_mem_handle_t *>(hBuffer);

  std::shared_lock<pi_shared_mutex> Guard(UrBuffer->Mutex);

  if (flags != UR_MEM_FLAG_READ_WRITE) {
    die("urMemBufferPartition: Level-Zero implements only read-write buffer,"
        "no read-only or write-only yet.");
  }

  *phMem = hBuffer;

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemGetNativeHandle(
    ur_mem_handle_t hMem, ///< [in] handle of the mem.
    ur_native_handle_t
        *phNativeMem ///< [out] a pointer to the native handle of the mem.
) {
  _ur_mem_handle_t *UrMem = ur_cast<_ur_mem_handle_t *>(hMem);

  std::shared_lock<pi_shared_mutex> Guard(UrMem->Mutex);
  char *ZeHandle = nullptr;
  UR_CALL(UrMem->getZeHandle(ZeHandle, _ur_mem_handle_t::read_write));
  *phNativeMem = ur_cast<ur_native_handle_t>(ZeHandle);

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemCreateWithNativeHandle(
    ur_native_handle_t hNativeMem, ///< [in] the native handle of the mem.
    ur_context_handle_t hContext,  ///< [in] handle of the context object
    ur_mem_handle_t
        *phMem ///< [out] pointer to the handle of the mem object created.
) {
  
  _ur_mem_handle_t *UrMem = ur_cast<_ur_mem_handle_t *>(hNativeMem);
  _ur_context_handle_t *UrContext = UrMem->UrContext;

  std::shared_lock<pi_shared_mutex> Lock(UrContext->Mutex);

  // Get base of the allocation
  void *Base = nullptr;
  size_t Size = 0;
  void *Ptr = ur_cast<void *>(hNativeMem);
  ZE2UR_CALL(zeMemGetAddressRange, (UrContext->ZeContext,
                                    Ptr,
                                    &Base,
                                    &Size));
#if 0
  PI_ASSERT(Ptr == Base, PI_ERROR_INVALID_VALUE);
#endif

  ZeStruct<ze_memory_allocation_properties_t> ZeMemProps;
  ze_device_handle_t ZeDevice = nullptr;
  ZE2UR_CALL(zeMemGetAllocProperties, (UrContext->ZeContext,
                                       Ptr,
                                       &ZeMemProps,
                                       &ZeDevice));

  // Check type of the allocation
  switch (ZeMemProps.type) {
  case ZE_MEMORY_TYPE_HOST:
  case ZE_MEMORY_TYPE_SHARED:
  case ZE_MEMORY_TYPE_DEVICE:
    break;
  case ZE_MEMORY_TYPE_UNKNOWN:
    // Memory allocation is unrelated to the context
    return UR_RESULT_ERROR_INVALID_CONTEXT;
  default:
    die("Unexpected memory type");
  }

  ur_device_handle_t Device {};
  if (ZeDevice) {
    Device = UrContext->getPlatform()->getDeviceFromNativeHandle(ZeDevice);
    // PI_ASSERT(Context->isValidDevice(Device), PI_ERROR_INVALID_CONTEXT);
  }

  _pi_buffer *Buffer = nullptr;
  try {
    Buffer = new _pi_buffer(hContext, Device, Size);
    *phMem = reinterpret_cast<ur_mem_handle_t>(Buffer);
  } catch (const std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  ur_platform_handle_t Plt = UrContext->getPlatform();
  std::unique_lock<pi_shared_mutex> ContextsLock(Plt->ContextsMutex,
                                                  std::defer_lock);
  if (IndirectAccessTrackingEnabled) {
    // We need to keep track of all memory allocations in the context
    ContextsLock.lock();
    // Retain context to be sure that it is released after all memory
    // allocations in this context are released.
#if 0
    PI_CALL(piContextRetain(UrContext));
#endif

    UrContext->MemAllocs.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(Ptr),
                                  std::forward_as_tuple(reinterpret_cast<ur_context_handle_t>(UrContext),
                                  true /*ownNativeHandle, how do we pass it here? or do we move all this logic to pi2ur? */
                                  ));
  }

  if (Device) {
    // If this allocation is on a device, then we re-use it for the buffer.
    // Nothing to do.
  } else if (Buffer->OnHost) {
    // If this is host allocation and buffer always stays on host there
    // nothing more to do.
  } else {
    // In all other cases (shared allocation, or host allocation that cannot
    // represent the buffer in this context) copy the data to a newly
    // created device allocation.
    char *ZeHandleDst;
    UR_CALL(Buffer->getZeHandle(ZeHandleDst,
                                _ur_mem_handle_t::write_only,
                                Device));

    // zeCommandListAppendMemoryCopy must not be called from simultaneous
    // threads with the same command list handle, so we need exclusive lock.
    std::scoped_lock<pi_mutex> Lock(UrContext->ImmediateCommandListMutex);
    ZE2UR_CALL(zeCommandListAppendMemoryCopy, (UrContext->ZeCommandListInit,
                                               ZeHandleDst,
                                               Ptr,
                                               Size,
                                               nullptr,
                                               0,
                                               nullptr));
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemGetInfo(
    ur_mem_handle_t
        hMemory, ///< [in] handle to the memory object being queried.
    ur_mem_info_t MemInfoType, ///< [in] type of the info to retrieve.
    size_t propSize, ///< [in] the number of bytes of memory pointed to by
                     ///< pMemInfo.
    void *pMemInfo,  ///< [out][optional] array of bytes holding the info.
                     ///< If propSize is less than the real number of bytes
                     ///< needed to return the info then the
                     ///< ::UR_RESULT_ERROR_INVALID_SIZE error is returned and
                     ///< pMemInfo is not used.
    size_t *pPropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data queried by pMemInfo.
) {
  auto Buffer = reinterpret_cast<_pi_buffer *>(hMemory);
  std::shared_lock<pi_shared_mutex> Lock(Buffer->Mutex);
#if 0
  ReturnHelper ReturnValue(ParamValueSize, ParamValue, ParamValueSizeRet);
#endif

  switch (MemInfoType) {
  case UR_MEM_INFO_CONTEXT: {
    std::memcpy(pMemInfo, &(Buffer->UrContext), propSize);
    if (pPropSizeRet) {
      *pPropSizeRet = sizeof(Buffer->UrContext);
    }
    break;
  }
  case UR_MEM_INFO_SIZE: {
    // Get size of the allocation
    std::memcpy(pMemInfo, &(Buffer->Size), propSize);
    if (pPropSizeRet)
      *pPropSizeRet = sizeof(Buffer->Size);
    break;
  }
  default:
    die("urMemGetInfo: Parameter is not implemented");
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemImageGetInfo(
    ur_mem_handle_t hMemory, ///< [in] handle to the image object being queried.
    ur_image_info_t ImgInfoType, ///< [in] type of image info to retrieve.
    size_t propSize, ///< [in] the number of bytes of memory pointer to by
                     ///< pImgInfo.
    void *pImgInfo,  ///< [out][optional] array of bytes holding the info.
                     ///< If propSize is less than the real number of bytes
                     ///< needed to return the info then the
                     ///< ::UR_RESULT_ERROR_INVALID_SIZE error is returned and
                     ///< pImgInfo is not used.
    size_t *pPropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data queried by pImgInfo.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMHostAlloc(
    ur_context_handle_t hContext, ///< [in] handle of the context object
    ur_usm_mem_flags_t *pUSMFlag, ///< [in] USM memory allocation flags
    size_t
        size, ///< [in] size in bytes of the USM memory object to be allocated
    uint32_t align, ///< [in] alignment of the USM memory object
    void **ppMem    ///< [out] pointer to USM host memory object
) {
  // L0 supports alignment up to 64KB and silently ignores higher values.
  // We flag alignment > 64KB as an invalid value.
  if (align > 65536)
    return UR_RESULT_ERROR_INVALID_VALUE;

  _ur_context_handle_t *UrContext = reinterpret_cast<_ur_context_handle_t *>(hContext);

  ur_platform_handle_t Plt = UrContext->getPlatform();
  // If indirect access tracking is enabled then lock the mutex which is
  // guarding contexts container in the platform. This prevents new kernels from
  // being submitted in any context while we are in the process of allocating a
  // memory, this is needed to properly capture allocations by kernels with
  // indirect access. This lock also protects access to the context's data
  // structures. If indirect access tracking is not enabled then lock context
  // mutex to protect access to context's data structures.
  std::shared_lock<pi_shared_mutex> ContextLock(UrContext->Mutex,
                                                std::defer_lock);
  std::unique_lock<pi_shared_mutex> IndirectAccessTrackingLock(
      Plt->ContextsMutex, std::defer_lock);
  if (IndirectAccessTrackingEnabled) {
    IndirectAccessTrackingLock.lock();
    // We are going to defer memory release if there are kernels with indirect
    // access, that is why explicitly retain context to be sure that it is
    // released after all memory allocations in this context are released.
#if 0
    UR_CALL(piContextRetain(UrContext));
#endif
  } else {
    ContextLock.lock();
  }

  if (!UseUSMAllocator ||
      // L0 spec says that allocation fails if Alignment != 2^n, in order to
      // keep the same behavior for the allocator, just call L0 API directly and
      // return the error code.
      ((align & (align - 1)) != 0)) {
      pi_usm_mem_properties Properties {};
      ur_result_t Res =  USMHostAllocImpl(ppMem,
                                          reinterpret_cast<ur_context_handle_t>(UrContext),
                                          &Properties,
                                          size,
                                          align);
    if (IndirectAccessTrackingEnabled) {
      // Keep track of all memory allocations in the context
      UrContext->MemAllocs.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(*ppMem),
                                 std::forward_as_tuple(reinterpret_cast<ur_context_handle_t>(UrContext)));
    }
    return Res;
  }

  // There is a single allocator for Host USM allocations, so we don't need to
  // find the allocator depending on context as we do for Shared and Device
  // allocations.
  try {
    *ppMem = UrContext->HostMemAllocContext->allocate(size, align);
    if (IndirectAccessTrackingEnabled) {
      // Keep track of all memory allocations in the context
      UrContext->MemAllocs.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(*ppMem),
                                 std::forward_as_tuple(reinterpret_cast<ur_context_handle_t>(UrContext)));
    }
  } catch (const UsmAllocationException &Ex) {
    *ppMem = nullptr;
    return Ex.getError();
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  return UR_RESULT_SUCCESS;

}

UR_APIEXPORT ur_result_t UR_APICALL urUSMDeviceAlloc(
    ur_context_handle_t hContext, ///< [in] handle of the context object
    ur_device_handle_t hDevice,   ///< [in] handle of the device object
    ur_usm_mem_flags_t *pUSMProp, ///< [in] USM memory properties
    size_t
        size, ///< [in] size in bytes of the USM memory object to be allocated
    uint32_t align, ///< [in] alignment of the USM memory object
    void **ppMem    ///< [out] pointer to USM device memory object
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMSharedAlloc(
    ur_context_handle_t hContext, ///< [in] handle of the context object
    ur_device_handle_t hDevice,   ///< [in] handle of the device object
    ur_usm_mem_flags_t *pUSMProp, ///< [in] USM memory properties
    size_t
        size, ///< [in] size in bytes of the USM memory object to be allocated
    uint32_t align, ///< [in] alignment of the USM memory object
    void **ppMem    ///< [out] pointer to USM shared memory object
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMFree(
    ur_context_handle_t hContext, ///< [in] handle of the context object
    void *pMem                    ///< [in] pointer to USM memory object
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMGetMemAllocInfo(
    ur_context_handle_t hContext, ///< [in] handle of the context object
    const void *pMem,             ///< [in] pointer to USM memory object
    ur_usm_alloc_info_t
        propName, ///< [in] the name of the USM allocation property to query
    size_t propValueSize, ///< [in] size in bytes of the USM allocation property
                          ///< value
    void *pPropValue, ///< [out][optional] value of the USM allocation property
    size_t *pPropValueSizeRet ///< [out][optional] bytes returned in USM
                              ///< allocation property
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}



ur_result_t USMFreeImpl(ur_context_handle_t Context, void *Ptr,
                             bool OwnZeMemHandle) {
  if (OwnZeMemHandle)
    ZE2UR_CALL(zeMemFree, (Context->ZeContext, Ptr));
  return UR_RESULT_SUCCESS;
}

void *USMMemoryAllocBase::allocate(size_t Size) {
  void *Ptr = nullptr;

  auto Res = allocateImpl(&Ptr, Size, sizeof(void *));
  if (Res != UR_RESULT_SUCCESS) {
    throw UsmAllocationException(Res);
  }

  return Ptr;
}

void *USMMemoryAllocBase::allocate(size_t Size, size_t Alignment) {
  void *Ptr = nullptr;

  auto Res = allocateImpl(&Ptr, Size, Alignment);
  if (Res != UR_RESULT_SUCCESS) {
    throw UsmAllocationException(Res);
  }
  return Ptr;
}

void USMMemoryAllocBase::deallocate(void *Ptr, bool OwnZeMemHandle) {
  auto Res = USMFreeImpl(Context, Ptr, OwnZeMemHandle);
  if (Res != UR_RESULT_SUCCESS) {
    throw UsmAllocationException(Res);
  }
}

ur_result_t USMSharedMemoryAlloc::allocateImpl(void **ResultPtr, size_t Size,
                                             uint32_t Alignment) {
  return USMSharedAllocImpl(ResultPtr, Context, Device, nullptr, Size,
                            Alignment);
}

ur_result_t USMSharedReadOnlyMemoryAlloc::allocateImpl(void **ResultPtr,
                                                     size_t Size,
                                                     uint32_t Alignment) {
  pi_usm_mem_properties Props[] = {PI_MEM_ALLOC_FLAGS,
                                   PI_MEM_ALLOC_DEVICE_READ_ONLY, 0};
  return USMSharedAllocImpl(ResultPtr, Context, Device, Props, Size, Alignment);
}

ur_result_t USMDeviceMemoryAlloc::allocateImpl(void **ResultPtr, size_t Size,
                                             uint32_t Alignment) {
  return USMDeviceAllocImpl(ResultPtr, Context, Device, nullptr, Size,
                            Alignment);
}

ur_result_t USMHostMemoryAlloc::allocateImpl(void **ResultPtr, size_t Size,
                                           uint32_t Alignment) {
  return USMHostAllocImpl(ResultPtr, Context, nullptr, Size, Alignment);
}

ur_result_t USMDeviceAllocImpl(void **ResultPtr, ur_context_handle_t Context,
                                    ur_device_handle_t Device,
                                    pi_usm_mem_properties *Properties,
                                    size_t Size, uint32_t Alignment) {
  // PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  // PI_ASSERT(Device, PI_ERROR_INVALID_DEVICE);

  // Check that incorrect bits are not set in the properties.
  // PI_ASSERT(!Properties || *Properties == 0 ||
  //               (*Properties == PI_MEM_ALLOC_FLAGS && *(Properties + 2) == 0),
  //           PI_ERROR_INVALID_VALUE);

  // TODO: translate PI properties to Level Zero flags
  ZeStruct<ze_device_mem_alloc_desc_t> ZeDesc;
  ZeDesc.flags = 0;
  ZeDesc.ordinal = 0;

  ZeStruct<ze_relaxed_allocation_limits_exp_desc_t> RelaxedDesc;
  if (Size > Device->ZeDeviceProperties->maxMemAllocSize) {
    // Tell Level-Zero to accept Size > maxMemAllocSize
    RelaxedDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    ZeDesc.pNext = &RelaxedDesc;
  }

  ZE2UR_CALL(zeMemAllocDevice, (Context->ZeContext, &ZeDesc, Size, Alignment,
                             Device->ZeDevice, ResultPtr));

  // PI_ASSERT(Alignment == 0 ||
  //               reinterpret_cast<std::uintptr_t>(*ResultPtr) % Alignment == 0,
  //           PI_ERROR_INVALID_VALUE);

  return UR_RESULT_SUCCESS;
}

ur_result_t USMSharedAllocImpl(void **ResultPtr, ur_context_handle_t Context,
                                    ur_device_handle_t Device, pi_usm_mem_properties *,
                                    size_t Size, uint32_t Alignment) {
  // PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  // PI_ASSERT(Device, PI_ERROR_INVALID_DEVICE);

  // TODO: translate PI properties to Level Zero flags
  ZeStruct<ze_host_mem_alloc_desc_t> ZeHostDesc;
  ZeHostDesc.flags = 0;
  ZeStruct<ze_device_mem_alloc_desc_t> ZeDevDesc;
  ZeDevDesc.flags = 0;
  ZeDevDesc.ordinal = 0;

  ZeStruct<ze_relaxed_allocation_limits_exp_desc_t> RelaxedDesc;
  if (Size > Device->ZeDeviceProperties->maxMemAllocSize) {
    // Tell Level-Zero to accept Size > maxMemAllocSize
    RelaxedDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    ZeDevDesc.pNext = &RelaxedDesc;
  }

  ZE2UR_CALL(zeMemAllocShared, (Context->ZeContext, &ZeDevDesc, &ZeHostDesc, Size,
                             Alignment, Device->ZeDevice, ResultPtr));

  // PI_ASSERT(Alignment == 0 ||
  //               reinterpret_cast<std::uintptr_t>(*ResultPtr) % Alignment == 0,
  //           PI_ERROR_INVALID_VALUE);

  // TODO: Handle PI_MEM_ALLOC_DEVICE_READ_ONLY.
  return UR_RESULT_SUCCESS;
}

ur_result_t USMHostAllocImpl(void **ResultPtr,
                             ur_context_handle_t Context,
                             pi_usm_mem_properties *Properties,
                             size_t Size,
                             uint32_t Alignment) {
  // PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);

  // Check that incorrect bits are not set in the properties.
  // PI_ASSERT(!Properties || *Properties == 0 ||
  //               (*Properties == PI_MEM_ALLOC_FLAGS && *(Properties + 2) == 0),
  //           PI_ERROR_INVALID_VALUE);

  // TODO: translate PI properties to Level Zero flags
  ZeStruct<ze_host_mem_alloc_desc_t> ZeHostDesc;
  ZeHostDesc.flags = 0;
  ZE2UR_CALL(zeMemAllocHost,
          (Context->ZeContext, &ZeHostDesc, Size, Alignment, ResultPtr));

  // PI_ASSERT(Alignment == 0 ||
  //               reinterpret_cast<std::uintptr_t>(*ResultPtr) % Alignment == 0,
  //           PI_ERROR_INVALID_VALUE);

  return UR_RESULT_SUCCESS;
}

// If indirect access tracking is not enabled then this functions just performs
// zeMemFree. If indirect access tracking is enabled then reference counting is
// performed.
ur_result_t ZeMemFreeHelper(ur_context_handle_t Context, void *Ptr,
                                 bool OwnZeMemHandle) {
  ur_platform_handle_t Plt = Context->getPlatform();
  std::unique_lock<pi_shared_mutex> ContextsLock(Plt->ContextsMutex,
                                                 std::defer_lock);
  if (IndirectAccessTrackingEnabled) {
    ContextsLock.lock();
    auto It = Context->MemAllocs.find(Ptr);
    if (It == std::end(Context->MemAllocs)) {
      die("All memory allocations must be tracked!");
    }
    if (!It->second.RefCount.decrementAndTest()) {
      // Memory can't be deallocated yet.
      return UR_RESULT_SUCCESS;
    }

    // Reference count is zero, it is ok to free memory.
    // We don't need to track this allocation anymore.
    Context->MemAllocs.erase(It);
  }

  if (OwnZeMemHandle)
    ZE2UR_CALL(zeMemFree, (Context->ZeContext, Ptr));

  if (IndirectAccessTrackingEnabled)
    UR_CALL(ContextReleaseHelper(Context));

  return UR_RESULT_SUCCESS;
}

bool ShouldUseUSMAllocator() {
  // Enable allocator by default if it's not explicitly disabled
  return std::getenv("SYCL_PI_LEVEL_ZERO_DISABLE_USM_ALLOCATOR") == nullptr;
}

const bool UseUSMAllocator = ShouldUseUSMAllocator();

// Helper function to deallocate USM memory, if indirect access support is
// enabled then a caller must lock the platform-level mutex guarding the
// container with contexts because deallocating the memory can turn RefCount of
// a context to 0 and as a result the context being removed from the list of
// tracked contexts.
// If indirect access tracking is not enabled then caller must lock Context
// mutex.
ur_result_t USMFreeHelper(ur_context_handle_t Context, void *Ptr,
                               bool OwnZeMemHandle) {
  if (IndirectAccessTrackingEnabled) {
    auto It = Context->MemAllocs.find(Ptr);
    if (It == std::end(Context->MemAllocs)) {
      die("All memory allocations must be tracked!");
    }
    if (!It->second.RefCount.decrementAndTest()) {
      // Memory can't be deallocated yet.
      return UR_RESULT_SUCCESS;
    }

    // Reference count is zero, it is ok to free memory.
    // We don't need to track this allocation anymore.
    Context->MemAllocs.erase(It);
  }

  if (!UseUSMAllocator) {
    ur_result_t Res = USMFreeImpl(reinterpret_cast<ur_context_handle_t>(Context), Ptr, OwnZeMemHandle);
    if (IndirectAccessTrackingEnabled)
      UR_CALL(ContextReleaseHelper(reinterpret_cast<ur_context_handle_t>(Context)));
    return Res;
  }

  // Query the device of the allocation to determine the right allocator context
  ze_device_handle_t ZeDeviceHandle;
  ZeStruct<ze_memory_allocation_properties_t> ZeMemoryAllocationProperties;

  // Query memory type of the pointer we're freeing to determine the correct
  // way to do it(directly or via an allocator)
  ZE2UR_CALL(zeMemGetAllocProperties,
          (Context->ZeContext, Ptr, &ZeMemoryAllocationProperties,
           &ZeDeviceHandle));

  // If memory type is host release from host pool
  if (ZeMemoryAllocationProperties.type == ZE_MEMORY_TYPE_HOST) {
    try {
      Context->HostMemAllocContext->deallocate(Ptr, OwnZeMemHandle);
    } catch (const UsmAllocationException &Ex) {
      return Ex.getError();
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }
    if (IndirectAccessTrackingEnabled)
      UR_CALL(ContextReleaseHelper(reinterpret_cast<ur_context_handle_t>(Context)));
    return UR_RESULT_SUCCESS;
  }

  // Points out an allocation in SharedReadOnlyMemAllocContexts
  auto SharedReadOnlyAllocsIterator = Context->SharedReadOnlyAllocs.end();

  if (!ZeDeviceHandle) {
    // The only case where it is OK not have device identified is
    // if the memory is not known to the driver. We should not ever get
    // this either, probably.
    PI_ASSERT(ZeMemoryAllocationProperties.type == ZE_MEMORY_TYPE_UNKNOWN,
              UR_RESULT_ERROR_INVALID_DEVICE);
  } else {
    ur_device_handle_t Device;
    // All context member devices or their descendants are of the same platform.
    auto Platform = Context->getPlatform();
    Device = Platform->getDeviceFromNativeHandle(ZeDeviceHandle);
    // PI_ASSERT(Device, PI_ERROR_INVALID_DEVICE);

    auto DeallocationHelper =
        [Context, Device, Ptr,
         OwnZeMemHandle](std::unordered_map<ze_device_handle_t, USMAllocContext>
                             &AllocContextMap) {
          try {
            auto It = AllocContextMap.find(Device->ZeDevice);
            if (It == AllocContextMap.end())
              return UR_RESULT_ERROR_INVALID_VALUE;

            // The right context is found, deallocate the pointer
            It->second.deallocate(Ptr, OwnZeMemHandle);
          } catch (const UsmAllocationException &Ex) {
            return Ex.getError();
          }

          if (IndirectAccessTrackingEnabled)
            UR_CALL(ContextReleaseHelper(reinterpret_cast<ur_context_handle_t>(Context)));
          return UR_RESULT_SUCCESS;
        };

    switch (ZeMemoryAllocationProperties.type) {
    case ZE_MEMORY_TYPE_SHARED:
      // Distinguish device_read_only allocations since they have own pool.
      SharedReadOnlyAllocsIterator = Context->SharedReadOnlyAllocs.find(Ptr);
      return DeallocationHelper(SharedReadOnlyAllocsIterator !=
                                        Context->SharedReadOnlyAllocs.end()
                                    ? Context->SharedReadOnlyMemAllocContexts
                                    : Context->SharedMemAllocContexts);
    case ZE_MEMORY_TYPE_DEVICE:
      return DeallocationHelper(Context->DeviceMemAllocContexts);
    default:
      // Handled below
      break;
    }
  }

  ur_result_t Res =USMFreeImpl(reinterpret_cast<ur_context_handle_t>(Context), Ptr, OwnZeMemHandle);
  if (SharedReadOnlyAllocsIterator != Context->SharedReadOnlyAllocs.end()) {
    Context->SharedReadOnlyAllocs.erase(SharedReadOnlyAllocsIterator);
  }
  if (IndirectAccessTrackingEnabled)
    UR_CALL(ContextReleaseHelper(reinterpret_cast<ur_context_handle_t>(Context)));
  return Res;
}

// If indirect access tracking is enabled then performs reference counting,
// otherwise just calls zeMemAllocDevice.
static ur_result_t ZeDeviceMemAllocHelper(void **ResultPtr,
                                          ur_context_handle_t Context,
                                          ur_device_handle_t Device,
                                          size_t Size) {
  ur_platform_handle_t Plt = Device->Platform;
  std::unique_lock<pi_shared_mutex> ContextsLock(Plt->ContextsMutex,
                                                 std::defer_lock);
  if (IndirectAccessTrackingEnabled) {
    // Lock the mutex which is guarding contexts container in the platform.
    // This prevents new kernels from being submitted in any context while
    // we are in the process of allocating a memory, this is needed to
    // properly capture allocations by kernels with indirect access.
    ContextsLock.lock();
    // We are going to defer memory release if there are kernels with
    // indirect access, that is why explicitly retain context to be sure
    // that it is released after all memory allocations in this context are
    // released.
#if 0 
    PI_CALL(piContextRetain(Context));
#endif
  }

  ze_device_mem_alloc_desc_t ZeDesc = {};
  ZeDesc.flags = 0;
  ZeDesc.ordinal = 0;
  ZE2UR_CALL(zeMemAllocDevice, (Context->ZeContext,
                                &ZeDesc,
                                Size,
                                1,
                                Device->ZeDevice,
                                ResultPtr));

  if (IndirectAccessTrackingEnabled) {
    // Keep track of all memory allocations in the context
    Context->MemAllocs.emplace(std::piecewise_construct,
                               std::forward_as_tuple(*ResultPtr),
                               std::forward_as_tuple(reinterpret_cast<ur_context_handle_t>(Context)));
  }
  return UR_RESULT_SUCCESS;
}


ur_result_t _pi_buffer::getZeHandle(char *&ZeHandle,
                                  access_mode_t AccessMode,
                                  ur_device_handle_t Device) {

  // NOTE: There might be no valid allocation at all yet and we get
  // here from piEnqueueKernelLaunch that would be doing the buffer
  // initialization. In this case the Device is not null as kernel
  // launch is always on a specific device.
  if (!Device)
    Device = LastDeviceWithValidAllocation;
  // If the device is still not selected then use the first one in
  // the context of the buffer.
  if (!Device)
    Device = UrContext->Devices[0];

  auto &Allocation = Allocations[Device];

  // Sub-buffers don't maintain own allocations but rely on parent buffer.
  if (isSubBuffer()) {
    UR_CALL(SubBuffer.Parent->getZeHandle(ZeHandle, AccessMode, Device));
    ZeHandle += SubBuffer.Origin;
    // Still store the allocation info in the PI sub-buffer for
    // getZeHandlePtr to work. At least zeKernelSetArgumentValue needs to
    // be given a pointer to the allocation handle rather than its value.
    //
    Allocation.ZeHandle = ZeHandle;
    Allocation.ReleaseAction = allocation_t::keep;
    LastDeviceWithValidAllocation = Device;
    return UR_RESULT_SUCCESS;
  }

  // First handle case where the buffer is represented by only
  // a single host allocation.
  if (OnHost) {
    auto &HostAllocation = Allocations[nullptr];
    // The host allocation may already exists, e.g. with imported
    // host ptr, or in case of interop buffer.
    if (!HostAllocation.ZeHandle) {
      if (USMAllocatorConfigInstance.EnableBuffers) {
        HostAllocation.ReleaseAction = allocation_t::free;
        UR_CALL(urUSMHostAlloc(UrContext,
                               nullptr,
                               Size,
                               getAlignment(),
                               reinterpret_cast<void **>(&ZeHandle)));
      } else {
        HostAllocation.ReleaseAction = allocation_t::free_native;
        UR_CALL(ZeHostMemAllocHelper(reinterpret_cast<void **>(&ZeHandle),
                                     UrContext,
                                     Size));
      }
      HostAllocation.ZeHandle = ZeHandle;
      HostAllocation.Valid = true;
    }
    Allocation = HostAllocation;
    Allocation.ReleaseAction = allocation_t::keep;
    ZeHandle = Allocation.ZeHandle;
    LastDeviceWithValidAllocation = Device;
    return UR_RESULT_SUCCESS;
  }
  // Reads user setting on how to deal with buffers in contexts where
  // all devices have the same root-device. Returns "true" if the
  // preference is to have allocate on each [sub-]device and migrate
  // normally (copy) to other sub-devices as needed. Returns "false"
  // if the preference is to have single root-device allocations
  // serve the needs of all [sub-]devices, meaning potentially more
  // cross-tile traffic.
  //
  static const bool SingleRootDeviceBufferMigration = [] {
    const char *EnvStr =
        std::getenv("SYCL_PI_LEVEL_ZERO_SINGLE_ROOT_DEVICE_BUFFER_MIGRATION");
    if (EnvStr)
      return (std::stoi(EnvStr) != 0);
    // The default is to migrate normally, which may not always be the
    // best option (depends on buffer access patterns), but is an
    // overall win on the set of the available benchmarks.
    return true;
  }();

  // Peform actual device allocation as needed.
  if (!Allocation.ZeHandle) {
    if (!SingleRootDeviceBufferMigration && UrContext->SingleRootDevice &&
        UrContext->SingleRootDevice != Device) {
      // If all devices in the context are sub-devices of the same device
      // then we reuse root-device allocation by all sub-devices in the
      // context.
      // TODO: we can probably generalize this and share root-device
      //       allocations by its own sub-devices even if not all other
      //       devices in the context have the same root.
      UR_CALL(getZeHandle(ZeHandle, AccessMode, UrContext->SingleRootDevice));
      Allocation.ReleaseAction = allocation_t::keep;
      Allocation.ZeHandle = ZeHandle;
      Allocation.Valid = true;
      return UR_RESULT_SUCCESS;
    } else { // Create device allocation
      if (USMAllocatorConfigInstance.EnableBuffers) {
        Allocation.ReleaseAction = allocation_t::free;
        UR_CALL(urUSMDeviceAlloc(UrContext,
                                 Device,
                                 nullptr,
                                 Size,
                                 getAlignment(),
                                 reinterpret_cast<void **>(&ZeHandle)));
      } else {
        Allocation.ReleaseAction = allocation_t::free_native;
        UR_CALL(ZeDeviceMemAllocHelper(reinterpret_cast<void **>(&ZeHandle),
                                       UrContext,
                                       Device,
                                       Size));
      }
    }
    Allocation.ZeHandle = ZeHandle;
  } else {
    ZeHandle = Allocation.ZeHandle;
  }

  // If some prior access invalidated this allocation then make it valid again.
  if (!Allocation.Valid) {
    // LastDeviceWithValidAllocation should always have valid allocation.
    if (Device == LastDeviceWithValidAllocation)
      die("getZeHandle: last used allocation is not valid");

    // For write-only access the allocation contents is not going to be used.
    // So don't do anything to make it "valid".
    bool NeedCopy = AccessMode != _ur_mem_handle_t::write_only;
    // It's also possible that the buffer doesn't have a valid allocation
    // yet presumably when it is passed to a kernel that will perform
    // it's intialization.
    if (NeedCopy && !LastDeviceWithValidAllocation) {
      NeedCopy = false;
    }
    char *ZeHandleSrc = nullptr;
    if (NeedCopy) {
      UR_CALL(getZeHandle(ZeHandleSrc,
                          _ur_mem_handle_t::read_only,
                          LastDeviceWithValidAllocation));
      // It's possible with the single root-device contexts that
      // the buffer is represented by the single root-device
      // allocation and then skip the copy to itself.
      if (ZeHandleSrc == ZeHandle)
        NeedCopy = false;
    }

    if (NeedCopy) {
      // Copy valid buffer data to this allocation.
      // TODO: see if we should better use peer's device allocation used
      // directly, if that capability is reported with zeDeviceCanAccessPeer,
      // instead of maintaining a separate allocation and performing
      // explciit copies.
      //
      // zeCommandListAppendMemoryCopy must not be called from simultaneous
      // threads with the same command list handle, so we need exclusive lock.
      ze_bool_t P2P = false;
      ZE2UR_CALL(zeDeviceCanAccessPeer,(Device->ZeDevice,
                                        LastDeviceWithValidAllocation->ZeDevice,
                                        &P2P));
      if (!P2P) {
        // P2P copy is not possible, so copy through the host.
        auto &HostAllocation = Allocations[nullptr];
        // The host allocation may already exists, e.g. with imported
        // host ptr, or in case of interop buffer.
        if (!HostAllocation.ZeHandle) {
          void *ZeHandleHost;
          if (USMAllocatorConfigInstance.EnableBuffers) {
            HostAllocation.ReleaseAction = allocation_t::free;
            UR_CALL(urUSMHostAlloc(UrContext,
                                   nullptr,
                                   Size,
                                   getAlignment(),
                                   &ZeHandleHost));
          } else {
            HostAllocation.ReleaseAction = allocation_t::free_native;
            UR_CALL(ZeHostMemAllocHelper(&ZeHandleHost, UrContext, Size));
          }
          HostAllocation.ZeHandle = reinterpret_cast<char *>(ZeHandleHost);
          HostAllocation.Valid = false;
        }
        std::scoped_lock<pi_mutex> Lock(UrContext->ImmediateCommandListMutex);
        if (!HostAllocation.Valid) {
          ZE2UR_CALL(zeCommandListAppendMemoryCopy,(UrContext->ZeCommandListInit,
                                                    HostAllocation.ZeHandle,
                                                    ZeHandleSrc,
                                                    Size,
                                                    nullptr,
                                                    0,
                                                    nullptr));
          // Mark the host allocation data  as valid so it can be reused.
          // It will be invalidated below if the current access is not
          // read-only.
          HostAllocation.Valid = true;
        }
        ZE2UR_CALL(zeCommandListAppendMemoryCopy,(UrContext->ZeCommandListInit,
                                               ZeHandle,
                                               HostAllocation.ZeHandle,
                                               Size,
                                               nullptr,
                                               0,
                                               nullptr));
      } else {
        // Perform P2P copy.
        std::scoped_lock<pi_mutex> Lock(UrContext->ImmediateCommandListMutex);
        ZE2UR_CALL(zeCommandListAppendMemoryCopy,
                (UrContext->ZeCommandListInit,
                 ZeHandle,
                 ZeHandleSrc,
                 Size,
                 nullptr,
                 0,
                 nullptr));
      }
    }
    Allocation.Valid = true;
    LastDeviceWithValidAllocation = Device;
  }

  // Invalidate other allocations that would become not valid if
  // this access is not read-only.
  if (AccessMode != _ur_mem_handle_t::read_only) {
    for (auto &Alloc : Allocations) {
      if (Alloc.first != LastDeviceWithValidAllocation)
        Alloc.second.Valid = false;
    }
  }

  zePrint("getZeHandle(pi_device{%p}) = %p\n", (void *)Device,
          (void *)Allocation.ZeHandle);
  return UR_RESULT_SUCCESS;
}

ur_result_t _pi_buffer::free() {
  for (auto &Alloc : Allocations) {
    auto &ZeHandle = Alloc.second.ZeHandle;
    // It is possible that the real allocation wasn't made if the buffer
    // wasn't really used in this location.
    if (!ZeHandle)
      continue;

    switch (Alloc.second.ReleaseAction) {
    case allocation_t::keep:
      break;
    case allocation_t::free: {
      ur_platform_handle_t Plt = UrContext->getPlatform();
      std::scoped_lock<pi_shared_mutex> Lock(
          IndirectAccessTrackingEnabled ? Plt->ContextsMutex : UrContext->Mutex);

      UR_CALL(USMFreeHelper(reinterpret_cast<ur_context_handle_t>(UrContext),
                            ZeHandle,
                            true));
      break;
    }
    case allocation_t::free_native:
      UR_CALL(ZeMemFreeHelper(reinterpret_cast<ur_context_handle_t>(UrContext),
                              ZeHandle, true));
      break;
    case allocation_t::unimport:
      ZeUSMImport.doZeUSMRelease(UrContext->getPlatform()->ZeDriver,
                                 ZeHandle);
      break;
    default:
      die("_pi_buffer::free(): Unhandled release action");
    }
    ZeHandle = nullptr; // don't leave hanging pointers
  }
  return UR_RESULT_SUCCESS;
}

// Buffer constructor
_pi_buffer::_pi_buffer(ur_context_handle_t Context,
                       size_t Size, char *HostPtr,
                       bool ImportedHostPtr = false)
    : _ur_mem_handle_t(Context), Size(Size), SubBuffer{nullptr, 0} {

  // We treat integrated devices (physical memory shared with the CPU)
  // differently from discrete devices (those with distinct memories).
  // For integrated devices, allocating the buffer in the host memory
  // enables automatic access from the device, and makes copying
  // unnecessary in the map/unmap operations. This improves performance.
  OnHost = Context->Devices.size() == 1 &&
            Context->Devices[0]->ZeDeviceProperties->flags &
                ZE_DEVICE_PROPERTY_FLAG_INTEGRATED;

  // Fill the host allocation data.
  if (HostPtr) {
    MapHostPtr = HostPtr;
    // If this host ptr is imported to USM then use this as a host
    // allocation for this buffer.
    if (ImportedHostPtr) {
      Allocations[nullptr].ZeHandle = HostPtr;
      Allocations[nullptr].Valid = true;
      Allocations[nullptr].ReleaseAction = _pi_buffer::allocation_t::unimport;
    }
  }

  // This initialization does not end up with any valid allocation yet.
  LastDeviceWithValidAllocation = nullptr;
}


_pi_buffer::_pi_buffer(ur_context_handle_t Context,
                       ur_device_handle_t Device,
                       size_t Size)
    : _ur_mem_handle_t(Context, Device), Size(Size) {
}    

// Interop-buffer constructor
_pi_buffer::_pi_buffer(ur_context_handle_t Context,
                       size_t Size,
                       ur_device_handle_t Device,
                       char *ZeMemHandle,
                       bool OwnZeMemHandle)
    : _ur_mem_handle_t(Context, Device), Size(Size), SubBuffer{nullptr, 0} {

  // Device == nullptr means host allocation
  Allocations[Device].ZeHandle = ZeMemHandle;
  Allocations[Device].Valid = true;
  Allocations[Device].ReleaseAction =
      OwnZeMemHandle ? allocation_t::free_native : allocation_t::keep;

  // Check if this buffer can always stay on host
  OnHost = false;
  if (!Device) { // Host allocation
    if (Context->Devices.size() == 1 &&
        Context->Devices[0]->ZeDeviceProperties->flags &
            ZE_DEVICE_PROPERTY_FLAG_INTEGRATED) {
      OnHost = true;
      MapHostPtr = ZeMemHandle; // map to this allocation
    }
  }
  LastDeviceWithValidAllocation = Device;
}

ur_result_t _pi_buffer::getZeHandlePtr(char **&ZeHandlePtr,
                                     access_mode_t AccessMode,
                                     ur_device_handle_t Device) {
  char *ZeHandle;
  UR_CALL(getZeHandle(ZeHandle, AccessMode, Device));
  ZeHandlePtr = &Allocations[Device].ZeHandle;
  return UR_RESULT_SUCCESS;
}

size_t _pi_buffer::getAlignment() const {
  // Choose an alignment that is at most 64 and is the next power of 2
  // for sizes less than 64.
  auto Alignment = Size;
  if (Alignment > 32UL)
    Alignment = 64UL;
  else if (Alignment > 16UL)
    Alignment = 32UL;
  else if (Alignment > 8UL)
    Alignment = 16UL;
  else if (Alignment > 4UL)
    Alignment = 8UL;
  else if (Alignment > 2UL)
    Alignment = 4UL;
  else if (Alignment > 1UL)
    Alignment = 2UL;
  else
    Alignment = 1UL;
  return Alignment;
}

