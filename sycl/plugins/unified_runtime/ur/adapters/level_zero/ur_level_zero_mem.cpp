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
#include <ur_bindings.hpp>

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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
    size_t inputRowPitch,    ///< [in] length of each row in bytes
    size_t inputSlicePitch,  ///< [in] length of each 2D slice of the 3D image
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemBufferCreate(
    ur_context_handle_t hContext, ///< [in] handle of the context object
    ur_mem_flags_t flags, ///< [in] allocation and usage information flags
    size_t size, ///< [in] size in bytes of the memory object to be allocated
    void *pHost, ///< [in][optional] pointer to the buffer data
    ur_mem_handle_t
        *phBuffer ///< [out] pointer to handle of the memory buffer created
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemGetNativeHandle(
    ur_mem_handle_t hMem, ///< [in] handle of the mem.
    ur_native_handle_t
        *phNativeMem ///< [out] a pointer to the native handle of the mem.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemCreateWithNativeHandle(
    ur_native_handle_t hNativeMem, ///< [in] the native handle of the mem.
    ur_context_handle_t hContext,  ///< [in] handle of the context object
    ur_mem_handle_t
        *phMem ///< [out] pointer to the handle of the mem object created.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
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