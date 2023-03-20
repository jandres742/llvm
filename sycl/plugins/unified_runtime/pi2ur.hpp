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

#include "ur/adapters/level_zero/ur_level_zero_common.hpp"
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

inline pi_result
piextPlatformGetNativeHandle(pi_platform Platform,
                             pi_native_handle *NativeHandle) {

  PI_ASSERT(Platform, PI_ERROR_INVALID_PLATFORM);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  auto UrPlatform = reinterpret_cast<ur_platform_handle_t>(Platform);

  ur_native_handle_t UrNativeHandle {};
  HANDLE_ERRORS(urPlatformGetNativeHandle(UrPlatform, &UrNativeHandle));

  *NativeHandle = reinterpret_cast<pi_native_handle>(UrNativeHandle);

  return PI_SUCCESS;
}

inline pi_result
piextPlatformCreateWithNativeHandle(pi_native_handle NativeHandle,
                                    pi_platform *Platform) {

  PI_ASSERT(Platform, PI_ERROR_INVALID_PLATFORM);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  ur_platform_handle_t UrPlatform {};
  ur_native_handle_t UrNativeHandle = reinterpret_cast<ur_native_handle_t>(NativeHandle);
  urPlatformCreateWithNativeHandle(UrNativeHandle,
                                   &UrPlatform);
                                   
  *Platform = reinterpret_cast<pi_platform>(UrPlatform);

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
inline pi_result
piextPluginGetOpaqueData(void *opaque_data_param,
                         void **opaque_data_return) {
  (void)opaque_data_param;
  (void)opaque_data_return;
  return PI_ERROR_UNKNOWN;
}
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Device
inline pi_result piDevicesGet(pi_platform Platform,
                              pi_device_type DeviceType,
                              pi_uint32 NumEntries,
                              pi_device *Devices,
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
  auto UrPlatform = reinterpret_cast<ur_platform_handle_t>(Platform);

  std::vector<ur_device_handle_t> UrDevices(Count);
  HANDLE_ERRORS(urDeviceGet(UrPlatform,
                            Type->second,
                            Count,
                            UrDevices.data(),
                            NumDevices));

  for (uint32_t DeviceCount = 0; DeviceCount < Count; DeviceCount++){
    _pi_device *PiDevice = nullptr;
    try {
      PiDevice = new _pi_device(UrDevices[DeviceCount]);
    } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
    } catch (...) {
      return PI_ERROR_UNKNOWN;
    }
    Devices[DeviceCount] = reinterpret_cast<pi_device>(PiDevice);
  }

  return PI_SUCCESS;
}

inline pi_result piDeviceRetain(pi_device Device) {
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;
  HANDLE_ERRORS(urDeviceRetain(UrDevice));
  return PI_SUCCESS;
}

inline pi_result piDeviceRelease(pi_device Device) {
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;
  HANDLE_ERRORS(urDeviceRelease(UrDevice));
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
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;

  HANDLE_ERRORS(urDeviceGetInfo(UrDevice,
                                InfoType->second, SizeInOut,
                                ParamValue,
                                ParamValueSizeRet));

  ur2piInfoValue(InfoType->second, ParamValueSize, &SizeInOut, ParamValue);

  return PI_SUCCESS;
}

inline pi_result
piextDeviceGetNativeHandle(pi_device Device,
                           pi_native_handle *NativeHandle) {
  PI_ASSERT(Device, PI_ERROR_INVALID_DEVICE);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;

  ur_native_handle_t UrNativeHandle {};
  HANDLE_ERRORS(urDeviceGetNativeHandle(UrDevice,
                                        &UrNativeHandle));
  *NativeHandle = reinterpret_cast<pi_native_handle>(UrNativeHandle);
  return PI_SUCCESS;
}

inline pi_result
piextDeviceCreateWithNativeHandle(pi_native_handle NativeHandle,
                                  pi_platform Platform,
                                  pi_device *Device) {

  PI_ASSERT(Device, PI_ERROR_INVALID_DEVICE);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  ur_native_handle_t UrNativeDevice = reinterpret_cast<ur_native_handle_t>(NativeHandle);
  ur_platform_handle_t UrPlatform = reinterpret_cast<ur_platform_handle_t>(Platform);
  ur_device_handle_t UrDevice {};
  HANDLE_ERRORS(urDeviceCreateWithNativeHandle(UrNativeDevice,
                                               UrPlatform,
                                               &UrDevice));

  try {
    _pi_device *PiDevice = new _pi_device(UrDevice);
    *Device = reinterpret_cast<pi_device>(PiDevice);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
}

inline pi_result piDevicePartition(pi_device Device,
                                   const pi_device_partition_property *Properties,
                                   pi_uint32 NumEntries,
                                   pi_device *SubDevices,
                                   pi_uint32 *NumSubDevices) {

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

  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
    ur_device_handle_t UrDevice = PiDevice->UrDevice;

  std::vector<ur_device_handle_t> UrSubDevices{};
  for (uint32_t SubDeviceCount = 0; SubDeviceCount < NumEntries; SubDeviceCount++){
    _pi_device *PiSubDevice = reinterpret_cast<_pi_device *>(SubDevices[SubDeviceCount]);
    ur_device_handle_t UrSubDevice = PiSubDevice->UrDevice;
    UrSubDevices.push_back(UrSubDevice);
  }

  HANDLE_ERRORS(urDevicePartition(UrDevice,
                                  UrProperties,
                                  NumEntries,
                                  UrSubDevices.data(),
                                  NumSubDevices));
  return PI_SUCCESS;
}

inline pi_result
piGetDeviceAndHostTimer(pi_device Device,
                        uint64_t *DeviceTime,
                        uint64_t *HostTime) {
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;
  HANDLE_ERRORS(urDeviceGetGlobalTimestamps(UrDevice,
                                            DeviceTime,
                                            HostTime));
  return PI_SUCCESS;
}

inline  pi_result
piextDeviceSelectBinary(pi_device Device, // TODO: does this need to be context?
                        pi_device_binary *Binaries,
                        pi_uint32 NumBinaries,
                        pi_uint32 *SelectedBinaryInd) {

  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;
  const uint8_t **UrBinaries = const_cast<const uint8_t **>(reinterpret_cast<uint8_t **>(Binaries));
  HANDLE_ERRORS(urDeviceSelectBinary(UrDevice,
                                     UrBinaries,
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

  std::vector<ur_device_handle_t> UrDevices{};
  for (uint32_t DeviceCount = 0; DeviceCount < NumDevices; DeviceCount++){
    _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Devices[DeviceCount]);
    ur_device_handle_t UrDevice = PiDevice->UrDevice;
    UrDevices.push_back(UrDevice);
  }

  ur_context_handle_t UrContext;  
  HANDLE_ERRORS(urContextCreate(DeviceCount,
                                UrDevices.data(),
                                &UrContext));
  
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

  // printf("%s %d *RetContext %lx\n", __FILE__, __LINE__, (unsigned long int)*RetContext);

  // printf("%s %d  *RetContext %lx PiContext %lx UrContext %lx\n",
  //   __FILE__, __LINE__, (unsigned long int) *RetContext, (unsigned long int)PiContext,
  //   (unsigned long int)UrContext);


  return PI_SUCCESS;
}

// FIXME: Dummy implementation to prevent link fail
inline pi_result
piextContextSetExtendedDeleter(pi_context Context,
                               pi_context_extended_deleter Function,
                               void *UserData) {
  std::ignore = Context;
  std::ignore = Function;
  std::ignore = UserData;
  die("piextContextSetExtendedDeleter: not supported");
  return PI_SUCCESS;
}

inline pi_result
piextContextGetNativeHandle(pi_context Context,
                            pi_native_handle *NativeHandle) {

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;

  ur_native_handle_t UrNativeHandle {};
  HANDLE_ERRORS(urContextGetNativeHandle(UrContext,
                                        &UrNativeHandle));
  *NativeHandle = reinterpret_cast<pi_native_handle>(UrNativeHandle);
  return PI_SUCCESS;

}


inline pi_result
piextContextCreateWithNativeHandle(pi_native_handle NativeHandle,
                                   pi_uint32 NumDevices,
                                   const pi_device *Devices,
                                   bool OwnNativeHandle,
                                   pi_context *RetContext) {
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Devices, PI_ERROR_INVALID_DEVICE);
  PI_ASSERT(RetContext, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(NumDevices, PI_ERROR_INVALID_VALUE);

  ur_native_handle_t NativeContext = reinterpret_cast<ur_native_handle_t>(NativeHandle);
  ur_context_handle_t UrContext {};
  HANDLE_ERRORS(urContextCreateWithNativeHandle(NativeContext,
                                                &UrContext));

  try {
    _pi_context *PiContext = new _pi_context(UrContext,
                                             NumDevices,
                                             Devices,
                                             OwnNativeHandle);
    *RetContext = reinterpret_cast<pi_context>(PiContext);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
}


inline pi_result
piContextGetInfo(pi_context Context,
                 pi_context_info ParamName,
                 size_t ParamValueSize,
                 void *ParamValue,
                 size_t *ParamValueSizeRet) {

  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);

  ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);
  ur_context_info_t ContextInfoType{};
  
  switch (ParamName) {
    case PI_CONTEXT_INFO_DEVICES: {
      ContextInfoType = UR_CONTEXT_INFO_DEVICES;
      break;
    }
    case PI_CONTEXT_INFO_PLATFORM: {
      die("urGetContextInfo: unsuppported ParamName.");
    }
    case PI_CONTEXT_INFO_NUM_DEVICES: {
      ContextInfoType = UR_CONTEXT_INFO_NUM_DEVICES;
      break;
    }
    case PI_CONTEXT_INFO_PROPERTIES: {
      die("urGetContextInfo: unsuppported ParamName.");
    }
    case PI_CONTEXT_INFO_REFERENCE_COUNT: {
      ContextInfoType = UR_EXT_CONTEXT_INFO_REFERENCE_COUNT;
      break;
    }
    case PI_CONTEXT_INFO_ATOMIC_MEMORY_ORDER_CAPABILITIES: {
      die("urGetContextInfo: unsuppported ParamName.");
    }
    case PI_EXT_ONEAPI_CONTEXT_INFO_USM_FILL2D_SUPPORT: {
      ContextInfoType = UR_CONTEXT_INFO_USM_FILL2D_SUPPORT;
      break;
    }
    case PI_EXT_ONEAPI_CONTEXT_INFO_USM_MEMSET2D_SUPPORT: {
      ContextInfoType = UR_CONTEXT_INFO_USM_MEMSET2D_SUPPORT;
      break;
    }
    case PI_EXT_ONEAPI_CONTEXT_INFO_USM_MEMCPY2D_SUPPORT: {
      ContextInfoType = UR_CONTEXT_INFO_USM_MEMCPY2D_SUPPORT;
      break;
    }
    default: {
      die("urGetContextInfo: unsuppported ParamName.");
    }
  }

  HANDLE_ERRORS(urContextGetInfo(hContext,
                                 ContextInfoType,
                                 ParamValueSize,
                                 ParamValue,
                                 ParamValueSizeRet));
  return PI_SUCCESS;
}

inline pi_result
piContextRetain(pi_context Context) {
  ur_context_handle_t hContext = reinterpret_cast<ur_context_handle_t>(Context);

  HANDLE_ERRORS(urContextRetain(hContext));

  return PI_SUCCESS;
}


inline pi_result
piContextRelease(pi_context Context) {
  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;

  // printf("%s %d hContext %lx\n", __FILE__, __LINE__, (unsigned long int)UrContext);

  HANDLE_ERRORS(urContextRelease(UrContext));

  return PI_SUCCESS;
}
// Context
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Queue
inline pi_result
piQueueCreate(pi_context Context,
              pi_device Device,
              pi_queue_properties Flags,
              pi_queue *Queue) {
  // printf("%s %d\n", __FILE__, __LINE__);

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;
  ur_queue_property_t Props {};
  ur_queue_handle_t UrQueue {};

  // printf("%s %d Queue %lx PiContext %lx Context %lx\n",
    // __FILE__, __LINE__, (unsigned long int)Queue, (unsigned long int)PiContext,
    // (unsigned long int)PiContext->UrContext);

  HANDLE_ERRORS(urQueueCreate(UrContext, 
                              UrDevice,
                              &Props,
                              &UrQueue));
  
  try {
    _pi_queue *PiQueue = new _pi_queue(UrContext,
                                       UrQueue,
                                       Device,
                                       true);
    *Queue = reinterpret_cast<pi_queue>(PiQueue);
  } catch (const std::bad_alloc &) {
      return PI_ERROR_OUT_OF_RESOURCES;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }
  
  return PI_SUCCESS;
}

inline pi_result
piextQueueCreate(pi_context Context,
                 pi_device Device,
                 pi_queue_properties *Properties,
                 pi_queue *Queue) {

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
  ur_context_handle_t UrContext = PiContext->UrContext;
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;
  
  ur_queue_handle_t UrQueue {};

  HANDLE_ERRORS(urQueueCreate(UrContext, 
                              UrDevice,
                              props,
                              &UrQueue));

  try {
    _pi_queue *PiQueue = new _pi_queue(UrContext,
                                       UrQueue,
                                       Device,
                                       true);
    *Queue = reinterpret_cast<pi_queue>(PiQueue);
  } catch (const std::bad_alloc &) {
      return PI_ERROR_OUT_OF_RESOURCES;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
}

inline pi_result
piextQueueCreateWithNativeHandle(pi_native_handle NativeHandle,
                                 pi_context Context,
                                 pi_device Device,
                                 bool OwnNativeHandle,
                                 pi_queue *Queue) {
  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);
  PI_ASSERT(Device, PI_ERROR_INVALID_DEVICE);

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;

  ur_native_handle_t UrNativeHandle = reinterpret_cast<ur_native_handle_t>(NativeHandle);
  ur_queue_handle_t UrQueue {};
  HANDLE_ERRORS(urQueueCreateWithNativeHandle(UrNativeHandle,
                                              UrContext,
                                              &UrQueue));

  try {
    _pi_queue *PiQueue = new _pi_queue(UrContext,
                                      UrQueue,
                                      Device,
                                      OwnNativeHandle);
    *Queue = reinterpret_cast<pi_queue>(PiQueue);
  } catch (const std::bad_alloc &) {
      return PI_ERROR_OUT_OF_RESOURCES;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }
      
  return PI_SUCCESS;
}

inline pi_result
piextQueueGetNativeHandle(pi_queue Queue,
                          pi_native_handle *NativeHandle) {

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;

  ur_native_handle_t UrNativeQueue {};
  HANDLE_ERRORS(urQueueGetNativeHandle(UrQueue,
                                      &UrNativeQueue));

  *NativeHandle = reinterpret_cast<pi_native_handle>(UrNativeQueue);

  return PI_SUCCESS;
}

inline pi_result
piQueueRelease(pi_queue Queue) {
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  
  HANDLE_ERRORS(urQueueRelease(UrQueue));

  return PI_SUCCESS;
}

inline pi_result
piQueueFinish(pi_queue Queue) {
  // Wait until command lists attached to the command queue are executed.
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;

  HANDLE_ERRORS(urQueueFinish(UrQueue));

  return PI_SUCCESS;
}

inline pi_result
piQueueGetInfo(pi_queue Queue,
               pi_queue_info ParamName,
               size_t ParamValueSize,
               void *ParamValue,
               size_t *ParamValueSizeRet) {
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;

  ur_queue_info_t UrParamName {};

  switch (ParamName) {
    case PI_QUEUE_INFO_CONTEXT: {
      UrParamName = UR_QUEUE_INFO_CONTEXT;
      break;
    }
    case PI_QUEUE_INFO_DEVICE: {
      UrParamName = UR_QUEUE_INFO_DEVICE;
      break;
    }
    case PI_QUEUE_INFO_DEVICE_DEFAULT: {
      UrParamName = UR_QUEUE_INFO_DEVICE_DEFAULT;
      break;
    }
    case PI_QUEUE_INFO_PROPERTIES: {
      UrParamName = UR_QUEUE_INFO_PROPERTIES;
      break;
    }
    case PI_QUEUE_INFO_REFERENCE_COUNT: {
      UrParamName = UR_QUEUE_INFO_REFERENCE_COUNT;
      break;
    }
    case PI_QUEUE_INFO_SIZE: {
      UrParamName = UR_QUEUE_INFO_SIZE;
      break;
    }
    case PI_EXT_ONEAPI_QUEUE_INFO_EMPTY: {
      UrParamName = UR_EXT_ONEAPI_QUEUE_INFO_EMPTY;
      break;
    }
    default: {
      printf("Unsupported ParamName in piQueueGetInfo: ParamName=%d(0x%x)\n",
              ParamName, ParamName);
      return PI_ERROR_INVALID_VALUE;
    }
  }

  HANDLE_ERRORS(urQueueGetInfo(UrQueue,
                               UrParamName,
                               ParamValueSize,
                               ParamValue,
                               ParamValueSizeRet));

  return PI_SUCCESS;
}

inline pi_result
piQueueRetain(pi_queue Queue) {

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;

  HANDLE_ERRORS(urQueueRetain(UrQueue));

  return PI_SUCCESS;
}

inline pi_result
piQueueFlush(pi_queue Queue) {

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;

  HANDLE_ERRORS(urQueueFlush(UrQueue));

  return PI_SUCCESS;
}

// Queue
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Program

inline pi_result piProgramCreate(pi_context Context,
                                 const void *ILBytes,
                                 size_t Length,
                                 pi_program *Program) {

  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  PI_ASSERT(ILBytes && Length, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;

  ur_program_properties_t UrProperties {};
  ur_program_handle_t UrProgram {};
  HANDLE_ERRORS(urProgramCreateWithIL(UrContext,
                                      ILBytes,
                                      Length,
                                      &UrProperties,
                                      &UrProgram));

  try {
    _pi_program * PiProgram = new _pi_program(_pi_program::IL,
                                              reinterpret_cast<_pi_context *>(Context),
                                              ILBytes,
                                              Length);
    PiProgram->UrProgram = UrProgram;
    *Program = reinterpret_cast<pi_program>(PiProgram);
    // printf("%s %d PiProgram %lx\n", __FILE__, __LINE__, (unsigned long int)PiProgram);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }
  return PI_SUCCESS;
}

inline pi_result
piProgramCreateWithBinary(pi_context Context,
                          pi_uint32 NumDevices,
                          const pi_device *DeviceList,
                          const size_t *Lengths,
                          const unsigned char **Binaries,
                          size_t NumMetadataEntries,
                          const pi_device_binary_property *Metadata,
                          pi_int32 *BinaryStatus,
                          pi_program *Program) {
  std::ignore = Metadata;
  std::ignore = NumMetadataEntries;

  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  PI_ASSERT(DeviceList && NumDevices, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Binaries && Lengths, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);

  // For now we support only one device.
  if (NumDevices != 1) {
    printf("piProgramCreateWithBinary: level_zero supports only one device.");
    return PI_ERROR_INVALID_VALUE;
  }
  if (!Binaries[0] || !Lengths[0]) {
    if (BinaryStatus)
      *BinaryStatus = PI_ERROR_INVALID_VALUE;
    return PI_ERROR_INVALID_VALUE;
  }
  
  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(DeviceList[0]);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;

  ur_program_handle_t UrProgram {};
  HANDLE_ERRORS(urProgramCreateWithBinary(UrContext,
                                          UrDevice,
                                          Lengths[0],
                                          Binaries[0],
                                          &UrProgram));

  try {
    _pi_program *PiProgram = new _pi_program(_pi_program::state::Native,
                                             PiContext,
                                             Binaries[0],
                                             Lengths[0]);
    PiProgram->UrProgram = UrProgram;
    *Program = reinterpret_cast<pi_program>(PiProgram);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  if (BinaryStatus)
    *BinaryStatus = PI_SUCCESS;

  return PI_SUCCESS;
}

inline pi_result
piclProgramCreateWithSource(pi_context Context,
                            pi_uint32 Count,
                            const char **Strings,
                            const size_t *Lengths,
                            pi_program *RetProgram) {
  std::ignore = Context;
  std::ignore = Count;
  std::ignore = Strings;
  std::ignore = Lengths;
  std::ignore = RetProgram;
  urPrint("piclProgramCreateWithSource: not supported in UR\n");
  return PI_ERROR_INVALID_OPERATION;
}

inline pi_result
piProgramGetInfo(pi_program Program,
                 pi_program_info ParamName,
                 size_t ParamValueSize,
                 void *ParamValue,
                 size_t *ParamValueSizeRet) {

  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);
  
  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(Program);
  ur_program_handle_t UrProgram = PiProgram->UrProgram;

  ur_program_info_t PropName {};

  switch(ParamName) {
    case PI_PROGRAM_INFO_REFERENCE_COUNT: {
      PropName = UR_PROGRAM_INFO_REFERENCE_COUNT;
      break;
    }
    case PI_PROGRAM_INFO_CONTEXT: {
      PropName = UR_PROGRAM_INFO_CONTEXT;
      break;
    }
    case PI_PROGRAM_INFO_NUM_DEVICES: {
      PropName = UR_PROGRAM_INFO_NUM_DEVICES;
      break;
    }
    case PI_PROGRAM_INFO_DEVICES: {
      PropName = UR_PROGRAM_INFO_DEVICES;
      break;
    }
    case PI_PROGRAM_INFO_SOURCE: {
      PropName = UR_PROGRAM_INFO_SOURCE;
      break;
    }
    case PI_PROGRAM_INFO_BINARY_SIZES: {
      PropName = UR_PROGRAM_INFO_BINARY_SIZES;
      break;
    }
    case PI_PROGRAM_INFO_BINARIES: {
      PropName = UR_PROGRAM_INFO_BINARIES;
      break;
    }
    case PI_PROGRAM_INFO_NUM_KERNELS: {
      PropName = UR_PROGRAM_INFO_NUM_KERNELS;
      break;
    }
    case PI_PROGRAM_INFO_KERNEL_NAMES: {
      PropName = UR_PROGRAM_INFO_KERNEL_NAMES;
      break;
    }
    default: {
      die("urProgramGetInfo: not implemented");
    }
  }

  HANDLE_ERRORS(urProgramGetInfo(UrProgram,
                                 PropName,
                                 ParamValueSize,
                                 ParamValue,
                                 ParamValueSizeRet));

  return PI_SUCCESS;
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
    urPrint("piProgramLink: level_zero supports only one device.");
    return PI_ERROR_INVALID_VALUE;
  }

  // Validate input parameters.
  PI_ASSERT(DeviceList, PI_ERROR_INVALID_DEVICE);
  // PI_ASSERT(Context->isValidDevice(DeviceList[0]), PI_ERROR_INVALID_DEVICE);
  PI_ASSERT(!PFnNotify && !UserData, PI_ERROR_INVALID_VALUE);
  if (NumInputPrograms == 0 || InputPrograms == nullptr)
    return PI_ERROR_INVALID_VALUE;

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;

  std::vector<ur_program_handle_t> UrPrograms(NumInputPrograms);
  for (uint32_t i = 0 ; i < NumInputPrograms; i++) {
    _pi_program *Program = reinterpret_cast<_pi_program *>(InputPrograms[i]);
    UrPrograms[i] = Program->UrProgram;
  }

  ur_program_handle_t UrProgram = {};
  HANDLE_ERRORS(urProgramLink(UrContext,
                                NumInputPrograms,
                                UrPrograms.data(),
                                Options,
                                &UrProgram));

  return PI_SUCCESS;
}

inline pi_result
piProgramCompile(pi_program Program,
                 pi_uint32 NumDevices,
                 const pi_device *DeviceList,
                 const char *Options,
                 pi_uint32 NumInputHeaders,
                 const pi_program *InputHeaders,
                 const char **HeaderIncludeNames,
                 void (*PFnNotify)(pi_program Program, void *UserData),
                 void *UserData) {

  std::ignore = NumInputHeaders;
  std::ignore = InputHeaders;
  std::ignore = HeaderIncludeNames;

  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);

  if ((NumDevices && !DeviceList) || (!NumDevices && DeviceList))
    return PI_ERROR_INVALID_VALUE;

  // These aren't supported.
  PI_ASSERT(!PFnNotify && !UserData, PI_ERROR_INVALID_VALUE);

  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(Program);
  ur_program_handle_t UrProgram = PiProgram->UrProgram;
  ur_context_handle_t UrContext = PiProgram->Context->UrContext;

  HANDLE_ERRORS(urProgramCompile(UrContext,
                                 UrProgram,
                                 Options));

  return PI_SUCCESS;
}

inline pi_result piProgramBuild(pi_program Program,
                                pi_uint32 NumDevices,
                                const pi_device *DeviceList,
                                const char *Options,
                                void (*PFnNotify)(pi_program Program, void *UserData),
                                void *UserData) {
  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);
  if ((NumDevices && !DeviceList) || (!NumDevices && DeviceList))
    return PI_ERROR_INVALID_VALUE;

  // We only support build to one device with Level Zero now.
  // TODO: we should eventually build to the possibly multiple root
  // devices in the context.
  if (NumDevices != 1) {
    urPrint("piProgramBuild: level_zero supports only one device.");
    return PI_ERROR_INVALID_VALUE;
  }

  // These aren't supported.
  PI_ASSERT(!PFnNotify && !UserData, PI_ERROR_INVALID_VALUE);

  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(Program);

  // Check if device belongs to associated context.
  PI_ASSERT(PiProgram->Context, PI_ERROR_INVALID_PROGRAM);
  // PI_ASSERT(PiProgram->Context->isValidDevice(DeviceList[0]),
  //           PI_ERROR_INVALID_VALUE); // TODO

  // It is legal to build a program created from either IL or from native
  // device code.
  if (PiProgram->State != _pi_program::IL &&
      PiProgram->State != _pi_program::Native)
    return PI_ERROR_INVALID_OPERATION;

  // We should have either IL or native device code.
  PI_ASSERT(PiProgram->Code, PI_ERROR_INVALID_PROGRAM);

  ur_program_handle_t UrProgram = PiProgram->UrProgram;
  ur_context_handle_t UrContext = PiProgram->Context->UrContext;

  // printf("%s %d PiProgram %lx\n", __FILE__, __LINE__, (unsigned long int)PiProgram);

  HANDLE_ERRORS(urProgramBuild(UrContext,
                              UrProgram,
                              Options));

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
    // printf("%s %d UrKernel %lx\n", __FILE__, __LINE__, (unsigned long int)UrKernel);
    // printf("%s %d *RetKernel %lx\n", __FILE__, __LINE__, (unsigned long int)*RetKernel);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
}

inline pi_result
piEnqueueMemImageFill(pi_queue Queue,
                      pi_mem Image,
                      const void *FillColor,
                      const size_t *Origin,
                      const size_t *Region,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventsWaitList,
                      pi_event *Event) {

  std::ignore = Image;
  std::ignore = FillColor;
  std::ignore = Origin;
  std::ignore = Region;
  std::ignore = NumEventsInWaitList;
  std::ignore = EventsWaitList;
  std::ignore = Event;

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);
  
  die("piEnqueueMemImageFill: not implemented");
  return PI_SUCCESS;
}

inline pi_result
piEnqueueNativeKernel(pi_queue Queue,
                      void (*UserFunc)(void *),
                      void *Args,
                      size_t CbArgs,
                      pi_uint32 NumMemObjects,
                      const pi_mem *MemList,
                      const void **ArgsMemLoc,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventsWaitList,
                      pi_event *Event) {
  std::ignore = UserFunc;
  std::ignore = Args;
  std::ignore = CbArgs;
  std::ignore = NumMemObjects;
  std::ignore = MemList;
  std::ignore = ArgsMemLoc;
  std::ignore = NumEventsInWaitList;
  std::ignore = EventsWaitList;
  std::ignore = Event;

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  die("piEnqueueNativeKernel: not implemented");
  return PI_SUCCESS;
}

inline pi_result
piextGetDeviceFunctionPointer(pi_device Device,
                              pi_program Program,
                              const char *FunctionName,
                              pi_uint64 *FunctionPointerRet) {

  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);

  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;

  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(Program);
  ur_program_handle_t UrProgram = PiProgram->UrProgram;

  void **FunctionPointer = reinterpret_cast<void **>(FunctionPointerRet);

  HANDLE_ERRORS(urProgramGetFunctionPointer(UrDevice,
                                            UrProgram,
                                            FunctionName,
                                            FunctionPointer));
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

  _pi_mem **PiArg = reinterpret_cast<_pi_mem **>(const_cast<pi_mem *>(ArgValue));
  ur_mem_handle_t UrMemory = (*PiArg)->UrMemory;

  // We don't yet know the device where this kernel will next be run on.
  // Thus we can't know the actual memory allocation that needs to be used.
  // Remember the memory object being used as an argument for this kernel
  // to process it later when the device is known (at the kernel enqueue).
  //
  // TODO: for now we have to conservatively assume the access as read-write.
  //       Improve that by passing SYCL buffer accessor type into
  //       piextKernelSetArgMemObj.
  //

  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);

  // printf("%s %d UrMemory %lx\n", __FILE__, __LINE__, (unsigned long int)UrMemory);

  HANDLE_ERRORS(urKernelSetArgMemObj(PiKernel->UrKernel,
                                     ArgIndex,
                                     UrMemory));
  return PI_SUCCESS;
}

inline pi_result 
piKernelSetArg(pi_kernel Kernel,
               pi_uint32 ArgIndex,
               size_t ArgSize,
               const void *ArgValue) {

   PI_ASSERT(Kernel, PI_ERROR_INVALID_KERNEL);

  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);

  HANDLE_ERRORS(urKernelSetArgValue(PiKernel->UrKernel,
                                    ArgIndex,
                                    ArgSize,
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

inline pi_result
piProgramRetain(pi_program Program) {
  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);
  
  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(Program);
  ur_program_handle_t UrProgram = PiProgram->UrProgram;
  HANDLE_ERRORS(urProgramRetain(reinterpret_cast<ur_program_handle_t>(UrProgram)));

  return PI_SUCCESS;
}

inline pi_result
piKernelSetExecInfo(pi_kernel Kernel,
                    pi_kernel_exec_info ParamName,
                    size_t ParamValueSize,
                    const void *ParamValue) {

  PI_ASSERT(Kernel, PI_ERROR_INVALID_KERNEL);
  PI_ASSERT(ParamValue, PI_ERROR_INVALID_VALUE);

  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);
  ur_kernel_handle_t hKernel = PiKernel->UrKernel;
  ur_kernel_exec_info_t propName {};
  switch(ParamName) {
    case   PI_USM_INDIRECT_ACCESS: {
      propName = UR_KERNEL_EXEC_INFO_USM_INDIRECT_ACCESS;
      break;
    }
    case PI_USM_PTRS: {
      propName = UR_KERNEL_EXEC_INFO_USM_PTRS;
      break;
    }
    default:
      return PI_ERROR_INVALID_PROPERTY;
  }
  HANDLE_ERRORS(urKernelSetExecInfo(hKernel,
                                    propName,
                                    ParamValueSize,
                                    ParamValue));

  return PI_SUCCESS;
}

inline pi_result
piextProgramGetNativeHandle(pi_program Program,
                            pi_native_handle *NativeHandle) {
  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(Program);
  ur_program_handle_t UrProgram = PiProgram->UrProgram;
  ur_native_handle_t NativeProgram {};
  HANDLE_ERRORS(urProgramGetNativeHandle(UrProgram,
                                         &NativeProgram));

  *NativeHandle = reinterpret_cast<pi_native_handle>(NativeProgram);

  return PI_SUCCESS;
}

inline pi_result
piextProgramCreateWithNativeHandle(pi_native_handle NativeHandle,
                                   pi_context Context,
                                  bool ownNativeHandle,
                                    pi_program *Program) {
  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);

  ur_native_handle_t NativeProgram = reinterpret_cast<ur_native_handle_t>(NativeHandle);
  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;
  ur_program_handle_t UrProgram {};
  HANDLE_ERRORS(urProgramCreateWithNativeHandle(NativeProgram,
                                                UrContext,
                                                &UrProgram));
    try {
    _pi_program *PiProgram = new _pi_program(_pi_program::state::Exe,
                                     PiContext,
                                    UrProgram,
                                    ownNativeHandle);
    *Program = reinterpret_cast<pi_program>(PiProgram);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
}


inline pi_result
piKernelGetInfo(pi_kernel Kernel,
                pi_kernel_info ParamName,
                size_t ParamValueSize,
                void *ParamValue,
                size_t *ParamValueSizeRet) {
  PI_ASSERT(Kernel, PI_ERROR_INVALID_KERNEL);

  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);
  ur_kernel_handle_t UrKernel = PiKernel->UrKernel;
  ur_kernel_info_t UrParamName {};
  switch(ParamName) {
    case  PI_KERNEL_INFO_FUNCTION_NAME: {
      UrParamName = UR_KERNEL_INFO_FUNCTION_NAME;
      break;
    }
    case PI_KERNEL_INFO_NUM_ARGS: {
      UrParamName = UR_KERNEL_INFO_NUM_ARGS;
      break;
    }
    case PI_KERNEL_INFO_REFERENCE_COUNT: {
      UrParamName = UR_KERNEL_INFO_REFERENCE_COUNT;
      break;
    }
    case PI_KERNEL_INFO_CONTEXT: {
      UrParamName = UR_KERNEL_INFO_CONTEXT;
      break;
    }
    case PI_KERNEL_INFO_PROGRAM: {
      UrParamName = UR_KERNEL_INFO_PROGRAM;
      break;
    }
    case PI_KERNEL_INFO_ATTRIBUTES: {
      UrParamName = UR_KERNEL_INFO_ATTRIBUTES;
      break;
    }
    default:
      return PI_ERROR_INVALID_PROPERTY;
  }

  HANDLE_ERRORS(urKernelGetInfo(UrKernel,
                                UrParamName,
                                ParamValueSize,
                                ParamValue,
                                ParamValueSizeRet));

  return PI_SUCCESS;
}

inline pi_result 
piKernelGetGroupInfo(pi_kernel Kernel,
                     pi_device Device,
                     pi_kernel_group_info ParamName,
                     size_t ParamValueSize,
                     void *ParamValue,
                     size_t *ParamValueSizeRet) {
  PI_ASSERT(Kernel, PI_ERROR_INVALID_KERNEL);
  PI_ASSERT(Device, PI_ERROR_INVALID_DEVICE);

  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);
  ur_kernel_handle_t UrKernel = PiKernel->UrKernel;
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;
  
  ur_kernel_group_info_t UrParamName {};
  switch (ParamName) {
    case PI_KERNEL_GROUP_INFO_GLOBAL_WORK_SIZE: {
      UrParamName = UR_KERNEL_GROUP_INFO_GLOBAL_WORK_SIZE;
      break;
    }
    case PI_KERNEL_GROUP_INFO_WORK_GROUP_SIZE: {
      UrParamName = UR_KERNEL_GROUP_INFO_WORK_GROUP_SIZE;
      break;
    }
    case PI_KERNEL_GROUP_INFO_COMPILE_WORK_GROUP_SIZE: {
      UrParamName = UR_KERNEL_GROUP_INFO_COMPILE_WORK_GROUP_SIZE;
      break;
    }
    case PI_KERNEL_GROUP_INFO_LOCAL_MEM_SIZE: {
      UrParamName = UR_KERNEL_GROUP_INFO_LOCAL_MEM_SIZE;
      break;
    }
    case PI_KERNEL_GROUP_INFO_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: {
      UrParamName = UR_KERNEL_GROUP_INFO_PREFERRED_WORK_GROUP_SIZE_MULTIPLE;
      break;
    }
    case PI_KERNEL_GROUP_INFO_PRIVATE_MEM_SIZE: {
      UrParamName = UR_KERNEL_GROUP_INFO_PRIVATE_MEM_SIZE;
      break;
    }
    // The number of registers used by the compiled kernel (device specific)
    case PI_KERNEL_GROUP_INFO_NUM_REGS: {
      die("PI_KERNEL_GROUP_INFO_NUM_REGS in piKernelGetGroupInfo not "
        "implemented\n");
      break;
    }
    default: {
      printf("Unknown ParamName in piKernelGetGroupInfo: ParamName=%d(0x%x)\n",
            ParamName, ParamName);
      return PI_ERROR_INVALID_VALUE;
    }
  }

  HANDLE_ERRORS(urKernelGetGroupInfo(UrKernel,
                                     UrDevice,
                                     UrParamName,
                                     ParamValueSize,
                                     ParamValue,
                                     ParamValueSizeRet));

  return PI_SUCCESS;
}

inline pi_result
piKernelRetain(pi_kernel Kernel) {

  PI_ASSERT(Kernel, PI_ERROR_INVALID_KERNEL);

  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);
  ur_kernel_handle_t UrKernel = PiKernel->UrKernel;

  HANDLE_ERRORS(urKernelRetain(UrKernel));

  return PI_SUCCESS;
}

inline pi_result
piKernelRelease(pi_kernel Kernel) {

  PI_ASSERT(Kernel, PI_ERROR_INVALID_KERNEL);

  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);
  ur_kernel_handle_t UrKernel = PiKernel->UrKernel;

  HANDLE_ERRORS(urKernelRelease(UrKernel));

  return PI_SUCCESS;
}

inline pi_result
piProgramRelease(pi_program Program) {
  
  PI_ASSERT(Program, PI_ERROR_INVALID_PROGRAM);

  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(Program);
  ur_program_handle_t UrProgram = PiProgram->UrProgram;

  HANDLE_ERRORS(urProgramRelease(UrProgram));

  return PI_SUCCESS;
}

inline pi_result
piextKernelSetArgPointer(pi_kernel Kernel,
                         pi_uint32 ArgIndex,
                         size_t ArgSize,
                         const void *ArgValue) {
  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);
  ur_kernel_handle_t UrKernel = PiKernel->UrKernel;

  HANDLE_ERRORS(urKernelSetArgValue(UrKernel,
                                    ArgIndex,
                                    ArgSize,
                                    ArgValue));

  return PI_SUCCESS;
}

inline pi_result
piKernelGetSubGroupInfo(pi_kernel Kernel,
                        pi_device Device,
                        pi_kernel_sub_group_info ParamName,
                        size_t InputValueSize,
                        const void *InputValue,
                        size_t ParamValueSize,
                        void *ParamValue,
                        size_t *ParamValueSizeRet) {

  (void)InputValueSize;
  (void)InputValue;

  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);
  ur_kernel_handle_t UrKernel = PiKernel->UrKernel;
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;

  ur_kernel_sub_group_info_t PropName {};
  switch (ParamName) {
    case PI_KERNEL_MAX_SUB_GROUP_SIZE: {
      PropName = UR_KERNEL_SUB_GROUP_INFO_MAX_SUB_GROUP_SIZE;
      break;
    }
    case PI_KERNEL_MAX_NUM_SUB_GROUPS: {
      PropName = UR_KERNEL_SUB_GROUP_INFO_MAX_NUM_SUB_GROUPS;
      break;
    }
    case PI_KERNEL_COMPILE_NUM_SUB_GROUPS: {
      PropName = UR_KERNEL_SUB_GROUP_INFO_COMPILE_NUM_SUB_GROUPS;
      break;
    }
    case PI_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL: {
      PropName = UR_KERNEL_SUB_GROUP_INFO_SUB_GROUP_SIZE_INTEL;
      break;
    }
  }
  HANDLE_ERRORS(urKernelGetSubGroupInfo(UrKernel,
                                        UrDevice,
                                        PropName,
                                        ParamValueSize,
                                        ParamValue,
                                        ParamValueSizeRet));

  return PI_SUCCESS;
}

inline pi_result
piProgramGetBuildInfo(pi_program Program,
                      pi_device Device,
                      pi_program_build_info ParamName,
                      size_t ParamValueSize,
                      void *ParamValue,
                      size_t *ParamValueSizeRet) {

  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(Program);
  ur_program_handle_t UrProgram = PiProgram->UrProgram;
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;

  ur_program_build_info_t PropName {};
  switch (ParamName) {
    case PI_PROGRAM_BUILD_INFO_STATUS: {
      PropName = UR_PROGRAM_BUILD_INFO_STATUS;
      break;
    }
    case PI_PROGRAM_BUILD_INFO_OPTIONS: {
      PropName = UR_PROGRAM_BUILD_INFO_OPTIONS;
      break;
    }
    case PI_PROGRAM_BUILD_INFO_LOG: {
      PropName = UR_PROGRAM_BUILD_INFO_LOG;
      break;
    }
    case PI_PROGRAM_BUILD_INFO_BINARY_TYPE: {
      PropName = UR_PROGRAM_BUILD_INFO_BINARY_TYPE;
      break;
    }
    default: {
      die("piProgramGetBuildInfo: not implemented");
    }
  }
  HANDLE_ERRORS(urProgramGetBuildInfo(UrProgram,
                                      UrDevice,
                                      PropName,
                                      ParamValueSize,
                                      ParamValue,
                                      ParamValueSizeRet));

  return PI_SUCCESS;
}

inline pi_result
piextKernelGetNativeHandle(pi_kernel Kernel,
                           pi_native_handle *NativeHandle) {
  PI_ASSERT(Kernel, PI_ERROR_INVALID_KERNEL);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);

  ur_kernel_handle_t UrKernel = PiKernel->UrKernel;
  ur_native_handle_t NativeKernel {};
  HANDLE_ERRORS(urKernelGetNativeHandle(UrKernel,
                                        &NativeKernel));

  *NativeHandle = reinterpret_cast<pi_native_handle>(NativeKernel);

  return PI_SUCCESS;
}

/// API for writing data from host to a device global variable.
///
/// \param Queue is the queue
/// \param Program is the program containing the device global variable
/// \param Name is the unique identifier for the device global variable
/// \param BlockingWrite is true if the write should block
/// \param Count is the number of bytes to copy
/// \param Offset is the byte offset into the device global variable to start
/// copying
/// \param Src is a pointer to where the data must be copied from
/// \param NumEventsInWaitList is a number of events in the wait list
/// \param EventWaitList is the wait list
/// \param Event is the resulting event
inline pi_result
piextEnqueueDeviceGlobalVariableWrite(pi_queue Queue,
                                      pi_program Program,
                                      const char *Name,
                                      pi_bool BlockingWrite,
                                      size_t Count,
                                      size_t Offset,
                                      const void *Src,
                                      pi_uint32 NumEventsInWaitList,
                                      const pi_event *EventsWaitList,
                                      pi_event *Event) {
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(Program);
  ur_program_handle_t UrProgram = PiProgram->UrProgram;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  HANDLE_ERRORS(urEnqueueDeviceGlobalVariableWrite(UrQueue,
                                                   UrProgram,
                                                   Name,
                                                   BlockingWrite,
                                                   Count,
                                                   Offset,
                                                   Src,
                                                   NumEventsInWaitList,
                                                   UrEventsWaitList.data(),
                                                   UrEvent));

  return PI_SUCCESS;
}

/// API reading data from a device global variable to host.
///
/// \param Queue is the queue
/// \param Program is the program containing the device global variable
/// \param Name is the unique identifier for the device global variable
/// \param BlockingRead is true if the read should block
/// \param Count is the number of bytes to copy
/// \param Offset is the byte offset into the device global variable to start
/// copying
/// \param Dst is a pointer to where the data must be copied to
/// \param NumEventsInWaitList is a number of events in the wait list
/// \param EventWaitList is the wait list
/// \param Event is the resulting event
inline pi_result
piextEnqueueDeviceGlobalVariableRead(pi_queue Queue,
                                     pi_program Program,
                                     const char *Name,
                                     pi_bool BlockingRead,
                                     size_t Count,
                                     size_t Offset,
                                     void *Dst,
                                     pi_uint32 NumEventsInWaitList,
                                     const pi_event *EventsWaitList,
                                     pi_event *Event) {

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_program *PiProgram = reinterpret_cast<_pi_program *>(Program);
  ur_program_handle_t UrProgram = PiProgram->UrProgram;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  HANDLE_ERRORS(urEnqueueDeviceGlobalVariableRead(UrQueue,
                                                  UrProgram,
                                                  Name,
                                                  BlockingRead,
                                                  Count,
                                                  Offset,
                                                  Dst,
                                                  NumEventsInWaitList,
                                                  UrEventsWaitList.data(),
                                                  UrEvent));

  return PI_SUCCESS;
}


// Program
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Memory
inline pi_result
piMemBufferCreate(pi_context Context,
                  pi_mem_flags Flags,
                  size_t Size,
                  void *HostPtr,
                  pi_mem *RetMem,
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

  try {
    _pi_buffer *PiMem = new _pi_buffer(UrContext);
    PiMem->UrMemory = UrBuffer;
    *RetMem = reinterpret_cast<pi_mem>(PiMem);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  // printf("%s %d *RetMem %lx\n", __FILE__, __LINE__, (unsigned long int)*RetMem);
  // printf("%s %d UrBuffer %lx\n", __FILE__, __LINE__, (unsigned long int)UrBuffer);

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

inline pi_result
piMemGetInfo(pi_mem Mem,
             pi_mem_info ParamName,
             size_t ParamValueSize,
             void *ParamValue,
             size_t *ParamValueSizeRet) {  
  _pi_mem *PiMem = reinterpret_cast<_pi_mem *>(Mem);
  PI_ASSERT(PiMem, PI_ERROR_INVALID_VALUE);
  // piMemImageGetInfo must be used for images

  // TODO: To implement
  // PI_ASSERT(!PiMem->isImage(), PI_ERROR_INVALID_VALUE);
  
  ur_mem_handle_t UrMemory = PiMem->UrMemory;
  ur_mem_info_t MemInfoType {};
  switch(ParamName) {
    case PI_MEM_CONTEXT: {
      MemInfoType =  UR_MEM_INFO_CONTEXT;
      break;
    }
    case PI_MEM_SIZE: {
      MemInfoType =  UR_MEM_INFO_SIZE;
      break;
    }
    default: {
      die("piMemGetInfo: unsuppported ParamName.");
    }
  }
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

  try {
    _pi_image *Image = new _pi_image(UrContext);
    Image->UrMemory = UrMem;
    *RetImage = reinterpret_cast<pi_mem>(Image);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

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
    _pi_buffer *PiMem = new _pi_buffer(PiBuffer->UrContext);
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
                      const pi_event *EventsWaitList, pi_event *Event) {
  
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;

  _pi_mem *PiSrcImage = reinterpret_cast<_pi_mem *>(SrcImage);
  ur_mem_handle_t UrImageSrc = PiSrcImage->UrMemory;

  _pi_mem *PiDstImage = reinterpret_cast<_pi_mem *>(DstImage);
  ur_mem_handle_t UrImageDst = PiDstImage->UrMemory;

  ur_rect_offset_t UrSrcOrigin {SrcOrigin->x, SrcOrigin->y, SrcOrigin->z};
  ur_rect_offset_t UrDstOrigin {DstOrigin->x, DstOrigin->y, DstOrigin->z};
  ur_rect_region_t UrRegion {Region->width, Region->depth, Region->height};
  
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;
  
  HANDLE_ERRORS(urEnqueueMemImageCopy(UrQueue,
                                      UrImageSrc,
                                      UrImageDst,
                                      UrSrcOrigin,
                                      UrDstOrigin,
                                      UrRegion,
                                      NumEventsInWaitList,
                                      UrEventsWaitList.data(),
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
    _pi_buffer *PiMem = new _pi_buffer(UrContext,
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

inline pi_result
piextUSMDeviceAlloc(void **ResultPtr,
                    pi_context Context,
                    pi_device Device,
                    pi_usm_mem_properties *Properties,
                    size_t Size,
                    pi_uint32 Alignment) {

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;

  ur_usm_mem_flags_t USMProp {};
  HANDLE_ERRORS(urUSMDeviceAlloc(UrContext,
                                 UrDevice,
                                 &USMProp,
                                 Size,
                                 Alignment,
                                 ResultPtr));

  try {
    _pi_buffer *PiMem = new _pi_buffer(UrContext,
                                 Size,
                                 nullptr,
                                 true);
    *ResultPtr = reinterpret_cast<pi_mem>(PiMem);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
  
}

inline pi_result
piextUSMSharedAlloc(void **ResultPtr,
                    pi_context Context,
                    pi_device Device,
                    pi_usm_mem_properties *Properties,
                    size_t Size,
                    pi_uint32 Alignment) {

  if (Properties && *Properties != 0) {
    PI_ASSERT(*(Properties) == PI_MEM_ALLOC_FLAGS && *(Properties + 2) == 0,
              PI_ERROR_INVALID_VALUE);
  }

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;
  _pi_device *PiDevice = reinterpret_cast<_pi_device *>(Device);
  ur_device_handle_t UrDevice = PiDevice->UrDevice;

  ur_usm_mem_flags_t USMProp {};
  HANDLE_ERRORS(urUSMSharedAlloc(UrContext,
                                 UrDevice,
                                 &USMProp,
                                 Size,
                                 Alignment,
                                 ResultPtr));

  try {
    _pi_buffer *PiMem = new _pi_buffer(UrContext,
                                       Size,
                                       nullptr,
                                       true);
    *ResultPtr = reinterpret_cast<pi_mem>(PiMem);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
}

inline pi_result
piextUSMFree(pi_context Context,
             void *Ptr) {
  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;

  HANDLE_ERRORS(urUSMFree(UrContext,
                          Ptr));

  return PI_SUCCESS;
}

inline pi_result
piMemRetain(pi_mem Mem) {
  PI_ASSERT(Mem, PI_ERROR_INVALID_MEM_OBJECT);

  _pi_mem *PiMem = reinterpret_cast<_pi_mem *>(Mem);
  ur_mem_handle_t UrMem = PiMem->UrMemory;

  HANDLE_ERRORS(urMemRetain(UrMem));

  return PI_SUCCESS;

}

inline pi_result
piMemRelease(pi_mem Mem) {
  PI_ASSERT(Mem, PI_ERROR_INVALID_MEM_OBJECT);

  _pi_mem *PiMem = reinterpret_cast<_pi_mem *>(Mem);
  ur_mem_handle_t UrMem = PiMem->UrMemory;

  HANDLE_ERRORS(urMemRelease(UrMem));

  return PI_SUCCESS;
}


/// Hint to migrate memory to the device
///
/// @param Queue is the queue to submit to
/// @param Ptr points to the memory to migrate
/// @param Size is the number of bytes to migrate
/// @param Flags is a bitfield used to specify memory migration options
/// @param NumEventsInWaitList is the number of events to wait on
/// @param EventsWaitList is an array of events to wait on
/// @param Event is the event that represents this operation
inline pi_result
piextUSMEnqueuePrefetch(pi_queue Queue,
                        const void *Ptr,
                        size_t Size,
                        pi_usm_migration_flags Flags,
                        pi_uint32 NumEventsInWaitList,
                        const pi_event *EventsWaitList,
                        pi_event *OutEvent) {

  // flags is currently unused so fail if set
  PI_ASSERT(Flags == 0, PI_ERROR_INVALID_VALUE);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;

  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*OutEvent);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  // TODO: to map from pi_usm_migration_flags to
  // ur_usm_migration_flags_t
  // once we have those defined
  ur_usm_migration_flags_t UrFlags {};
  HANDLE_ERRORS(urEnqueueUSMPrefetch(UrQueue,
                                     Ptr,
                                     Size,
                                     UrFlags,
                                     NumEventsInWaitList,
                                     UrEventsWaitList.data(),
                                     UrEvent));

  return PI_SUCCESS;
}

/// USM memadvise API to govern behavior of automatic migration mechanisms
///
/// @param Queue is the queue to submit to
/// @param Ptr is the data to be advised
/// @param Length is the size in bytes of the meory to advise
/// @param Advice is device specific advice
/// @param Event is the event that represents this operation
///
inline pi_result
piextUSMEnqueueMemAdvise(pi_queue Queue,
                         const void *Ptr,
                         size_t Length,
                         pi_mem_advice Advice,
                         pi_event *OutEvent) {
  
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  
  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*OutEvent);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  // TODO: to map from pi_mem_advice to ur_mem_advice_t
  // once we have those defined
  ur_mem_advice_t UrAdvice {};
  HANDLE_ERRORS(urEnqueueUSMMemAdvise(UrQueue,
                                      Ptr,
                                      Length,
                                      UrAdvice,
                                      UrEvent));
  return PI_SUCCESS;
}

/// USM 2D Fill API
///
/// \param queue is the queue to submit to
/// \param ptr is the ptr to fill
/// \param pitch is the total width of the destination memory including padding
/// \param pattern is a pointer with the bytes of the pattern to set
/// \param pattern_size is the size in bytes of the pattern
/// \param width is width in bytes of each row to fill
/// \param height is height the columns to fill
/// \param num_events_in_waitlist is the number of events to wait on
/// \param events_waitlist is an array of events to wait on
/// \param event is the event that represents this operation
inline pi_result
piextUSMEnqueueFill2D(pi_queue Queue,
                      void *Ptr,
                      size_t Pitch,
                      size_t PatternSize,
                      const void *Pattern,
                      size_t Width,
                      size_t Height,
                      pi_uint32 NumEventsWaitList,
                      const pi_event *EventsWaitList,
                      pi_event *Event) {

  std::ignore = Queue;
  std::ignore = Ptr;
  std::ignore = Pitch;
  std::ignore = PatternSize;
  std::ignore = Pattern;
  std::ignore = Width;
  std::ignore = Height;
  std::ignore = NumEventsWaitList;
  std::ignore = EventsWaitList;
  std::ignore = Event;
  die("piextUSMEnqueueFill2D: not implemented");
  return {};
}


inline pi_result
piextUSMEnqueueMemset2D(pi_queue Queue,
                        void *Ptr,
                        size_t Pitch,
                        int Value,
                        size_t Width,
                        size_t Height,
                        pi_uint32 NumEventsWaitList,
                        const pi_event *EventsWaitList,
                        pi_event *Event) {
  std::ignore = Queue;
  std::ignore = Ptr;
  std::ignore = Pitch;
  std::ignore = Value;
  std::ignore = Width;
  std::ignore = Height;
  std::ignore = NumEventsWaitList;
  std::ignore = EventsWaitList;
  std::ignore = Event;
  die("piextUSMEnqueueMemset2D: not implemented");
  return PI_SUCCESS;
}

inline pi_result
piextUSMGetMemAllocInfo(pi_context Context,
                        const void *Ptr,
                        pi_mem_alloc_info ParamName,
                        size_t ParamValueSize,
                        void *ParamValue,
                        size_t *ParamValueSizeRet) {

  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;

  ur_usm_alloc_info_t UrParamName{};
  switch(ParamName) {
    case PI_MEM_ALLOC_TYPE: {
      UrParamName = UR_USM_ALLOC_INFO_TYPE;
      break;
    }
    case PI_MEM_ALLOC_BASE_PTR: {
      UrParamName = UR_USM_ALLOC_INFO_BASE_PTR;
      break;
    }
    case PI_MEM_ALLOC_SIZE: {
      UrParamName = UR_USM_ALLOC_INFO_SIZE;
      break;
    }
    case PI_MEM_ALLOC_DEVICE: {
      UrParamName = UR_USM_ALLOC_INFO_DEVICE;
      break;
    }
    default: {
      die("piextUSMGetMemAllocInfo: unsuppported ParamName.");
    }
  }

  HANDLE_ERRORS(urUSMGetMemAllocInfo(UrContext,
                                     Ptr,
                                     UrParamName,
                                     ParamValueSize,
                                     ParamValue,
                                     ParamValueSizeRet))
  return PI_SUCCESS;
}

inline pi_result
piMemImageGetInfo(pi_mem Image,
                  pi_image_info ParamName,
                  size_t ParamValueSize,
                  void *ParamValue,
                  size_t *ParamValueSizeRet) { // missing
  std::ignore = Image;
  std::ignore = ParamName;
  std::ignore = ParamValueSize;
  std::ignore = ParamValue;
  std::ignore = ParamValueSizeRet;

  // TODO: use urMemImageGetInfo

  die("piMemImageGetInfo: not implemented");
  return {};
}

/// USM 2D Memcpy API
///
/// \param queue is the queue to submit to
/// \param blocking is whether this operation should block the host
/// \param dst_ptr is the location the data will be copied
/// \param dst_pitch is the total width of the destination memory including
/// padding
/// \param src_ptr is the data to be copied
/// \param dst_pitch is the total width of the source memory including padding
/// \param width is width in bytes of each row to be copied
/// \param height is height the columns to be copied
/// \param num_events_in_waitlist is the number of events to wait on
/// \param events_waitlist is an array of events to wait on
/// \param event is the event that represents this operation
inline pi_result
piextUSMEnqueueMemcpy2D(pi_queue Queue,
                        pi_bool Blocking,
                        void *DstPtr,
                        size_t DstPitch,
                        const void *SrcPtr,
                        size_t SrcPitch,
                        size_t Width,
                        size_t Height,
                        pi_uint32 NumEventsInWaitList,
                        const pi_event *EventsWaitList,
                        pi_event *Event) {

  if (!DstPtr || !SrcPtr)
    return PI_ERROR_INVALID_VALUE;

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  HANDLE_ERRORS(urEnqueueUSMMemcpy2D(UrQueue,
                                     Blocking,
                                     DstPtr,
                                     DstPitch,
                                     SrcPtr,
                                     SrcPitch,
                                     Width,
                                     Height,
                                     NumEventsInWaitList,
                                     UrEventsWaitList.data(),
                                     UrEvent));
  return PI_SUCCESS;
}

// Memory
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Enqueue

inline pi_result
piEnqueueKernelLaunch(pi_queue Queue,
                      pi_kernel Kernel,
                      pi_uint32 WorkDim,
                      const size_t *GlobalWorkOffset,
                      const size_t *GlobalWorkSize,
                      const size_t *LocalWorkSize,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventsWaitList,
                      pi_event *OutEvent) {

  PI_ASSERT(Kernel, PI_ERROR_INVALID_KERNEL);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);
  PI_ASSERT((WorkDim > 0) && (WorkDim < 4), PI_ERROR_INVALID_WORK_DIMENSION);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);
  ur_kernel_handle_t hKernel = PiKernel->UrKernel;
  uint32_t workDim = reinterpret_cast<uint32_t>(WorkDim);
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  // _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*OutEvent);
  // ur_event_handle_t *UrEvent = &PiEvent->UrEvent;
  // ur_event_handle_t *UrEvent = reinterpret_cast<ur_event_handle_t *>(OutEvent);

  ur_event_handle_t *UrEvent {};
  ur_event_handle_t InternalEvent {};
  if (OutEvent) {
    UrEvent = &InternalEvent;
  }

  // printf("%s %d OutEvent %lx UrEvent %lx\n", __FILE__, __LINE__, (unsigned long int)OutEvent, (unsigned long int)UrEvent);

  HANDLE_ERRORS(urEnqueueKernelLaunch(UrQueue,
                                      hKernel,
                                      workDim,
                                      GlobalWorkOffset,
                                      GlobalWorkSize,
                                      LocalWorkSize,
                                      NumEventsInWaitList,
                                      UrEventsWaitList.data(),
                                      UrEvent));

  if (*OutEvent == nullptr) {
    _pi_event *PiEvent {};
    try {
      PiEvent = new _pi_event(*UrEvent);
      *OutEvent = reinterpret_cast<pi_event>(PiEvent);
    } catch (const std::bad_alloc &) {
      return PI_ERROR_OUT_OF_HOST_MEMORY;
    } catch (...) {
      return PI_ERROR_UNKNOWN;
    }
  } else {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*OutEvent);
    PiEvent->UrEvent = *UrEvent;
  }


  // printf("%s %d\n", __FILE__, __LINE__);

  return PI_SUCCESS;
}

inline pi_result
piEnqueueMemImageWrite(pi_queue Queue, pi_mem Image,
                       pi_bool BlockingWrite, pi_image_offset Origin,
                       pi_image_region Region, size_t InputRowPitch,
                       size_t InputSlicePitch, const void *Ptr,
                       pi_uint32 NumEventsInWaitList,
                       const pi_event *EventsWaitList,
                       pi_event *Event) {

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiMem = reinterpret_cast<_pi_mem *>(Image);
  ur_mem_handle_t UrImage = PiMem->UrMemory;
  ur_rect_offset_t UrOrigin {Origin->x, Origin->y, Origin->z};
  ur_rect_region_t UrRegion {Region->width, Region->depth, Region->height};
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;
  HANDLE_ERRORS(urEnqueueMemImageWrite(UrQueue,
                                       UrImage,
                                       BlockingWrite,
                                       UrOrigin,
                                       UrRegion,
                                       InputRowPitch,
                                       InputSlicePitch,
                                       const_cast<void *>(Ptr),
                                       NumEventsInWaitList,
                                       UrEventsWaitList.data(),
                                       UrEvent));

  return PI_SUCCESS;
}

inline pi_result
piEnqueueMemImageRead(pi_queue Queue, pi_mem Image,
                      pi_bool BlockingRead, pi_image_offset Origin,
                      pi_image_region Region, size_t RowPitch,
                      size_t SlicePitch, void *Ptr,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventsWaitList,
                      pi_event *Event) {
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiMem = reinterpret_cast<_pi_mem *>(Image);
  ur_mem_handle_t UrImage = PiMem->UrMemory;
  ur_rect_offset_t UrOrigin {Origin->x, Origin->y, Origin->z};
  ur_rect_region_t UrRegion {Region->width, Region->depth, Region->height};
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;
  HANDLE_ERRORS(urEnqueueMemImageRead(UrQueue,
                                      UrImage,
                                      BlockingRead,
                                      UrOrigin,
                                      UrRegion,
                                      RowPitch,
                                      SlicePitch,
                                      Ptr,
                                      NumEventsInWaitList,
                                      UrEventsWaitList.data(),
                                      UrEvent));

  return PI_SUCCESS;
}

inline pi_result
piEnqueueMemBufferMap(pi_queue Queue,
                      pi_mem Mem,
                      pi_bool BlockingMap,
                      pi_map_flags MapFlags,
                      size_t Offset,
                      size_t Size,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventsWaitList,
                      pi_event *OutEvent,
                      void **RetMap) {
  // TODO: we don't implement read-only or write-only, always read-write.
  // assert((map_flags & PI_MAP_READ) != 0);
  // assert((map_flags & PI_MAP_WRITE) != 0);
  PI_ASSERT(Mem, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiMem = reinterpret_cast<_pi_mem *>(Mem);
  ur_mem_handle_t UrMem = PiMem->UrMemory;

  ur_map_flags_t UrMapFlags {};
  if (MapFlags == PI_MAP_READ)
    UrMapFlags |= UR_MAP_FLAG_READ;
  if (MapFlags == PI_MAP_WRITE)
    UrMapFlags |= UR_MAP_FLAG_WRITE;
  if (MapFlags == PI_MAP_WRITE_INVALIDATE_REGION)
    UrMapFlags |= UR_EXT_MAP_FLAG_WRITE_INVALIDATE_REGION;

  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*OutEvent);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;
  urEnqueueMemBufferMap(UrQueue,
                        UrMem,
                        BlockingMap,
                        UrMapFlags,
                        Offset,
                        Size,
                        NumEventsInWaitList,
                        UrEventsWaitList.data(),
                        UrEvent,
                        RetMap);

  return PI_SUCCESS;
}

inline pi_result
piEnqueueMemUnmap(pi_queue Queue,
                  pi_mem Mem,
                  void *MappedPtr,
                  pi_uint32 NumEventsInWaitList,
                  const pi_event *EventsWaitList,
                  pi_event *OutEvent) {
  
  PI_ASSERT(Mem, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiMem = reinterpret_cast<_pi_mem *>(Mem);
  ur_mem_handle_t UrMem = PiMem->UrMemory;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*OutEvent);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;
  HANDLE_ERRORS(urEnqueueMemUnmap(UrQueue,
                                  UrMem,
                                  MappedPtr,
                                  NumEventsInWaitList,
                                  UrEventsWaitList.data(),
                                  UrEvent));

  return PI_SUCCESS;
}

inline pi_result
piEnqueueMemBufferFill(pi_queue Queue, pi_mem Buffer,
                       const void *Pattern, size_t PatternSize,
                       size_t Offset, size_t Size,
                       pi_uint32 NumEventsInWaitList,
                       const pi_event *EventsWaitList,
                       pi_event *Event) {
  PI_ASSERT(Buffer, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiBuffer = reinterpret_cast<_pi_mem *>(Buffer);
  ur_mem_handle_t UrBuffer = PiBuffer->UrMemory;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;
  HANDLE_ERRORS(urEnqueueMemBufferFill(UrQueue,
                                       UrBuffer,
                                       Pattern,
                                       PatternSize,
                                       Offset,
                                       Size,
                                       NumEventsInWaitList,
                                       UrEventsWaitList.data(),
                                       UrEvent));
  return PI_SUCCESS;
}

inline pi_result
piextUSMEnqueueMemset(pi_queue Queue,
                      void *Ptr,
                      pi_int32 Value,
                      size_t Count,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventsWaitList,
                      pi_event *Event) {
  PI_ASSERT(Ptr, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiBuffer = reinterpret_cast<_pi_mem *>(Ptr);
  ur_mem_handle_t UrBuffer = PiBuffer->UrMemory;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  uint32_t Pattern = Value;
  size_t PatternSize = sizeof(Pattern);
  HANDLE_ERRORS(urEnqueueMemBufferFill(UrQueue,
                                       UrBuffer,
                                       const_cast<const void *>(reinterpret_cast<void *>(&Pattern)),
                                       PatternSize,
                                       0,
                                       Count,
                                       NumEventsInWaitList,
                                       UrEventsWaitList.data(),
                                       UrEvent));
  return PI_SUCCESS;
}

inline pi_result
piEnqueueMemBufferCopyRect(pi_queue Queue,
                           pi_mem SrcMem,
                           pi_mem DstMem,
                           pi_buff_rect_offset SrcOrigin,
                           pi_buff_rect_offset DstOrigin,
                           pi_buff_rect_region Region,
                           size_t SrcRowPitch,
                           size_t SrcSlicePitch,
                           size_t DstRowPitch,
                           size_t DstSlicePitch,
                           pi_uint32 NumEventsInWaitList,
                           const pi_event *EventsWaitList,
                           pi_event *Event) {

  PI_ASSERT(SrcMem && DstMem, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);


  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiSrcBuffer = reinterpret_cast<_pi_mem *>(SrcMem);
  ur_mem_handle_t UrBufferSrc = PiSrcBuffer->UrMemory;
  _pi_mem *PiDstBuffer = reinterpret_cast<_pi_mem *>(DstMem);
  ur_mem_handle_t UrBufferDst = PiDstBuffer->UrMemory;
  ur_rect_offset_t UrSrcOrigin {SrcOrigin->x_bytes, SrcOrigin->y_scalar, SrcOrigin->z_scalar};
  ur_rect_offset_t UrDstOrigin {DstOrigin->x_bytes, DstOrigin->y_scalar, DstOrigin->z_scalar};
  ur_rect_region_t UrRegion {Region->width_bytes, Region->depth_scalar, Region->height_scalar};
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  HANDLE_ERRORS(urEnqueueMemBufferCopyRect(UrQueue,
                                           UrBufferSrc,
                                           UrBufferDst,
                                           UrSrcOrigin,
                                           UrDstOrigin,
                                           UrRegion,
                                           SrcRowPitch,
                                           SrcSlicePitch,
                                           DstRowPitch,
                                           DstSlicePitch,
                                           NumEventsInWaitList,
                                           UrEventsWaitList.data(),
                                           UrEvent));
  return PI_SUCCESS;
}

inline pi_result 
piEnqueueMemBufferCopy(pi_queue Queue, pi_mem SrcMem, pi_mem DstMem,
                       size_t SrcOffset,
                       size_t DstOffset,
                       size_t Size,
                       pi_uint32 NumEventsInWaitList,
                       const pi_event *EventsWaitList,
                       pi_event *Event) {
 
  PI_ASSERT(SrcMem && DstMem, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiSrcBuffer = reinterpret_cast<_pi_mem *>(SrcMem);
  ur_mem_handle_t UrBufferSrc = PiSrcBuffer->UrMemory;
  _pi_mem *PiDstBuffer = reinterpret_cast<_pi_mem *>(DstMem);
  ur_mem_handle_t UrBufferDst = PiDstBuffer->UrMemory;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  HANDLE_ERRORS(urEnqueueMemBufferCopy(UrQueue,
                                       UrBufferSrc,
                                       UrBufferDst,
                                       SrcOffset,
                                       DstOffset,
                                       Size,
                                       NumEventsInWaitList,
                                       UrEventsWaitList.data(),
                                       UrEvent));
  return PI_SUCCESS;
}

inline pi_result 
piextUSMEnqueueMemcpy(pi_queue Queue,
                      pi_bool Blocking,
                      void *DstPtr,
                      const void *SrcPtr,
                      size_t Size,
                      pi_uint32 NumEventsInWaitList,
                      const pi_event *EventsWaitList,
                      pi_event *Event) {

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  HANDLE_ERRORS(urEnqueueUSMMemcpy(UrQueue,
                                   Blocking,
                                   DstPtr,
                                   SrcPtr,
                                   Size,
                                   NumEventsInWaitList,
                                   UrEventsWaitList.data(),
                                   UrEvent));
  return PI_SUCCESS;
}

inline pi_result
piEnqueueMemBufferWriteRect(pi_queue Queue,
                            pi_mem Buffer,
                            pi_bool BlockingWrite,
                            pi_buff_rect_offset BufferOffset,
                            pi_buff_rect_offset HostOffset,
                            pi_buff_rect_region Region,
                            size_t BufferRowPitch,
                            size_t BufferSlicePitch,
                            size_t HostRowPitch,
                            size_t HostSlicePitch,
                            const void *Ptr,
                            pi_uint32 NumEventsInWaitList,
                            const pi_event *EventsWaitList,
                            pi_event *Event) {

  PI_ASSERT(Buffer, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiBuffer = reinterpret_cast<_pi_mem *>(Buffer);
  ur_mem_handle_t UrBuffer = PiBuffer->UrMemory;
  ur_rect_offset_t UrBufferOffset {BufferOffset->x_bytes, BufferOffset->y_scalar, BufferOffset->z_scalar};
  ur_rect_offset_t UrHostOffset {HostOffset->x_bytes, HostOffset->y_scalar, HostOffset->z_scalar};
  ur_rect_region_t UrRegion {Region->width_bytes, Region->depth_scalar, Region->height_scalar};
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  HANDLE_ERRORS(urEnqueueMemBufferWriteRect(UrQueue,
                                            UrBuffer,
                                            BlockingWrite,
                                            UrBufferOffset,
                                            UrHostOffset,
                                            UrRegion,
                                            BufferRowPitch,
                                            BufferSlicePitch,
                                            HostRowPitch,
                                            HostSlicePitch,
                                            const_cast<void *>(Ptr),
                                            NumEventsInWaitList,
                                            UrEventsWaitList.data(),
                                            UrEvent));
  
 return PI_SUCCESS;
}

inline pi_result
piEnqueueMemBufferWrite(pi_queue Queue,
                        pi_mem Buffer,
                        pi_bool BlockingWrite,
                        size_t Offset,
                        size_t Size,
                        const void *Ptr,
                        pi_uint32 NumEventsInWaitList,
                        const pi_event *EventsWaitList,
                        pi_event *Event) {

  PI_ASSERT(Buffer, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiBuffer = reinterpret_cast<_pi_mem *>(Buffer);
  ur_mem_handle_t UrBuffer = PiBuffer->UrMemory;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  HANDLE_ERRORS(urEnqueueMemBufferWrite(UrQueue,
                                        UrBuffer,
                                        BlockingWrite,
                                        Offset,
                                        Size,
                                        const_cast<void *>(Ptr),
                                        NumEventsInWaitList,
                                        UrEventsWaitList.data(),
                                        UrEvent));
  
  return PI_SUCCESS;
}

inline pi_result
piEnqueueMemBufferReadRect(pi_queue Queue,
                           pi_mem Buffer,
                           pi_bool BlockingRead,
                           pi_buff_rect_offset BufferOffset,
                           pi_buff_rect_offset HostOffset,
                           pi_buff_rect_region Region,
                           size_t BufferRowPitch,
                           size_t BufferSlicePitch,
                           size_t HostRowPitch,
                           size_t HostSlicePitch,
                           void *Ptr,
                           pi_uint32 NumEventsInWaitList,
                           const pi_event *EventsWaitList,
                           pi_event *Event) {

  PI_ASSERT(Buffer, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiBuffer = reinterpret_cast<_pi_mem *>(Buffer);
  ur_mem_handle_t UrBuffer = PiBuffer->UrMemory;
  ur_rect_offset_t UrBufferOffset {BufferOffset->x_bytes, BufferOffset->y_scalar, BufferOffset->z_scalar};
  ur_rect_offset_t UrHostOffset {HostOffset->x_bytes, HostOffset->y_scalar, HostOffset->z_scalar};
  ur_rect_region_t UrRegion {Region->width_bytes, Region->depth_scalar, Region->height_scalar};

  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  HANDLE_ERRORS(urEnqueueMemBufferReadRect(UrQueue,
                                           UrBuffer,
                                           BlockingRead,
                                           UrBufferOffset,
                                           UrHostOffset,
                                           UrRegion,
                                           BufferRowPitch,
                                           BufferSlicePitch,
                                           HostRowPitch,
                                           HostSlicePitch,
                                           Ptr,
                                           NumEventsInWaitList,
                                           UrEventsWaitList.data(),
                                           UrEvent));

  return PI_SUCCESS;
}

inline pi_result
piEnqueueMemBufferRead(pi_queue Queue,
                        pi_mem Src,
                        pi_bool BlockingRead,
                        size_t Offset,
                        size_t Size,
                        void *Dst,
                        pi_uint32 NumEventsInWaitList,
                        const pi_event *EventsWaitList,
                        pi_event *Event) {
  PI_ASSERT(Src, PI_ERROR_INVALID_MEM_OBJECT);
  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);
  
  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  _pi_mem *PiBuffer = reinterpret_cast<_pi_mem *>(Src);
  ur_mem_handle_t UrBuffer = PiBuffer->UrMemory;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*Event);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  HANDLE_ERRORS(urEnqueueMemBufferRead(UrQueue,
                                       UrBuffer,
                                       BlockingRead,
                                       Offset,
                                       Size,
                                       Dst,
                                       NumEventsInWaitList,
                                       UrEventsWaitList.data(),
                                       UrEvent));
  return PI_SUCCESS;
}

inline pi_result
piEnqueueEventsWaitWithBarrier(pi_queue Queue,
                               pi_uint32 NumEventsInWaitList,
                               const pi_event *EventsWaitList,
                               pi_event *OutEvent) {

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);

  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*OutEvent);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;

  HANDLE_ERRORS(urEnqueueEventsWaitWithBarrier(UrQueue,
                                               NumEventsInWaitList,
                                               UrEventsWaitList.data(),
                                               UrEvent));
 
  return PI_SUCCESS;
}


inline pi_result
piEnqueueEventsWait(pi_queue Queue,
                    pi_uint32 NumEventsInWaitList,
                    const pi_event *EventsWaitList,
                    pi_event *OutEvent) {

  PI_ASSERT(Queue, PI_ERROR_INVALID_QUEUE);
  _pi_queue *PiQueue = reinterpret_cast<_pi_queue *>(Queue);
  ur_queue_handle_t UrQueue = PiQueue->UrQueue;
  std::vector<ur_event_handle_t> UrEventsWaitList(NumEventsInWaitList);
  for (uint32_t EventIt = 0; EventIt < NumEventsInWaitList; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(*OutEvent);
  ur_event_handle_t *UrEvent = &PiEvent->UrEvent;
  
  HANDLE_ERRORS(urEnqueueEventsWait(UrQueue,
                                    NumEventsInWaitList,
                                    UrEventsWaitList.data(),
                                    UrEvent));

  return PI_SUCCESS;
}
// Enqueue
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Events
inline pi_result
piEventsWait(pi_uint32 NumEvents,
             const pi_event *EventsWaitList) {
  if (NumEvents && !EventsWaitList) {
    return PI_ERROR_INVALID_EVENT;
  }

  std::vector<ur_event_handle_t> UrEventsWaitList(NumEvents);
  for (uint32_t EventIt = 0; EventIt < NumEvents; EventIt++) {
    _pi_event *PiEvent = reinterpret_cast<_pi_event *>(EventsWaitList[EventIt]);
    UrEventsWaitList[EventIt] = PiEvent->UrEvent;
  }

  HANDLE_ERRORS(urEventWait(NumEvents,
                            UrEventsWaitList.data()));

  return PI_SUCCESS;
}

inline pi_result
piEventGetInfo(pi_event Event,
               pi_event_info ParamName,
               size_t ParamValueSize,
               void *ParamValue,
               size_t *ParamValueSizeRet) {

  PI_ASSERT(Event, PI_ERROR_INVALID_EVENT);

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(Event);
  ur_event_handle_t UrEvent = PiEvent->UrEvent;

  ur_event_info_t PropName {};
  if (ParamName == PI_EVENT_INFO_COMMAND_QUEUE) {
    PropName = UR_EVENT_INFO_COMMAND_QUEUE;
  } else if (ParamName == PI_EVENT_INFO_CONTEXT) {
    PropName = UR_EVENT_INFO_CONTEXT;
  } else if (ParamName == PI_EVENT_INFO_COMMAND_TYPE) {
    PropName = UR_EVENT_INFO_COMMAND_TYPE;
  } else if (ParamName == PI_EVENT_INFO_COMMAND_EXECUTION_STATUS) {
    PropName = UR_EVENT_INFO_COMMAND_EXECUTION_STATUS;
  } else if (ParamName == PI_EVENT_INFO_REFERENCE_COUNT) {
    PropName = UR_EVENT_INFO_REFERENCE_COUNT;
  } else {
    return PI_ERROR_INVALID_VALUE;
  }

  HANDLE_ERRORS(urEventGetInfo(UrEvent,
                               PropName,
                               ParamValueSize,
                               ParamValue,
                               ParamValueSizeRet));

  return PI_SUCCESS;
}

inline pi_result
piextEventGetNativeHandle(pi_event Event,
                          pi_native_handle *NativeHandle) {

  PI_ASSERT(Event, PI_ERROR_INVALID_EVENT);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(Event);
  ur_event_handle_t UrEvent = PiEvent->UrEvent;

  ur_native_handle_t *UrNativeEvent = reinterpret_cast<ur_native_handle_t *>(NativeHandle);
  HANDLE_ERRORS(urEventGetNativeHandle(UrEvent,
                                       UrNativeEvent));

  return PI_SUCCESS;
}

inline pi_result
piEventGetProfilingInfo(pi_event Event,
                        pi_profiling_info ParamName,
                        size_t ParamValueSize,
                        void *ParamValue,
                        size_t *ParamValueSizeRet) {

  PI_ASSERT(Event, PI_ERROR_INVALID_EVENT);

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(Event);
  ur_event_handle_t UrEvent = PiEvent->UrEvent;

  // printf("%s %d ParamName %d\n", __FILE__, __LINE__, (uint32_t)ParamName);

  ur_profiling_info_t PropName {};
  switch(ParamName) {
    case PI_PROFILING_INFO_COMMAND_QUEUED: {
      // printf("%s %d\n", __FILE__, __LINE__);
      PropName = UR_PROFILING_INFO_COMMAND_QUEUED;
      break;
    }
    case PI_PROFILING_INFO_COMMAND_SUBMIT: {
      // printf("%s %d\n", __FILE__, __LINE__);
      PropName = UR_PROFILING_INFO_COMMAND_SUBMIT;
      break;
    }
    case PI_PROFILING_INFO_COMMAND_START: {
      // printf("%s %d\n", __FILE__, __LINE__);
      PropName = UR_PROFILING_INFO_COMMAND_START;
      break;
    }
    case PI_PROFILING_INFO_COMMAND_END: {
      // printf("%s %d\n", __FILE__, __LINE__);
      PropName = UR_PROFILING_INFO_COMMAND_END;
      break;
    }
    default:
      // printf("%s %d\n", __FILE__, __LINE__);
      return PI_ERROR_INVALID_PROPERTY;
  }

  HANDLE_ERRORS(urEventGetProfilingInfo(UrEvent,
                                        PropName,
                                        ParamValueSize,
                                        ParamValue,
                                        ParamValueSizeRet));

  // printf("%s %d\n", __FILE__, __LINE__);

  return PI_SUCCESS;
}

inline pi_result
piEventCreate(pi_context Context,
              pi_event *RetEvent) {

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;

  ur_event_handle_t UrEvent {};
  // pass null for the hNativeHandle to use urEventCreateWithNativeHandle
  // as urEventCreate
  HANDLE_ERRORS(urEventCreateWithNativeHandle(nullptr,
                                              UrContext,
                                              &UrEvent));

  _pi_event *PiEvent {};
  try {
    PiEvent = new _pi_event(UrEvent);
    *RetEvent = reinterpret_cast<pi_event>(PiEvent);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
}

inline pi_result
piextEventCreateWithNativeHandle(pi_native_handle NativeHandle,
                                           pi_context Context,
                                           bool OwnNativeHandle,
                                           pi_event *Event) {

  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  PI_ASSERT(Event, PI_ERROR_INVALID_EVENT);
  PI_ASSERT(NativeHandle, PI_ERROR_INVALID_VALUE);

  ur_native_handle_t UrNativeKernel = reinterpret_cast<ur_native_handle_t>(NativeHandle);

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;

  ur_event_handle_t UrEvent {};
  HANDLE_ERRORS(urEventCreateWithNativeHandle(UrNativeKernel,
                                              UrContext,
                                              &UrEvent));  

  _pi_event *PiEvent {};
  try {
    PiEvent = new _pi_event(UrEvent);
    *Event = reinterpret_cast<pi_event>(PiEvent);
  } catch (const std::bad_alloc &) {
    return PI_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  PiEvent->OwnZeEvent = OwnNativeHandle;

  return PI_SUCCESS;
}


inline pi_result
piEventSetCallback(pi_event Event,
                   pi_int32 CommandExecCallbackType,
                   void (*PFnNotify)(pi_event Event,
                                    pi_int32 EventCommandStatus,
                                    void *UserData),
                   void *UserData) {
  (void)Event;
  (void)CommandExecCallbackType;
  (void)PFnNotify;
  (void)UserData;
  die("piEventSetCallback: deprecated, to be removed");
  return PI_SUCCESS;
}

inline pi_result
piEventSetStatus(pi_event Event,
                 pi_int32 ExecutionStatus) {
  std::ignore = Event;
  std::ignore = ExecutionStatus;
  die("piEventSetStatus: deprecated, to be removed");
  return PI_SUCCESS;
}

inline pi_result
piEventRetain(pi_event Event) {
  PI_ASSERT(Event, PI_ERROR_INVALID_EVENT);
  
  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(Event);
  ur_event_handle_t UrEvent = PiEvent->UrEvent;
  HANDLE_ERRORS(urEventRetain(UrEvent));
  
  return PI_SUCCESS;
}

inline pi_result
piEventRelease(pi_event Event) {
  PI_ASSERT(Event, PI_ERROR_INVALID_EVENT);

  _pi_event *PiEvent = reinterpret_cast<_pi_event *>(Event);
  ur_event_handle_t UrEvent = PiEvent->UrEvent;
  HANDLE_ERRORS(urEventRelease(UrEvent));

  return PI_SUCCESS;
}


// Events
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Sampler
inline pi_result
piSamplerCreate(pi_context Context,
                const pi_sampler_properties *SamplerProperties,
                pi_sampler *RetSampler) {

  PI_ASSERT(Context, PI_ERROR_INVALID_CONTEXT);
  PI_ASSERT(RetSampler, PI_ERROR_INVALID_VALUE);

  _pi_context *PiContext = reinterpret_cast<_pi_context *>(Context);
  ur_context_handle_t UrContext = PiContext->UrContext;
  ur_sampler_property_t UrProps [6] {};
  UrProps[0] = UR_SAMPLER_PROPERTIES_NORMALIZED_COORDS;
  UrProps[1] = SamplerProperties[1];

  UrProps[2] = UR_SAMPLER_PROPERTIES_ADDRESSING_MODE;
  if (SamplerProperties[3] & PI_SAMPLER_ADDRESSING_MODE_MIRRORED_REPEAT)
    UrProps[3] = UR_SAMPLER_ADDRESSING_MODE_MIRRORED_REPEAT;
  else if (SamplerProperties[3] & PI_SAMPLER_ADDRESSING_MODE_REPEAT)
    UrProps[3] = UR_SAMPLER_ADDRESSING_MODE_REPEAT;
  else if (SamplerProperties[3] & PI_SAMPLER_ADDRESSING_MODE_CLAMP_TO_EDGE)
    UrProps[3] = UR_SAMPLER_ADDRESSING_MODE_CLAMP_TO_EDGE;
  else if (SamplerProperties[3] & PI_SAMPLER_ADDRESSING_MODE_CLAMP)
    UrProps[3] = UR_SAMPLER_ADDRESSING_MODE_CLAMP;
  else if (SamplerProperties[3] & PI_SAMPLER_ADDRESSING_MODE_NONE)
    UrProps[3] = UR_SAMPLER_ADDRESSING_MODE_NONE;

  UrProps[4] = UR_SAMPLER_PROPERTIES_FILTER_MODE;
  if (SamplerProperties[4] & PI_SAMPLER_FILTER_MODE_NEAREST)
    UrProps[5] = UR_EXT_SAMPLER_FILTER_MODE_NEAREST;
  else if (SamplerProperties[4] & PI_SAMPLER_FILTER_MODE_LINEAR)
    UrProps[5] = UR_EXT_SAMPLER_FILTER_MODE_LINEAR;

  ur_sampler_handle_t UrSampler {};

  HANDLE_ERRORS(urSamplerCreate(UrContext, 
                                UrProps,
                                &UrSampler));
  
  try {
    _pi_sampler *PiSampler = new _pi_sampler(UrSampler);
    *RetSampler = reinterpret_cast<pi_sampler>(PiSampler);
  } catch (const std::bad_alloc &) {
      return PI_ERROR_OUT_OF_RESOURCES;
  } catch (...) {
    return PI_ERROR_UNKNOWN;
  }

  return PI_SUCCESS;
}

inline pi_result
piSamplerGetInfo(pi_sampler Sampler,
                 pi_sampler_info ParamName,
                 size_t ParamValueSize,
                 void *ParamValue,
                 size_t *ParamValueSizeRet) {
  std::ignore = Sampler;
  std::ignore = ParamName;
  std::ignore = ParamValueSize;
  std::ignore = ParamValue;
  std::ignore = ParamValueSizeRet;

  die("piSamplerGetInfo: not implemented");
  return PI_SUCCESS;
}


// Special version of piKernelSetArg to accept pi_sampler.
inline pi_result
piextKernelSetArgSampler(pi_kernel Kernel,
                         pi_uint32 ArgIndex,
                         const pi_sampler *ArgValue) {
  _pi_kernel *PiKernel = reinterpret_cast<_pi_kernel *>(Kernel);
  ur_kernel_handle_t UrKernel = PiKernel->UrKernel;
  _pi_sampler *PiSampler = reinterpret_cast<_pi_sampler *>(const_cast<pi_sampler *>(ArgValue));
  ur_sampler_handle_t UrSampler = PiSampler->UrSampler;
  
  HANDLE_ERRORS(urKernelSetArgSampler(UrKernel,
                                      ArgIndex,
                                      UrSampler));

  return PI_SUCCESS;
}

inline pi_result
piSamplerRetain(pi_sampler Sampler) {
  PI_ASSERT(Sampler, PI_ERROR_INVALID_SAMPLER);

  _pi_sampler *PiSampler = reinterpret_cast<_pi_sampler *>(Sampler);
  ur_sampler_handle_t UrSampler = PiSampler->UrSampler;

  HANDLE_ERRORS(urSamplerRetain(UrSampler));

  return PI_SUCCESS;
}

inline pi_result
piSamplerRelease(pi_sampler Sampler) {
  PI_ASSERT(Sampler, PI_ERROR_INVALID_SAMPLER);

  _pi_sampler *PiSampler = reinterpret_cast<_pi_sampler *>(Sampler);
  ur_sampler_handle_t UrSampler = PiSampler->UrSampler;

  HANDLE_ERRORS(urSamplerRelease(UrSampler));

  if (!PiSampler->RefCount.decrementAndTest())
    return PI_SUCCESS;

  delete PiSampler;

  return PI_SUCCESS;
}

// Sampler
///////////////////////////////////////////////////////////////////////////////

} // namespace pi2ur
