//===---------------- pi2ur.hpp - PI API to UR API  --------------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//
#pragma once

#include <unordered_map>

#include "pi2ur_context.hpp"

#include "ur_api.h"
#include <sycl/detail/pi.h>
#include <ur/ur.hpp>
#include <cstdarg>

// #include "pi2ur_common.hpp"

// Map of UR error codes to PI error codes
static pi_result ur2piResult(ur_result_t urResult) {
  std::unordered_map<ur_result_t, pi_result> ErrorMapping = {
      {UR_RESULT_SUCCESS, PI_SUCCESS},
      {UR_RESULT_ERROR_UNKNOWN, PI_ERROR_UNKNOWN},
      {UR_RESULT_ERROR_DEVICE_LOST, PI_ERROR_DEVICE_NOT_FOUND},
      {UR_RESULT_ERROR_INVALID_OPERATION, PI_ERROR_INVALID_OPERATION},
      {UR_RESULT_ERROR_INVALID_PLATFORM, PI_ERROR_INVALID_PLATFORM},
      {UR_RESULT_ERROR_INVALID_ARGUMENT, PI_ERROR_INVALID_ARG_VALUE},
      {UR_RESULT_ERROR_INVALID_VALUE, PI_ERROR_INVALID_VALUE},
      {UR_RESULT_ERROR_INVALID_EVENT, PI_ERROR_INVALID_EVENT},
      {UR_RESULT_ERROR_INVALID_BINARY, PI_ERROR_INVALID_BINARY},
      {UR_RESULT_ERROR_INVALID_KERNEL_NAME, PI_ERROR_INVALID_KERNEL_NAME},
      {UR_RESULT_ERROR_INVALID_FUNCTION_NAME, PI_ERROR_BUILD_PROGRAM_FAILURE},
      {UR_RESULT_ERROR_INVALID_WORK_GROUP_SIZE,
       PI_ERROR_INVALID_WORK_GROUP_SIZE},
      {UR_RESULT_ERROR_MODULE_BUILD_FAILURE, PI_ERROR_BUILD_PROGRAM_FAILURE},
      {UR_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, PI_ERROR_OUT_OF_RESOURCES},
      {UR_RESULT_ERROR_OUT_OF_HOST_MEMORY, PI_ERROR_OUT_OF_HOST_MEMORY}};

  auto It = ErrorMapping.find(urResult);
  if (It == ErrorMapping.end()) {
    return PI_ERROR_UNKNOWN;
  }
  return It->second;
}

// Early exits on any error
#define HANDLE_ERRORS(urCall)                                                  \
  if (auto Result = urCall)                                                    \
    return ur2piResult(Result);

// A version of return helper that returns pi_result and not ur_result_t
class ReturnHelper : public UrReturnHelper {
public:
  using UrReturnHelper::UrReturnHelper;

  template <class T> pi_result operator()(const T &t) {
    return ur2piResult(UrReturnHelper::operator()(t));
  }
  // Array return value
  template <class T> pi_result operator()(const T *t, size_t s) {
    return ur2piResult(UrReturnHelper::operator()(t, s));
  }
  // Array return value where element type is differrent from T
  template <class RetType, class T> pi_result operator()(const T *t, size_t s) {
    return ur2piResult(UrReturnHelper::operator()<RetType>(t, s));
  }
};

// A version of return helper that supports conversion through a map
class ConvertHelper : public ReturnHelper {
  using ReturnHelper::ReturnHelper;

public:
  // Convert the value using a conversion map
  template <typename TypeUR, typename TypePI>
  pi_result convert(const std::unordered_map<TypeUR, TypePI> &Map) {
    *param_value_size_ret = sizeof(TypePI);

    // There is no value to convert.
    if (!param_value)
      return PI_SUCCESS;

    auto pValueUR = static_cast<TypeUR *>(param_value);
    auto pValuePI = static_cast<TypePI *>(param_value);

    // Cannot convert to a smaller storage type
    PI_ASSERT(sizeof(TypePI) >= sizeof(TypeUR), PI_ERROR_UNKNOWN);

    auto It = Map.find(*pValueUR);
    if (It == Map.end()) {
      die("ConvertHelper: unhandled value");
    }

    *pValuePI = It->second;
    return PI_SUCCESS;
  }

  // Convert the array (0-terminated) using a conversion map
  template <typename TypeUR, typename TypePI>
  pi_result convertArray(const std::unordered_map<TypeUR, TypePI> &Map) {
    // Cannot convert to a smaller element storage type
    PI_ASSERT(sizeof(TypePI) >= sizeof(TypeUR), PI_ERROR_UNKNOWN);
    *param_value_size_ret *= sizeof(TypePI) / sizeof(TypeUR);

    // There is no value to convert. Adjust to a possibly bigger PI storage.
    if (!param_value)
      return PI_SUCCESS;

    PI_ASSERT(*param_value_size_ret % sizeof(TypePI) == 0, PI_ERROR_UNKNOWN);

    // Make a copy of the input UR array as we may possibly overwrite following
    // elements while converting previous ones (if extending).
    auto ValueUR = new char[*param_value_size_ret];
    auto pValueUR = reinterpret_cast<TypeUR *>(ValueUR);
    auto pValuePI = static_cast<TypePI *>(param_value);
    memcpy(pValueUR, param_value, *param_value_size_ret);

    while (pValueUR) {
      if (*pValueUR == 0) {
        *pValuePI = 0;
        break;
      }

      auto It = Map.find(*pValueUR);
      if (It == Map.end()) {
        die("ConvertHelper: unhandled value");
      }
      *pValuePI = It->second;
      ++pValuePI;
      ++pValueUR;
    }

    delete[] ValueUR;
    return PI_SUCCESS;
  }

  // Convert the bitset using a conversion map
  template <typename TypeUR, typename TypePI>
  pi_result convertBitSet(const std::unordered_map<TypeUR, TypePI> &Map) {
    // There is no value to convert.
    if (!param_value)
      return PI_SUCCESS;

    auto pValuePI = static_cast<TypePI *>(param_value);
    auto pValueUR = static_cast<TypeUR *>(param_value);

    // Cannot handle biteset large than size_t
    PI_ASSERT(sizeof(TypeUR) <= sizeof(size_t), PI_ERROR_UNKNOWN);
    size_t In = *pValueUR;
    TypePI Out = 0;

    size_t Val;
    while ((Val = In & -In)) { // Val is the rightmost set bit in In
      In &= In - 1;            // Reset the rightmost set bit

      // Convert the Val alone and merge it into Out
      *pValueUR = TypeUR(Val);
      if (auto Res = convert(Map))
        return Res;
      Out |= *pValuePI;
    }
    *pValuePI = TypePI(Out);
    return PI_SUCCESS;
  }
};

// Translate UR info values to PI info values
inline pi_result ur2piInfoValue(ur_device_info_t ParamName,
                                size_t ParamValueSizePI,
                                size_t *ParamValueSizeUR, void *ParamValue) {

  ConvertHelper Value(ParamValueSizePI, ParamValue, ParamValueSizeUR);

  if (ParamName == UR_DEVICE_INFO_TYPE) {
    static std::unordered_map<ur_device_type_t, pi_device_type> Map = {
        {UR_DEVICE_TYPE_CPU, PI_DEVICE_TYPE_CPU},
        {UR_DEVICE_TYPE_GPU, PI_DEVICE_TYPE_GPU},
        {UR_DEVICE_TYPE_FPGA, PI_DEVICE_TYPE_ACC},
    };
    return Value.convert(Map);
  } else if (ParamName == UR_DEVICE_INFO_QUEUE_PROPERTIES) {
    static std::unordered_map<ur_queue_flag_t, pi_queue_properties> Map = {
        {UR_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE,
         PI_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE},
        {UR_QUEUE_FLAG_PROFILING_ENABLE, PI_QUEUE_FLAG_PROFILING_ENABLE},
        {UR_QUEUE_FLAG_ON_DEVICE, PI_QUEUE_FLAG_ON_DEVICE},
        {UR_QUEUE_FLAG_ON_DEVICE_DEFAULT, PI_QUEUE_FLAG_ON_DEVICE_DEFAULT},
    };
    return Value.convertBitSet(Map);
  } else if (ParamName == UR_DEVICE_INFO_EXECUTION_CAPABILITIES) {
    static std::unordered_map<ur_device_exec_capability_flag_t,
                              pi_queue_properties>
        Map = {
            {UR_DEVICE_EXEC_CAPABILITY_FLAG_KERNEL,
             PI_DEVICE_EXEC_CAPABILITIES_KERNEL},
            {UR_DEVICE_EXEC_CAPABILITY_FLAG_NATIVE_KERNEL,
             PI_DEVICE_EXEC_CAPABILITIES_NATIVE_KERNEL},
        };
    return Value.convertBitSet(Map);
  } else if (ParamName == UR_DEVICE_INFO_PARTITION_AFFINITY_DOMAIN) {
    static std::unordered_map<ur_device_affinity_domain_flag_t,
                              pi_device_affinity_domain>
        Map = {
            {UR_DEVICE_AFFINITY_DOMAIN_FLAG_NUMA,
             PI_DEVICE_AFFINITY_DOMAIN_NUMA},
            {UR_DEVICE_AFFINITY_DOMAIN_FLAG_NEXT_PARTITIONABLE,
             PI_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE},
        };
    return Value.convertBitSet(Map);
  } else if (ParamName == UR_DEVICE_INFO_PARTITION_TYPE) {
    static std::unordered_map<ur_device_partition_property_t,
                              pi_device_partition_property>
        Map = {
            {UR_DEVICE_PARTITION_BY_AFFINITY_DOMAIN,
             PI_DEVICE_PARTITION_BY_AFFINITY_DOMAIN},
            {UR_EXT_DEVICE_PARTITION_PROPERTY_FLAG_BY_CSLICE,
             PI_EXT_INTEL_DEVICE_PARTITION_BY_CSLICE},
            {(ur_device_partition_property_t)
                 UR_DEVICE_AFFINITY_DOMAIN_FLAG_NEXT_PARTITIONABLE,
             (pi_device_partition_property)
                 PI_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE},
        };
    return Value.convertArray(Map);
  } else if (ParamName == UR_DEVICE_INFO_PARTITION_PROPERTIES) {
    static std::unordered_map<ur_device_partition_property_t,
                              pi_device_partition_property>
        Map = {
            {UR_DEVICE_PARTITION_BY_AFFINITY_DOMAIN,
             PI_DEVICE_PARTITION_BY_AFFINITY_DOMAIN},
            {UR_EXT_DEVICE_PARTITION_PROPERTY_FLAG_BY_CSLICE,
             PI_EXT_INTEL_DEVICE_PARTITION_BY_CSLICE},
        };
    return Value.convertArray(Map);
  }

  if (ParamValueSizePI && ParamValueSizePI != *ParamValueSizeUR) {
    fprintf(stderr, "UR InfoType=%d PI=%d but UR=%d\n", ParamName,
            (int)ParamValueSizePI, (int)*ParamValueSizeUR);
    die("ur2piInfoValue: size mismatch");
  }
  return PI_SUCCESS;
}

namespace pi2ur {
inline pi_result piPlatformsGet(pi_uint32 num_entries, pi_platform *platforms,
                                pi_uint32 *num_platforms) {

  uint32_t Count = num_entries;
  auto phPlatforms = reinterpret_cast<ur_platform_handle_t *>(platforms);
  HANDLE_ERRORS(urPlatformGet(Count, phPlatforms, num_platforms));
  return PI_SUCCESS;
}

inline pi_result piPlatformGetInfo(pi_platform platform,
                                   pi_platform_info ParamName,
                                   size_t ParamValueSize, void *ParamValue,
                                   size_t *ParamValueSizeRet) {

  static std::unordered_map<pi_platform_info, ur_platform_info_t> InfoMapping =
      {
          {PI_PLATFORM_INFO_EXTENSIONS, UR_PLATFORM_INFO_NAME},
          {PI_PLATFORM_INFO_NAME, UR_PLATFORM_INFO_NAME},
          {PI_PLATFORM_INFO_PROFILE, UR_PLATFORM_INFO_PROFILE},
          {PI_PLATFORM_INFO_VENDOR, UR_PLATFORM_INFO_VENDOR_NAME},
          {PI_PLATFORM_INFO_VERSION, UR_PLATFORM_INFO_VERSION},
      };

  auto InfoType = InfoMapping.find(ParamName);
  if (InfoType == InfoMapping.end()) {
    return PI_ERROR_UNKNOWN;
  }

  size_t SizeInOut = ParamValueSize;
  auto hPlatform = reinterpret_cast<ur_platform_handle_t>(platform);
  HANDLE_ERRORS(urPlatformGetInfo(hPlatform, InfoType->second, SizeInOut,
                                  ParamValue, ParamValueSizeRet));
  return PI_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// Device
inline pi_result piDevicesGet(pi_platform Platform, pi_device_type DeviceType,
                              pi_uint32 NumEntries, pi_device *Devices,
                              pi_uint32 *NumDevices) {

  static std::unordered_map<pi_device_type, ur_device_type_t> TypeMapping = {
      {PI_DEVICE_TYPE_ALL, UR_DEVICE_TYPE_ALL},
      {PI_DEVICE_TYPE_GPU, UR_DEVICE_TYPE_GPU},
      {PI_DEVICE_TYPE_CPU, UR_DEVICE_TYPE_CPU},
      {PI_DEVICE_TYPE_ACC, UR_DEVICE_TYPE_FPGA},
  };

  auto Type = TypeMapping.find(DeviceType);
  if (Type == TypeMapping.end()) {
    return PI_ERROR_UNKNOWN;
  }

  uint32_t Count = NumEntries;
  auto hPlatform = reinterpret_cast<ur_platform_handle_t>(Platform);
  auto phDevices = reinterpret_cast<ur_device_handle_t *>(Devices);
  HANDLE_ERRORS(
      urDeviceGet(hPlatform, Type->second, Count, phDevices, NumDevices));
  return PI_SUCCESS;
}

inline pi_result piDeviceRetain(pi_device Device) {
  auto hDevice = reinterpret_cast<ur_device_handle_t>(Device);
  HANDLE_ERRORS(urDeviceRetain(hDevice));
  return PI_SUCCESS;
}

inline pi_result piDeviceRelease(pi_device Device) {
  auto hDevice = reinterpret_cast<ur_device_handle_t>(Device);
  HANDLE_ERRORS(urDeviceRelease(hDevice));
  return PI_SUCCESS;
}

inline pi_result piPluginGetLastError(char **message) { return PI_SUCCESS; }

inline pi_result piDeviceGetInfo(pi_device Device, pi_device_info ParamName,
                                 size_t ParamValueSize, void *ParamValue,
                                 size_t *ParamValueSizeRet) {

  static std::unordered_map<pi_device_info, ur_device_info_t> InfoMapping = {
      {PI_DEVICE_INFO_TYPE, UR_DEVICE_INFO_TYPE},
      {PI_DEVICE_INFO_PARENT_DEVICE, UR_DEVICE_INFO_PARENT_DEVICE},
      {PI_DEVICE_INFO_PLATFORM, UR_DEVICE_INFO_PLATFORM},
      {PI_DEVICE_INFO_VENDOR_ID, UR_DEVICE_INFO_VENDOR_ID},
      {PI_DEVICE_INFO_UUID, UR_DEVICE_INFO_UUID},
      {PI_DEVICE_INFO_ATOMIC_64, UR_DEVICE_INFO_ATOMIC_64},
      {PI_DEVICE_INFO_EXTENSIONS, UR_DEVICE_INFO_EXTENSIONS},
      {PI_DEVICE_INFO_NAME, UR_DEVICE_INFO_NAME},
      {PI_DEVICE_INFO_COMPILER_AVAILABLE, UR_DEVICE_INFO_COMPILER_AVAILABLE},
      {PI_DEVICE_INFO_LINKER_AVAILABLE, UR_DEVICE_INFO_LINKER_AVAILABLE},
      {PI_DEVICE_INFO_MAX_COMPUTE_UNITS, UR_DEVICE_INFO_MAX_COMPUTE_UNITS},
      {PI_DEVICE_INFO_MAX_WORK_ITEM_DIMENSIONS,
       UR_DEVICE_INFO_MAX_WORK_ITEM_DIMENSIONS},
      {PI_DEVICE_INFO_MAX_WORK_GROUP_SIZE, UR_DEVICE_INFO_MAX_WORK_GROUP_SIZE},
      {PI_DEVICE_INFO_MAX_WORK_ITEM_SIZES, UR_DEVICE_INFO_MAX_WORK_ITEM_SIZES},
      {PI_DEVICE_INFO_MAX_CLOCK_FREQUENCY, UR_DEVICE_INFO_MAX_CLOCK_FREQUENCY},
      {PI_DEVICE_INFO_ADDRESS_BITS, UR_DEVICE_INFO_ADDRESS_BITS},
      {PI_DEVICE_INFO_MAX_MEM_ALLOC_SIZE, UR_DEVICE_INFO_MAX_MEM_ALLOC_SIZE},
      {PI_DEVICE_INFO_GLOBAL_MEM_SIZE, UR_DEVICE_INFO_GLOBAL_MEM_SIZE},
      {PI_DEVICE_INFO_LOCAL_MEM_SIZE, UR_DEVICE_INFO_LOCAL_MEM_SIZE},
      {PI_DEVICE_INFO_IMAGE_SUPPORT, UR_DEVICE_INFO_IMAGE_SUPPORTED},
      {PI_DEVICE_INFO_HOST_UNIFIED_MEMORY, UR_DEVICE_INFO_HOST_UNIFIED_MEMORY},
      {PI_DEVICE_INFO_AVAILABLE, UR_DEVICE_INFO_AVAILABLE},
      {PI_DEVICE_INFO_VENDOR, UR_DEVICE_INFO_VENDOR},
      {PI_DEVICE_INFO_DRIVER_VERSION, UR_DEVICE_INFO_DRIVER_VERSION},
      {PI_DEVICE_INFO_VERSION, UR_DEVICE_INFO_VERSION},
      {PI_DEVICE_INFO_PARTITION_MAX_SUB_DEVICES,
       UR_DEVICE_INFO_PARTITION_MAX_SUB_DEVICES},
      {PI_DEVICE_INFO_REFERENCE_COUNT, UR_DEVICE_INFO_REFERENCE_COUNT},
      {PI_DEVICE_INFO_PARTITION_PROPERTIES,
       UR_DEVICE_INFO_PARTITION_PROPERTIES},
      {PI_DEVICE_INFO_PARTITION_AFFINITY_DOMAIN,
       UR_DEVICE_INFO_PARTITION_AFFINITY_DOMAIN},
      {PI_DEVICE_INFO_PARTITION_TYPE, UR_DEVICE_INFO_PARTITION_TYPE},
      {PI_DEVICE_INFO_OPENCL_C_VERSION, UR_EXT_DEVICE_INFO_OPENCL_C_VERSION},
      {PI_DEVICE_INFO_PREFERRED_INTEROP_USER_SYNC,
       UR_DEVICE_INFO_PREFERRED_INTEROP_USER_SYNC},
      {PI_DEVICE_INFO_PRINTF_BUFFER_SIZE, UR_DEVICE_INFO_PRINTF_BUFFER_SIZE},
      {PI_DEVICE_INFO_PROFILE, UR_DEVICE_INFO_PROFILE},
      {PI_DEVICE_INFO_BUILT_IN_KERNELS, UR_DEVICE_INFO_BUILT_IN_KERNELS},
      {PI_DEVICE_INFO_QUEUE_PROPERTIES, UR_DEVICE_INFO_QUEUE_PROPERTIES},
      {PI_DEVICE_INFO_EXECUTION_CAPABILITIES,
       UR_DEVICE_INFO_EXECUTION_CAPABILITIES},
      {PI_DEVICE_INFO_ENDIAN_LITTLE, UR_DEVICE_INFO_ENDIAN_LITTLE},
      {PI_DEVICE_INFO_ERROR_CORRECTION_SUPPORT,
       UR_DEVICE_INFO_ERROR_CORRECTION_SUPPORT},
      {PI_DEVICE_INFO_PROFILING_TIMER_RESOLUTION,
       UR_DEVICE_INFO_PROFILING_TIMER_RESOLUTION},
      {PI_DEVICE_INFO_LOCAL_MEM_TYPE, UR_DEVICE_INFO_LOCAL_MEM_TYPE},
      {PI_DEVICE_INFO_MAX_CONSTANT_ARGS, UR_DEVICE_INFO_MAX_CONSTANT_ARGS},
      {PI_DEVICE_INFO_MAX_CONSTANT_BUFFER_SIZE,
       UR_DEVICE_INFO_MAX_CONSTANT_BUFFER_SIZE},
      {PI_DEVICE_INFO_GLOBAL_MEM_CACHE_TYPE,
       UR_DEVICE_INFO_GLOBAL_MEM_CACHE_TYPE},
      {PI_DEVICE_INFO_GLOBAL_MEM_CACHELINE_SIZE,
       UR_DEVICE_INFO_GLOBAL_MEM_CACHELINE_SIZE},
      {PI_DEVICE_INFO_GLOBAL_MEM_CACHE_SIZE,
       UR_DEVICE_INFO_GLOBAL_MEM_CACHE_SIZE},
      {PI_DEVICE_INFO_MAX_PARAMETER_SIZE, UR_DEVICE_INFO_MAX_PARAMETER_SIZE},
      {PI_DEVICE_INFO_MEM_BASE_ADDR_ALIGN, UR_DEVICE_INFO_MEM_BASE_ADDR_ALIGN},
      {PI_DEVICE_INFO_MAX_SAMPLERS, UR_DEVICE_INFO_MAX_SAMPLERS},
      {PI_DEVICE_INFO_MAX_READ_IMAGE_ARGS, UR_DEVICE_INFO_MAX_READ_IMAGE_ARGS},
      {PI_DEVICE_INFO_MAX_WRITE_IMAGE_ARGS,
       UR_DEVICE_INFO_MAX_WRITE_IMAGE_ARGS},
      {PI_DEVICE_INFO_SINGLE_FP_CONFIG, UR_DEVICE_INFO_SINGLE_FP_CONFIG},
      {PI_DEVICE_INFO_HALF_FP_CONFIG, UR_DEVICE_INFO_HALF_FP_CONFIG},
      {PI_DEVICE_INFO_DOUBLE_FP_CONFIG, UR_DEVICE_INFO_DOUBLE_FP_CONFIG},
      {PI_DEVICE_INFO_IMAGE2D_MAX_WIDTH, UR_DEVICE_INFO_IMAGE2D_MAX_WIDTH},
      {PI_DEVICE_INFO_IMAGE2D_MAX_HEIGHT, UR_DEVICE_INFO_IMAGE2D_MAX_HEIGHT},
      {PI_DEVICE_INFO_IMAGE3D_MAX_WIDTH, UR_DEVICE_INFO_IMAGE3D_MAX_WIDTH},
      {PI_DEVICE_INFO_IMAGE3D_MAX_HEIGHT, UR_DEVICE_INFO_IMAGE3D_MAX_HEIGHT},
      {PI_DEVICE_INFO_IMAGE3D_MAX_DEPTH, UR_DEVICE_INFO_IMAGE3D_MAX_DEPTH},
      {PI_DEVICE_INFO_IMAGE_MAX_BUFFER_SIZE,
       UR_DEVICE_INFO_IMAGE_MAX_BUFFER_SIZE},
      {PI_DEVICE_INFO_IMAGE_MAX_ARRAY_SIZE,
       (ur_device_info_t)UR_DEVICE_INFO_IMAGE_MAX_ARRAY_SIZE},
      {PI_DEVICE_INFO_NATIVE_VECTOR_WIDTH_CHAR,
       UR_DEVICE_INFO_NATIVE_VECTOR_WIDTH_CHAR},
      {PI_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_CHAR,
       UR_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_CHAR},
      {PI_DEVICE_INFO_NATIVE_VECTOR_WIDTH_SHORT,
       UR_DEVICE_INFO_NATIVE_VECTOR_WIDTH_SHORT},
      {PI_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_SHORT,
       UR_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_SHORT},
      {PI_DEVICE_INFO_NATIVE_VECTOR_WIDTH_INT,
       UR_DEVICE_INFO_NATIVE_VECTOR_WIDTH_INT},
      {PI_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_INT,
       UR_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_INT},
      {PI_DEVICE_INFO_NATIVE_VECTOR_WIDTH_LONG,
       UR_DEVICE_INFO_NATIVE_VECTOR_WIDTH_LONG},
      {PI_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_LONG,
       UR_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_LONG},
      {PI_DEVICE_INFO_NATIVE_VECTOR_WIDTH_FLOAT,
       UR_DEVICE_INFO_NATIVE_VECTOR_WIDTH_FLOAT},
      {PI_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_FLOAT,
       UR_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_FLOAT},
      {PI_DEVICE_INFO_NATIVE_VECTOR_WIDTH_DOUBLE,
       UR_DEVICE_INFO_NATIVE_VECTOR_WIDTH_DOUBLE},
      {PI_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_DOUBLE,
       UR_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_DOUBLE},
      {PI_DEVICE_INFO_NATIVE_VECTOR_WIDTH_HALF,
       UR_DEVICE_INFO_NATIVE_VECTOR_WIDTH_HALF},
      {PI_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_HALF,
       UR_DEVICE_INFO_PREFERRED_VECTOR_WIDTH_HALF},
      {PI_DEVICE_INFO_MAX_NUM_SUB_GROUPS, UR_DEVICE_INFO_MAX_NUM_SUB_GROUPS},
      {PI_DEVICE_INFO_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS,
       UR_DEVICE_INFO_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS},
      {PI_DEVICE_INFO_SUB_GROUP_SIZES_INTEL,
       UR_DEVICE_INFO_SUB_GROUP_SIZES_INTEL},
      {PI_DEVICE_INFO_IL_VERSION, UR_DEVICE_INFO_IL_VERSION},
      {PI_DEVICE_INFO_USM_HOST_SUPPORT, UR_DEVICE_INFO_USM_HOST_SUPPORT},
      {PI_DEVICE_INFO_USM_DEVICE_SUPPORT, UR_DEVICE_INFO_USM_DEVICE_SUPPORT},
      {PI_DEVICE_INFO_USM_SINGLE_SHARED_SUPPORT,
       UR_DEVICE_INFO_USM_SINGLE_SHARED_SUPPORT},
      {PI_DEVICE_INFO_USM_CROSS_SHARED_SUPPORT,
       UR_DEVICE_INFO_USM_CROSS_SHARED_SUPPORT},
      {PI_DEVICE_INFO_USM_SYSTEM_SHARED_SUPPORT,
       UR_DEVICE_INFO_USM_SYSTEM_SHARED_SUPPORT},
      {PI_DEVICE_INFO_USM_HOST_SUPPORT, UR_DEVICE_INFO_USM_HOST_SUPPORT},
      {PI_DEVICE_INFO_USM_DEVICE_SUPPORT, UR_DEVICE_INFO_USM_DEVICE_SUPPORT},
      {PI_DEVICE_INFO_USM_SINGLE_SHARED_SUPPORT,
       UR_DEVICE_INFO_USM_SINGLE_SHARED_SUPPORT},
      {PI_DEVICE_INFO_USM_CROSS_SHARED_SUPPORT,
       UR_DEVICE_INFO_USM_CROSS_SHARED_SUPPORT},
      {PI_DEVICE_INFO_USM_SYSTEM_SHARED_SUPPORT,
       UR_DEVICE_INFO_USM_SYSTEM_SHARED_SUPPORT},
      {PI_DEVICE_INFO_PCI_ADDRESS, UR_DEVICE_INFO_PCI_ADDRESS},
      {PI_DEVICE_INFO_GPU_EU_COUNT, UR_DEVICE_INFO_GPU_EU_COUNT},
      {PI_DEVICE_INFO_GPU_EU_SIMD_WIDTH, UR_DEVICE_INFO_GPU_EU_SIMD_WIDTH},
      {PI_DEVICE_INFO_GPU_SUBSLICES_PER_SLICE,
       UR_DEVICE_INFO_GPU_SUBSLICES_PER_SLICE},
      {PI_DEVICE_INFO_BUILD_ON_SUBDEVICE,
       (ur_device_info_t)UR_EXT_DEVICE_INFO_BUILD_ON_SUBDEVICE},
      {PI_EXT_ONEAPI_DEVICE_INFO_MAX_WORK_GROUPS_3D,
       (ur_device_info_t)UR_EXT_DEVICE_INFO_MAX_WORK_GROUPS_3D},
      {PI_DEVICE_INFO_IMAGE_MAX_ARRAY_SIZE,
       (ur_device_info_t)UR_DEVICE_INFO_IMAGE_MAX_ARRAY_SIZE},
      {PI_DEVICE_INFO_DEVICE_ID, (ur_device_info_t)UR_DEVICE_INFO_DEVICE_ID},
      {PI_EXT_INTEL_DEVICE_INFO_FREE_MEMORY,
       (ur_device_info_t)UR_EXT_DEVICE_INFO_FREE_MEMORY},
      {PI_EXT_INTEL_DEVICE_INFO_MEMORY_CLOCK_RATE,
       (ur_device_info_t)UR_DEVICE_INFO_MEMORY_CLOCK_RATE},
      {PI_EXT_INTEL_DEVICE_INFO_MEMORY_BUS_WIDTH,
       (ur_device_info_t)UR_EXT_DEVICE_INFO_MEMORY_BUS_WIDTH},
      {PI_EXT_INTEL_DEVICE_INFO_MAX_COMPUTE_QUEUE_INDICES,
       (ur_device_info_t)UR_DEVICE_INFO_MAX_COMPUTE_QUEUE_INDICES},
      {PI_DEVICE_INFO_GPU_SLICES,
       (ur_device_info_t)UR_EXT_DEVICE_INFO_GPU_SLICES},
      {PI_DEVICE_INFO_GPU_EU_COUNT_PER_SUBSLICE,
       (ur_device_info_t)UR_EXT_DEVICE_INFO_GPU_EU_COUNT_PER_SUBSLICE},
      {PI_DEVICE_INFO_GPU_HW_THREADS_PER_EU,
       (ur_device_info_t)UR_EXT_DEVICE_INFO_GPU_HW_THREADS_PER_EU},
      {PI_DEVICE_INFO_MAX_MEM_BANDWIDTH,
       (ur_device_info_t)UR_EXT_DEVICE_INFO_MAX_MEM_BANDWIDTH},
      {PI_EXT_ONEAPI_DEVICE_INFO_BFLOAT16_MATH_FUNCTIONS,
       (ur_device_info_t)UR_DEVICE_INFO_BFLOAT16},
      {PI_DEVICE_INFO_ATOMIC_MEMORY_SCOPE_CAPABILITIES,
       (ur_device_info_t)UR_DEVICE_INFO_ATOMIC_MEMORY_SCOPE_CAPABILITIES},
  };

  auto InfoType = InfoMapping.find(ParamName);
  if (InfoType == InfoMapping.end()) {
    return PI_ERROR_UNKNOWN;
  }

  size_t SizeInOut = ParamValueSize;
  auto hDevice = reinterpret_cast<ur_device_handle_t>(Device);
  HANDLE_ERRORS(urDeviceGetInfo(hDevice, InfoType->second, SizeInOut,
                                ParamValue, ParamValueSizeRet));

  ur2piInfoValue(InfoType->second, ParamValueSize, &SizeInOut, ParamValue);

  return PI_SUCCESS;
}

inline pi_result piDevicePartition(
    pi_device Device, const pi_device_partition_property *Properties,
    pi_uint32 NumEntries, pi_device *SubDevices, pi_uint32 *NumSubDevices) {

  if (!Properties || !Properties[0])
    return PI_ERROR_INVALID_VALUE;

  static std::unordered_map<pi_device_partition_property,
                            ur_device_partition_property_t>
      PropertyMap = {
          {PI_DEVICE_PARTITION_EQUALLY, UR_DEVICE_PARTITION_EQUALLY},
          {PI_DEVICE_PARTITION_BY_COUNTS, UR_DEVICE_PARTITION_BY_COUNTS},
          {PI_DEVICE_PARTITION_BY_AFFINITY_DOMAIN,
           UR_DEVICE_PARTITION_BY_AFFINITY_DOMAIN},
          {PI_EXT_INTEL_DEVICE_PARTITION_BY_CSLICE,
           UR_EXT_DEVICE_PARTITION_PROPERTY_FLAG_BY_CSLICE},
      };

  auto PropertyIt = PropertyMap.find(Properties[0]);
  if (PropertyIt == PropertyMap.end()) {
    return PI_ERROR_UNKNOWN;
  }

  // Some partitioning types require a value
  auto Value = uint32_t(Properties[1]);
  if (PropertyIt->second == UR_DEVICE_PARTITION_BY_AFFINITY_DOMAIN) {
    static std::unordered_map<pi_device_affinity_domain,
                              ur_device_affinity_domain_flag_t>
        ValueMap = {
            {PI_DEVICE_AFFINITY_DOMAIN_NUMA,
             UR_DEVICE_AFFINITY_DOMAIN_FLAG_NUMA},
            {PI_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE,
             UR_DEVICE_AFFINITY_DOMAIN_FLAG_NEXT_PARTITIONABLE},
        };
    auto ValueIt = ValueMap.find(Properties[1]);
    if (ValueIt == ValueMap.end()) {
      return PI_ERROR_UNKNOWN;
    }
    Value = ValueIt->second;
  }

  // Translate partitioning properties from PI-way
  // (array of uintptr_t values) to UR-way
  // (array of {uint32_t, uint32_t} pairs)
  //
  // TODO: correctly terminate the UR properties, see:
  // https://github.com/oneapi-src/unified-runtime/issues/183
  //
  ur_device_partition_property_t UrProperties[] = {
      ur_device_partition_property_t(PropertyIt->second), Value, 0};

  auto hDevice = reinterpret_cast<ur_device_handle_t>(Device);
  auto phSubDevices = reinterpret_cast<ur_device_handle_t *>(SubDevices);
  HANDLE_ERRORS(urDevicePartition(hDevice, UrProperties, NumEntries, phSubDevices,
                                  NumSubDevices));
  return PI_SUCCESS;
}

inline pi_result piGetDeviceAndHostTimer(pi_device Device, uint64_t *DeviceTime,
                                  uint64_t *HostTime) {
  auto hDevice = reinterpret_cast<ur_device_handle_t>(Device);
  HANDLE_ERRORS(urDeviceGetGlobalTimestamps(hDevice, DeviceTime, HostTime));
  return PI_SUCCESS;
}

inline  pi_result
piextDeviceSelectBinary(pi_device Device, // TODO: does this need to be context?
                        pi_device_binary *Binaries, pi_uint32 NumBinaries,
                        pi_uint32 *SelectedBinaryInd) {

  auto hDevice = reinterpret_cast<ur_device_handle_t>(Device);
  const uint8_t **ppBinaries = const_cast<const uint8_t **>(reinterpret_cast<uint8_t **>(Binaries));
  HANDLE_ERRORS(urDeviceSelectBinary(hDevice,
                                     ppBinaries,
                                     NumBinaries,
                                     SelectedBinaryInd));
  return PI_SUCCESS;
}

// Device
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Context
inline pi_result piContextCreate(const pi_context_properties *Properties,
                          pi_uint32 NumDevices, const pi_device *Devices,
                          void (*PFnNotify)(const char *ErrInfo,
                                            const void *PrivateInfo, size_t CB,
                                            void *UserData),
                          void *UserData, pi_context *RetContext) {
  uint32_t DeviceCount = reinterpret_cast<uint32_t>(NumDevices);
  ur_device_handle_t *phDevices = reinterpret_cast<ur_device_handle_t *>(const_cast<pi_device *>(Devices));
  // ur_context_handle_t *phContext = reinterpret_cast<ur_context_handle_t *>(RetContext);

  ur_context_handle_t UrContext;  
  HANDLE_ERRORS(urContextCreate(DeviceCount, phDevices, &UrContext));

  printf("%s %d UrContext %lx\n", __FILE__, __LINE__, (unsigned long int)UrContext);
  
  try {
    // _pi_context *Context = new _pi_context(*phContext);
    // *RetContext = reinterpret_cast<pi_context>(Context);
    _pi_context *PiContext = new _pi_context(UrContext);
    *RetContext = reinterpret_cast<pi_context>(PiContext);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  // printf("%s %d  *RetContext %lx PiContext %lx UrContext %lx\n",
  //   __FILE__, __LINE__, (unsigned long int) *RetContext, (unsigned long int)PiContext,
  //   (unsigned long int)UrContext);


  return PI_SUCCESS;
}

inline pi_result piContextGetInfo(pi_context Context, pi_context_info ParamName,
                           size_t ParamValueSize, void *ParamValue,
                           size_t *ParamValueSizeRet) {

  ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);
  ur_context_info_t ContextInfoType{};
  (urContextGetInfo(hContext, ContextInfoType, ParamValueSize, ParamValue, ParamValueSizeRet));

  return PI_SUCCESS;
}

inline pi_result piContextRetain(pi_context Context) {
  ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);

  (urContextRetain(hContext));

  return PI_SUCCESS;
}


inline pi_result piContextRelease(pi_context Context) {
  ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);

  (urContextRelease(hContext));

  return PI_SUCCESS;
}
// Context
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Queue
inline pi_result piQueueCreate(pi_context Context, pi_device Device,
                        pi_queue_properties Flags, pi_queue *Queue) {
  printf("%s %d\n", __FILE__, __LINE__);

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_device_handle_t hDevice = reinterpret_cast<ur_device_handle_t>(Device);
  ur_queue_property_t props {};
  ur_queue_handle_t *phQueue = reinterpret_cast<ur_queue_handle_t *>(Queue);

  printf("%s %d Queue %lx PiContext %lx Context %lx\n",
    __FILE__, __LINE__, (unsigned long int)Queue, (unsigned long int)PiContext,
    (unsigned long int)PiContext->UrContext);

  HANDLE_ERRORS(urQueueCreate(PiContext->UrContext, 
                              hDevice,
                              &props,
                              phQueue));
  return PI_SUCCESS;
}

inline pi_result piextQueueCreate(pi_context Context, pi_device Device,
                           pi_queue_properties *Properties, pi_queue *Queue) {

  PI_ASSERT(Properties, PI_ERROR_INVALID_VALUE);
  // Expect flags mask to be passed first.
  PI_ASSERT(Properties[0] == PI_QUEUE_FLAGS, PI_ERROR_INVALID_VALUE);

  PI_ASSERT(Properties[2] == 0 ||
                (Properties[2] == PI_QUEUE_COMPUTE_INDEX && Properties[4] == 0),
            PI_ERROR_INVALID_VALUE);

  // Check that unexpected bits are not set.
  PI_ASSERT(
      !(Properties[1] & ~(PI_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE |
                  PI_QUEUE_FLAG_PROFILING_ENABLE | PI_QUEUE_FLAG_ON_DEVICE |
                  PI_QUEUE_FLAG_ON_DEVICE_DEFAULT |
                  PI_EXT_ONEAPI_QUEUE_FLAG_DISCARD_EVENTS |
                  PI_EXT_ONEAPI_QUEUE_FLAG_PRIORITY_LOW |
                  PI_EXT_ONEAPI_QUEUE_FLAG_PRIORITY_HIGH)),
      PI_ERROR_INVALID_VALUE);

  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);
  PI_ASSERT(Device, PI_ERROR_INVALID_DEVICE);

  ur_queue_property_t props [5] {};
  props[0] = UR_QUEUE_PROPERTIES_FLAGS;
  if (Properties[1] & PI_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE)
    props[1] |= UR_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE;
  if (Properties[1] & PI_QUEUE_FLAG_PROFILING_ENABLE)
    props[1] |= UR_QUEUE_FLAG_PROFILING_ENABLE;
  if (Properties[1] & PI_QUEUE_FLAG_ON_DEVICE)
    props[1] |= UR_QUEUE_FLAG_ON_DEVICE;
  if (Properties[1] & PI_QUEUE_FLAG_ON_DEVICE_DEFAULT)
    props[1] |= UR_QUEUE_FLAG_ON_DEVICE_DEFAULT;
  if (Properties[1] & PI_EXT_ONEAPI_QUEUE_FLAG_DISCARD_EVENTS)
    props[1] |= UR_QUEUE_FLAG_DISCARD_EVENTS;
  if (Properties[1] & PI_EXT_ONEAPI_QUEUE_FLAG_PRIORITY_LOW)
    props[1] |= UR_QUEUE_FLAG_PRIORITY_LOW;
  if (Properties[1] & PI_EXT_ONEAPI_QUEUE_FLAG_PRIORITY_HIGH)
    props[1] |= UR_QUEUE_FLAG_PRIORITY_HIGH;

  if (Properties[2] != 0) {
    props[2] = UR_QUEUE_PROPERTIES_COMPUTE_INDEX;
    props[3] = Properties[3];
  }

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);

  ur_device_handle_t hDevice = reinterpret_cast<ur_device_handle_t>(Device);
  ur_queue_handle_t *phQueue = reinterpret_cast<ur_queue_handle_t *>(Queue);
  // ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);

// printf("%s %d Queue %lx PiContext %lx Context %lx hContext %lx\n",
//     __FILE__, __LINE__, (unsigned long int)Queue, (unsigned long int)PiContext,
//     (unsigned long int)PiContext->UrContext, (unsigned long int)hContext);

  HANDLE_ERRORS(urQueueCreate(PiContext->UrContext, 
                              hDevice,
                              props,
                              phQueue));

  printf("%s %d Queue %lx PiContext %lx UrContext %lx\n",
    __FILE__, __LINE__, (unsigned long int)Queue, (unsigned long int)PiContext,
    (unsigned long int)PiContext->UrContext);
  // printf("%s %d UrContext %lx\n", __FILE__, __LINE__,
  //   (unsigned long int)hContext);
  // if (hContext) {
  //   printf("%s %d Context->Devices[0] %lx\n", __FILE__, __LINE__, (unsigned long int)Context->Devices[0]);
  //   if (Context->Devices[0]) {
  //     printf("%s %d Context->Devices[0]->ZeDevice %lx\n", __FILE__, __LINE__, (unsigned long int)Context->Devices[0]->ZeDevice);
  //   }
  // }

  return PI_SUCCESS;
}

inline pi_result piQueueRelease(pi_queue Queue) {
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  ur_queue_handle_t hQueue = reinterpret_cast<ur_queue_handle_t>(Queue);
  
  HANDLE_ERRORS(urQueueRelease(hQueue));

  return PI_SUCCESS;
}

// Queue
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Program

inline pi_result piProgramCreate(pi_context Context, const void *ILBytes,
                          size_t Length, pi_program *Program) {

  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  PI_ASSERT(ILBytes && Length, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);

  try {
    _pi_program * PiProgram = new _pi_program(_pi_program::IL,
                                              reinterpret_cast<_pi_context *>(Context),
                                              ILBytes,
                                              Length);
    *Program = reinterpret_cast<pi_program>(PiProgram);

    printf("%s %d *Program %lx\n", __FILE__, __LINE__, (unsigned long int)*Program);

  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }
  return PI_SUCCESS;
}

inline pi_result piextProgramSetSpecializationConstant(pi_program ProgramIn,
                                                pi_uint32 SpecID, size_t Size,
                                                const void *SpecValue) {
  _pi_program *Program = reinterpret_cast<_pi_program *>(ProgramIn);                                                  

  std::scoped_lock<pi_shared_mutex> Guard(Program->Mutex);

  // Remember the value of this specialization constant until the program is
  // built.  Note that we only save the pointer to the buffer that contains the
  // value.  The caller is responsible for maintaining storage for this buffer.
  //
  // NOTE: SpecSize is unused in Level Zero, the size is known from SPIR-V by
  // SpecID.
  Program->SpecConstants[SpecID] = SpecValue;

  return PI_SUCCESS;
}

inline pi_result piProgramBuild(pi_program ProgramIn, pi_uint32 NumDevices,
                         const pi_device *DeviceList, const char *Options,
                         void (*PFnNotify)(pi_program Program, void *UserData),
                         void *UserData) {
  PI_ASSERT(ProgramIn, PI_ERROR_INVALID_PROGRAM);
  printf("%s %d\n", __FILE__, __LINE__);
  if ((NumDevices && !DeviceList) || (!NumDevices && DeviceList))
    return PI_ERROR_INVALID_VALUE;

  printf("%s %d\n", __FILE__, __LINE__);

  _pi_program *Program = reinterpret_cast<_pi_program *>(ProgramIn);

#if 0
  // We only support build to one device with Level Zero now.
  // TODO: we should eventually build to the possibly multiple root
  // devices in the context.
  if (NumDevices != 1) {
    zePrint("piProgramBuild: level_zero supports only one device.");
    return PI_ERROR_INVALID_VALUE;
  }
#endif

  // These aren't supported.
  PI_ASSERT(!PFnNotify && !UserData, PI_ERROR_INVALID_VALUE);
  printf("%s %d\n", __FILE__, __LINE__);
  std::scoped_lock<pi_shared_mutex> Guard(Program->Mutex);
  // Check if device belongs to associated context.
  PI_ASSERT(Program->Context, PI_ERROR_INVALID_PROGRAM);
  // PI_ASSERT(Program->Context->isValidDevice(DeviceList[0]),
  //           PI_ERROR_INVALID_VALUE);

#if 0
  // It is legal to build a program created from either IL or from native
  // device code.
  if (Program->State != _pi_program::IL &&
      Program->State != _pi_program::Native)
    return PI_ERROR_INVALID_OPERATION;
#endif

#if 0
  // We should have either IL or native device code.
  PI_ASSERT(Program->Code, PI_ERROR_INVALID_PROGRAM);
#endif

#if 0
  // Ask Level Zero to build and load the native code onto the device.
  // ZeStruct<ze_module_desc_t> ZeModuleDesc;
  _pi_program::SpecConstantShim Shim(Program);
  ZeModuleDesc.format = (Program->State == _pi_program::IL)
                            ? ZE_MODULE_FORMAT_IL_SPIRV
                            : ZE_MODULE_FORMAT_NATIVE;
#endif

#if 0
  for (auto SpecTuple : Program->SpecConstants) {
    HANDLE_ERRORS(urProgramSetSpecializationConstant(Program->UrProgram,
                                                     SpecTuple.first,
                                                     sizeof(void *),
                                                     SpecTuple.second));
  }
#endif
  printf("%s %d\n", __FILE__, __LINE__);
  // ze_device_handle_t ZeDevice = DeviceList[0]->ZeDevice;
  ur_context_handle_t UrContext = Program->Context->UrContext;
  ur_module_handle_t UrModule = nullptr;

  printf("%s %d Program->Context->UrContext %lx\n", __FILE__, __LINE__,
    (unsigned long int)Program->Context->UrContext);

  pi_result Result = PI_SUCCESS;
  Program->State = _pi_program::Exe;
  HANDLE_ERRORS(urModuleCreate(UrContext,
                               Program->Code.get(),
                               Program->CodeLength,
                               Options,
                               nullptr,
                               nullptr,
                               &UrModule));
  printf("%s %d\n", __FILE__, __LINE__);
  
    printf("%s %d Program->UrModules.size() %zd\n", __FILE__, __LINE__, Program->UrModules.size());
  
    Program->UrModules.resize(1u);
    printf("%s %d Program->UrModules.size() %zd\n", __FILE__, __LINE__, Program->UrModules.size());
    Program->UrModules[0] = UrModule;
    printf("%s %d\n", __FILE__, __LINE__);
    ur_program_handle_t UrProgram = {};
    HANDLE_ERRORS(urProgramCreate(UrContext,
                                  1u,
                                  &UrModule,
                                  nullptr,
                                  &UrProgram));

    Program->UrProgram = UrProgram;

    printf("%s %d PiProgram %lx UrProgram %lx\n", __FILE__, __LINE__,
      (unsigned long int)Program,
      (unsigned long int)Program->UrProgram);

#if 0
  if (ZeResult != ZE_RESULT_SUCCESS) {
    // We adjust pi_program below to avoid attempting to release zeModule when
    // RT calls piProgramRelease().
    ZeModule = nullptr;
    Program->State = _pi_program::Invalid;
    Result = mapError(ZeResult);
  } else
#endif

#if 0
  {
    // The call to zeModuleCreate does not report an error if there are
    // unresolved symbols because it thinks these could be resolved later via a
    // call to zeModuleDynamicLink.  However, modules created with
    // piProgramBuild are supposed to be fully linked and ready to use.
    // Therefore, do an extra check now for unresolved symbols.
    ZeResult = checkUnresolvedSymbols(ZeModule, &Program->ZeBuildLog);
    if (ZeResult != ZE_RESULT_SUCCESS) {
      Program->State = _pi_program::Invalid;
      Result = (ZeResult == ZE_RESULT_ERROR_MODULE_LINK_FAILURE)
                   ? PI_ERROR_BUILD_PROGRAM_FAILURE
                   : mapError(ZeResult);
    }
  }
#endif
printf("%s %d\n", __FILE__, __LINE__);
  // We no longer need the IL / native code.
  Program->Code.reset();
  Program->UrModule = UrModule;
  Program->UrDevice = reinterpret_cast<ur_device_handle_t>(DeviceList[0]);

  printf("%s %d Program %lx UrProgram %lx\n", __FILE__, __LINE__,
      (unsigned long int)Program,
      (unsigned long int)Program->UrProgram);

  return Result;
}

inline pi_result piProgramLink(pi_context Context,
                               pi_uint32 NumDevices,
                               const pi_device *DeviceList,
                               const char *Options,
                               pi_uint32 NumInputPrograms,
                               const pi_program *InputPrograms,
                               void (*PFnNotify)(pi_program Program, void *UserData),
                               void *UserData,
                               pi_program *RetProgram) {

  // We only support one device with Level Zero currently.
  if (NumDevices != 1) {
    printf("piProgramLink: level_zero supports only one device.");
    return PI_ERROR_INVALID_VALUE;
  }

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);

  // We do not support any link flags at this time because the Level Zero API
  // does not have any way to pass flags that are specific to linking.
  if (Options && *Options != '\0') {
    std::string ErrorMessage(
        "Level Zero does not support kernel link flags: \"");
    ErrorMessage.append(Options);
    ErrorMessage.push_back('\"');
    _pi_program *PiProgram =
        new _pi_program(_pi_program::Invalid, PiContext, ErrorMessage);
    *RetProgram = reinterpret_cast<pi_program>(PiProgram);
    printf("%s %d *RetProgram %lx\n", __FILE__, __LINE__, (unsigned long int)*RetProgram);
    return PI_ERROR_LINK_PROGRAM_FAILURE;
  }

#if 0
  // Validate input parameters.
  PI_ASSERT(DeviceList, PI_ERROR_INVALID_DEVICE);
  // PI_ASSERT(Context->isValidDevice(DeviceList[0]), PI_ERROR_INVALID_DEVICE);
  PI_ASSERT(!PFnNotify && !UserData, PI_ERROR_INVALID_VALUE);
#endif
  if (NumInputPrograms == 0 || InputPrograms == nullptr)
    return PI_ERROR_INVALID_VALUE;

  pi_result Result = PI_SUCCESS;
  try {
    // Acquire a "shared" lock on each of the input programs, and also validate
    // that they are all in Object state.
    //
    // There is no danger of deadlock here even if two threads call
    // piProgramLink simultaneously with the same input programs in a different
    // order.  If we were acquiring these with "exclusive" access, this could
    // lead to a classic lock ordering deadlock.  However, there is no such
    // deadlock potential with "shared" access.  There could also be a deadlock
    // potential if there was some other code that holds more than one of these
    // locks simultaneously with "exclusive" access.  However, there is no such
    // code like that, so this is also not a danger.
    std::vector<std::shared_lock<pi_shared_mutex>> Guards(NumInputPrograms);
    for (pi_uint32 I = 0; I < NumInputPrograms; I++) {
      _pi_program *PiProgram = reinterpret_cast<_pi_program *>(InputPrograms[I]);
      std::shared_lock<pi_shared_mutex> Guard(PiProgram->Mutex);
      Guards[I].swap(Guard);
      if (PiProgram->State != _pi_program::Object) {
        printf("%s %d\n", __FILE__, __LINE__);
        return PI_ERROR_INVALID_OPERATION;
      }
    }

    // Previous calls to piProgramCompile did not actually compile the SPIR-V.
    // Instead, we postpone compilation until this point, when all the modules
    // are linked together.  By doing compilation and linking together, the JIT
    // compiler is able see all modules and do cross-module optimizations.
    //
#if 0
    std::vector<const ze_module_constants_t *> SpecConstPtrs(NumInputPrograms);
    std::vector<_pi_program::SpecConstantShim> SpecConstShims;
    SpecConstShims.reserve(NumInputPrograms);

    for (pi_uint32 I = 0; I < NumInputPrograms; I++) {
      pi_program Program = InputPrograms[I];
      CodeSizes[I] = Program->CodeLength;
      CodeBufs[I] = Program->Code.get();
      BuildFlagPtrs[I] = Program->BuildFlags.c_str();
      SpecConstShims.emplace_back(Program);
      SpecConstPtrs[I] = SpecConstShims[I].ze();
    }
#endif

#if 0
    // We need a Level Zero extension to compile multiple programs together into
    // a single Level Zero module.  However, we don't need that extension if
    // there happens to be only one input program.
    //
    // The "|| (NumInputPrograms == 1)" term is a workaround for a bug in the
    // Level Zero driver.  The driver's "ze_module_program_exp_desc_t"
    // extension should work even in the case when there is just one input
    // module.  However, there is currently a bug in the driver that leads to a
    // crash.  As a workaround, do not use the extension when there is one
    // input module.
    //
    // TODO: Remove this workaround when the driver is fixed.
    if (DeviceList[0]->Platform->ZeDriverModuleProgramExtensionFound &&
        (NumInputPrograms != 1)) {
      printf("piProgramLink: level_zero driver does not have static linking "
              "support.");
      printf("%s %d\n", __FILE__, __LINE__);
      return PI_ERROR_INVALID_VALUE;
    }
#endif

    ur_context_handle_t UrContext = PiContext->UrContext;

    std::vector<ur_module_handle_t> UrModules(NumInputPrograms);
    for (uint32_t i = 0 ; i < NumInputPrograms; i++) {
      _pi_program *Program = reinterpret_cast<_pi_program *>(InputPrograms[i]);
      UrModules[i] = Program->UrModule;
    }

    _pi_program * PiProgram = nullptr;
    try {
      _pi_program *PiProgram = reinterpret_cast<_pi_program *>(InputPrograms[0]);
      PiProgram = new _pi_program(_pi_program::state::Exe,
                                  PiContext,
                                  PiProgram->Code.get(),
                                  PiProgram->CodeLength);
      *RetProgram = reinterpret_cast<pi_program>(PiProgram);
      printf("%s %d *RetProgram %lx\n", __FILE__, __LINE__, (unsigned long int)*RetProgram);
    } catch (const std::bad_alloc &) {
      return PI_ERROR_OUT_OF_HOST_MEMORY;
    } catch (...) {
      return PI_ERROR_UNKNOWN;
    }

    PiProgram->UrModules.resize(NumInputPrograms);

    for (uint32_t i = 0 ; i < NumInputPrograms; i++) {
      PiProgram->UrModules[i] = UrModules[i];
    }

    ur_program_handle_t UrProgram = {};
    HANDLE_ERRORS(urProgramCreate(UrContext,
                                  NumInputPrograms,
                                  UrModules.data(),
                                  nullptr,
                                  &UrProgram));

    PiProgram->UrProgram = UrProgram;

    printf("%s %d PiProgram %lx UrProgram %lx\n", __FILE__, __LINE__,
      (unsigned long int)PiProgram,
      (unsigned long int)PiProgram->UrProgram);

#if 0
    // We still create a _pi_program object even if there is a BUILD_FAILURE
    // because we need the object to hold the ZeBuildLog.  There is no build
    // log created for other errors, so we don't create an object.
    Result = mapError(ZeResult);
    if (ZeResult != ZE_RESULT_SUCCESS &&
        ZeResult != ZE_RESULT_ERROR_MODULE_BUILD_FAILURE) {
      return Result;
    }
#endif
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return Result;
}

inline pi_result piKernelCreate(pi_program ProgramIn, const char *KernelName,
                         pi_kernel *RetKernel) {
  PI_ASSERT(ProgramIn, PI_ERROR_INVALID_PROGRAM);
  PI_ASSERT(RetKernel, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(KernelName, PI_ERROR_INVALID_VALUE);

  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(ProgramIn);

  ur_kernel_handle_t UrKernel {};  
  HANDLE_ERRORS(urKernelCreate(PiProgram->UrProgram, KernelName, &UrKernel));
  try {
    _pi_kernel *PiKernel = new _pi_kernel(UrKernel);
    *RetKernel = reinterpret_cast<pi_kernel>(PiKernel);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
}

// Special version of piKernelSetArg to accept pi_mem.
inline pi_result piextKernelSetArgMemObj(pi_kernel Kernel, pi_uint32 ArgIndex,
                                  const pi_mem *ArgValue) {

  // TODO: the better way would probably be to add a new PI API for
  // extracting native PI object from PI handle, and have SYCL
  // RT pass that directly to the regular piKernelSetArg (and
  // then remove this piextKernelSetArgMemObj).

  PI_ASSERT(Kernel, PI_ERROR_INVALID_KERNEL);

  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);

  HANDLE_ERRORS(urKernelSetArgValue(PiKernel->UrKernel,
                                    ArgIndex,
                                    sizeof(ArgValue),
                                    ArgValue));
  return PI_SUCCESS;
}

inline pi_result
piextKernelCreateWithNativeHandle(pi_native_handle NativeHandle,
                                  pi_context Context,
                                  pi_program Program,
                                  bool OwnNativeHandle,
                                  pi_kernel *Kernel) {
  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Kernel, PI_ERROR_INVALID_KERNEL);

  ur_native_handle_t hNativeKernel = reinterpret_cast<ur_native_handle_t>(NativeHandle);
  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;
  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(Program);
  ur_kernel_handle_t UrKernel {};
  HANDLE_ERRORS(urKernelCreateWithNativeHandle(hNativeKernel,
                                               UrContext,
                                               &UrKernel));

  try {
    _pi_kernel *PiKernel = new _pi_kernel(reinterpret_cast<char *>(NativeHandle),
                                          OwnNativeHandle);
    PiKernel->PiProgram = PiProgram;
    PiKernel->UrKernel = UrKernel;
    *Kernel = reinterpret_cast<pi_kernel>(PiKernel);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
}


// Program
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Memory
inline pi_result piMemBufferCreate(pi_context Context, pi_mem_flags Flags, size_t Size,
                            void *HostPtr, pi_mem *RetMem,
                            const pi_mem_properties *properties) {

  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  PI_ASSERT(RetMem, PI_ERROR_INVALID_VALUE);

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;
  ur_mem_flags_t UrBufferFlags {};
  ur_mem_handle_t UrBuffer {};
  HANDLE_ERRORS(urMemBufferCreate(UrContext,
                                  UrBufferFlags,
                                  Size,
                                  HostPtr,
                                  &UrBuffer));

  *RetMem = reinterpret_cast<pi_mem>(UrBuffer);

  return PI_SUCCESS;
}

inline pi_result piextUSMHostAlloc(void **ResultPtr, pi_context Context,
                            pi_usm_mem_properties *Properties, size_t Size,
                            pi_uint32 Alignment) {

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;
  ur_usm_mem_flags_t UrUsmFlags {};
  HANDLE_ERRORS(urUSMHostAlloc(UrContext,
                               &UrUsmFlags,
                               Size,
                               Alignment,
                               ResultPtr));
  return PI_SUCCESS;
}

inline pi_result piMemGetInfo(pi_mem Mem,
                              pi_mem_info ParamName,
                              size_t ParamValueSize,
                              void *ParamValue,
                              size_t *ParamValueSizeRet) {  
  _pi_mem *PiMem = reinterpret_cast<_pi_mem *>(Mem);
  PI_ASSERT(PiMem, PI_ERROR_INVALID_VALUE);
  // piMemImageGetInfo must be used for images
#if 0
  PI_ASSERT(!PiMem->   ->isImage(), PI_ERROR_INVALID_VALUE);
#endif
  
  ur_mem_handle_t UrMemory = PiMem->UrMemory;
  ur_mem_info_t MemInfoType {};
  if (ParamName == PI_MEM_CONTEXT)
    MemInfoType =  UR_MEM_INFO_CONTEXT;
  else if (ParamName == PI_MEM_SIZE)
    MemInfoType =  UR_MEM_INFO_SIZE;
  HANDLE_ERRORS(urMemGetInfo(UrMemory,
                               MemInfoType,
                               ParamValueSize,
                               ParamValue,
                               ParamValueSizeRet));
  return PI_SUCCESS;
}

inline pi_result piMemImageCreate(pi_context Context,
                                  pi_mem_flags Flags,
                                  const pi_image_format *ImageFormat,
                                  const pi_image_desc *ImageDesc,
                                  void *HostPtr,
                                  pi_mem *RetImage) {

  // TODO: implement read-only, write-only
  if ((Flags & PI_MEM_FLAGS_ACCESS_RW) == 0) {
    die("piMemImageCreate: Level-Zero implements only read-write buffer,"
        "no read-only or write-only yet.");
  }
  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  PI_ASSERT(RetImage, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(ImageFormat, PI_ERROR_INVALID_IMAGE_FORMAT_DESCRIPTOR);

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;

  ur_mem_flags_t UrFlags {};
  ur_image_format_t UrFormat {};
  ur_image_desc_t UrDesc {};
  ur_mem_handle_t UrMem {};
  HANDLE_ERRORS(urMemImageCreate(UrContext,
                                 UrFlags,
                                 &UrFormat,
                                 &UrDesc,
                                 HostPtr,
                                 &UrMem));

  _pi_mem *Image = new _pi_mem(UrContext);
  Image->UrMemory = UrMem;

  *RetImage = reinterpret_cast<pi_mem>(Image);

  return PI_SUCCESS;
}

inline pi_result piMemBufferPartition(pi_mem Buffer,
                                      pi_mem_flags Flags,
                                      pi_buffer_create_type BufferCreateType,
                                      void *BufferCreateInfo,
                                      pi_mem *RetMem) {
#if 0
  PI_ASSERT(Buffer && !Buffer->isImage() &&
                !(static_cast<pi_buffer>(Buffer))->isSubBuffer(),
            PI_ERROR_INVALID_MEM_OBJECT);

  PI_ASSERT(BufferCreateType == PI_BUFFER_CREATE_TYPE_REGION &&
                BufferCreateInfo && RetMem,
            PI_ERROR_INVALID_VALUE);

  PI_ASSERT(Region->size != 0u, PI_ERROR_INVALID_BUFFER_SIZE);
  PI_ASSERT(Region->origin <= (Region->origin + Region->size),
            PI_ERROR_INVALID_VALUE);
#endif

  _pi_mem *PiBuffer = reinterpret_cast<_pi_mem *>(Buffer);
  ur_mem_handle_t UrBuffer = PiBuffer->UrMemory;
  ur_mem_flags_t UrFlags {};
  ur_buffer_create_type_t UrBufferCreateType {};
  ur_buffer_region_t UrBufferCreateInfo {};
  ur_mem_handle_t UrMem {};
  HANDLE_ERRORS(urMemBufferPartition(UrBuffer,
                                     UrFlags,
                                     UrBufferCreateType,
                                     &UrBufferCreateInfo,
                                     &UrMem));

  try {
    _pi_mem *PiMem = new _pi_mem(PiBuffer->UrContext);
    PiMem->UrMemory = UrMem;
    *RetMem = reinterpret_cast<pi_mem>(PiMem);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS; 
}

inline pi_result piextMemGetNativeHandle(pi_mem Mem,
                                         pi_native_handle *NativeHandle) {
  PI_ASSERT(Mem, PI_ERROR_INVALID_MEM_OBJECT);

  _pi_mem *PiMem = reinterpret_cast<_pi_mem *>(Mem);

  ur_mem_handle_t UrMem = PiMem->UrMemory;
  ur_native_handle_t NativeMem {};
  HANDLE_ERRORS(urMemGetNativeHandle(UrMem,
                                     &NativeMem));

  *NativeHandle = reinterpret_cast<pi_native_handle>(NativeMem);

  return PI_SUCCESS; 
}

inline pi_result
piEnqueueMemImageCopy(pi_queue Queue, pi_mem SrcImage, pi_mem DstImage,
                      pi_image_offset SrcOrigin, pi_image_offset DstOrigin,
                      pi_image_region Region, pi_uint32 NumEventsInWaitList,
                      const pi_event *EventWaitList, pi_event *Event) {
  
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  // _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  // ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  ur_queue_handle_t UrQueue = reinterpret_cast<ur_queue_handle_t>(Queue);

  _pi_mem *PiSrcImage = reinterpret_cast<_pi_mem *>(SrcImage);
  ur_mem_handle_t UrImageSrc = PiSrcImage->UrMemory;

  _pi_mem *PiDstImage = reinterpret_cast<_pi_mem *>(DstImage);
  ur_mem_handle_t UrImageDst = PiDstImage->UrMemory;

  ur_rect_offset_t UrSrcOrigin {SrcOrigin->x, SrcOrigin->y, SrcOrigin->z};
  ur_rect_offset_t UrDstOrigin {DstOrigin->x, DstOrigin->y, DstOrigin->z};
  ur_rect_region_t UrRegion {Region->width, Region->depth, Region->height};
  uint32_t UrNumEventsInWaitList = reinterpret_cast<uint32_t>(NumEventsInWaitList);
  const ur_event_handle_t *UrEventWaitList = reinterpret_cast<const ur_event_handle_t *>(EventWaitList);
  ur_event_handle_t *UrEvent = reinterpret_cast<ur_event_handle_t *>(Event);
  
  HANDLE_ERRORS(urEnqueueMemImageCopy(UrQueue,
                                      UrImageSrc,
                                      UrImageDst,
                                      UrSrcOrigin,
                                      UrDstOrigin,
                                      UrRegion,
                                      UrNumEventsInWaitList,
                                      UrEventWaitList,
                                      UrEvent));
  return PI_SUCCESS;
}

inline pi_result
piextMemCreateWithNativeHandle(pi_native_handle NativeHandle,
                               pi_context Context,
                               bool OwnNativeHandle,
                               pi_mem *Mem) {
  PI_ASSERT(Mem, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);

  ur_native_handle_t UrNativeMem = reinterpret_cast<ur_native_handle_t>(NativeHandle);
  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;
  ur_mem_handle_t UrMem = {};
  HANDLE_ERRORS(urMemCreateWithNativeHandle(UrNativeMem,
                                            UrContext,
                                            &UrMem));

  ur_mem_info_t UrMemInfoType = UR_MEM_INFO_SIZE;
  size_t Size = 0;
  HANDLE_ERRORS(urMemGetInfo(UrMem,
                             UrMemInfoType,
                             sizeof(Size),
                             &Size,
                             nullptr));

  try {
    _pi_mem *PiMem = new _pi_mem(UrContext,
                                 Size,
                                 reinterpret_cast<char *>(NativeHandle),
                                 OwnNativeHandle);
    *Mem = reinterpret_cast<pi_mem>(PiMem);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
}

// Memory
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
inline pi_result
piEnqueueKernelLaunch(pi_queue Queue,
                      pi_kernel Kernel,
                      pi_uint32 WorkDim,
                      const size_t *GlobalWorkOffset,
                      const size_t *GlobalWorkSize,
                      const size_t *LocalWorkSize,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventWaitList,
                      pi_event *OutEvent) {

  PI_ASSERT(Kernel, PI_ERROR_INVALID_KERNEL);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);
  PI_ASSERT((WorkDim > 0) && (WorkDim < 4), PI_ERROR_INVALID_WORK_DIMENSION);

  ur_queue_handle_t hQueue = reinterpret_cast<ur_queue_handle_t>(Queue);
  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Queue);
  ur_kernel_handle_t hKernel = PiKernel->UrKernel;
  uint32_t workDim = reinterpret_cast<uint32_t>(WorkDim);
  uint32_t numEventsInWaitList = reinterpret_cast<uint32_t>(NumEventsInWaitList);
  const ur_event_handle_t *phEventWaitList = reinterpret_cast<const ur_event_handle_t *>(EventWaitList);
  ur_event_handle_t *phEvent = reinterpret_cast<ur_event_handle_t *>(OutEvent);

  HANDLE_ERRORS(urEnqueueKernelLaunch(hQueue,
                                      hKernel,
                                      workDim,
                                      GlobalWorkOffset,
                                      GlobalWorkSize,
                                      LocalWorkSize,
                                      numEventsInWaitList,
                                      phEventWaitList,
                                      phEvent));

  return PI_SUCCESS;

}

inline pi_result
piEnqueueMemImageWrite(pi_queue Queue, pi_mem Image,
                       pi_bool BlockingWrite, pi_image_offset Origin,
                       pi_image_region Region, size_t InputRowPitch,
                       size_t InputSlicePitch, const void *Ptr,
                       pi_uint32 NumEventsInWaitList,
                       const pi_event *EventWaitList,
                       pi_event *Event) {

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  ur_queue_handle_t hQueue = reinterpret_cast<ur_queue_handle_t>(Queue);
  _pi_mem *PiMem = reinterpret_cast<_pi_mem *>(Image);
  ur_mem_handle_t hImage = PiMem->UrMemory;
  ur_rect_offset_t UrOrigin {Origin->x, Origin->y, Origin->z};
  ur_rect_region_t UrRegion {Region->width, Region->depth, Region->height};
  uint32_t numEventsInWaitList = reinterpret_cast<uint32_t>(NumEventsInWaitList);
  const ur_event_handle_t *phEventWaitList = reinterpret_cast<const ur_event_handle_t *>(EventWaitList);
  ur_event_handle_t *phEvent = reinterpret_cast<ur_event_handle_t *>(Event);
  HANDLE_ERRORS(urEnqueueMemImageWrite(hQueue,
                                       hImage,
                                       BlockingWrite,
                                       UrOrigin,
                                       UrRegion,
                                       InputRowPitch,
                                       InputSlicePitch,
                                       const_cast<void *>(Ptr),
                                       numEventsInWaitList,
                                       phEventWaitList,
                                       phEvent));

  return PI_SUCCESS;
}

inline pi_result
piEnqueueMemImageRead(pi_queue Queue, pi_mem Image,
                      pi_bool BlockingRead, pi_image_offset Origin,
                      pi_image_region Region, size_t RowPitch,
                      size_t SlicePitch, void *Ptr,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventWaitList,
                      pi_event *Event) {
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  ur_queue_handle_t hQueue = reinterpret_cast<ur_queue_handle_t>(Queue);
  _pi_mem *PiMem = reinterpret_cast<_pi_mem *>(Image);
  ur_mem_handle_t hImage = PiMem->UrMemory;
  ur_rect_offset_t UrOrigin {Origin->x, Origin->y, Origin->z};
  ur_rect_region_t UrRegion {Region->width, Region->depth, Region->height};
  uint32_t numEventsInWaitList = reinterpret_cast<uint32_t>(NumEventsInWaitList);
  const ur_event_handle_t *phEventWaitList = reinterpret_cast<const ur_event_handle_t *>(EventWaitList);
  ur_event_handle_t *phEvent = reinterpret_cast<ur_event_handle_t *>(Event);
  HANDLE_ERRORS(urEnqueueMemImageRead(hQueue,
                                      hImage,
                                      BlockingRead,
                                      UrOrigin,
                                      UrRegion,
                                      RowPitch,
                                      SlicePitch,
                                      Ptr,
                                      numEventsInWaitList,
                                      phEventWaitList,
                                      phEvent));

  return PI_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace pi2ur
