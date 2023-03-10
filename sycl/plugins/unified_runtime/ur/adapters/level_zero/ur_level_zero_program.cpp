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
#include "ur_level_zero_program.hpp"
#include <ur_bindings.hpp>

// Check to see if a Level Zero module has any unresolved symbols.
//
// @param ZeModule    The module handle to check.
// @param ZeBuildLog  If there are unresolved symbols, this build log handle is
//                     modified to receive information telling which symbols
//                     are unresolved.
//
// @return ZE_RESULT_ERROR_MODULE_LINK_FAILURE indicates there are unresolved
//  symbols.  ZE_RESULT_SUCCESS indicates all symbols are resolved.  Any other
//  value indicates there was an error and we cannot tell if symbols are
//  resolved.
static ze_result_t
checkUnresolvedSymbols(ze_module_handle_t ZeModule,
                       ze_module_build_log_handle_t *ZeBuildLog) {

  // First check to see if the module has any imported symbols.  If there are
  // no imported symbols, it's not possible to have any unresolved symbols.  We
  // do this check first because we assume it's faster than the call to
  // zeModuleDynamicLink below.
  ZeStruct<ze_module_properties_t> ZeModuleProps;
  ze_result_t ZeResult =
      ZE_CALL_NOCHECK(zeModuleGetProperties, (ZeModule, &ZeModuleProps));
  if (ZeResult != ZE_RESULT_SUCCESS)
    return ZeResult;

  // If there are imported symbols, attempt to "link" the module with itself.
  // As a side effect, this will return the error
  // ZE_RESULT_ERROR_MODULE_LINK_FAILURE if there are any unresolved symbols.
  if (ZeModuleProps.flags & ZE_MODULE_PROPERTY_FLAG_IMPORTS) {
    return ZE_CALL_NOCHECK(zeModuleDynamicLink, (1, &ZeModule, ZeBuildLog));
  }
  return ZE_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramCreate(
    ur_context_handle_t hContext, ///< [in] handle of the context instance
    uint32_t count, ///< [in] number of module handles in module list.
    const ur_module_handle_t
        *phModules, ///< [in][range(0, count)] pointer to array of modules.
    const char *pOptions, ///< [in][optional] pointer to linker options
                          ///< null-terminated string.
    ur_program_handle_t
        *phProgram ///< [out] pointer to handle of program object created.
) {
  printf("%s %d\n", __FILE__, __LINE__);
  // Construct a ze_module_program_exp_desc_t which contains information about
  // all of the modules that will be linked together.
  ZeStruct<ze_module_program_exp_desc_t> ZeModuleDescExp;
  std::vector<size_t> CodeSizes(count);
  std::vector<const uint8_t *> CodeBufs(count);
  std::vector<const char *> BuildFlagPtrs(count);

  for (uint32_t i = 0; i < count; i++) {
    _ur_module_handle_t *UrModule = reinterpret_cast<_ur_module_handle_t *>(phModules[i]);
    CodeSizes[i] = UrModule->length;
    CodeBufs[i] = reinterpret_cast<const uint8_t*>(const_cast<const void *>(UrModule->pIL));
  }

printf("%s %d\n", __FILE__, __LINE__);
  ZeModuleDescExp.count = count;
  ZeModuleDescExp.inputSizes = CodeSizes.data();
  printf("%s %d CodeSizes.size() %zd\n", __FILE__, __LINE__, CodeSizes.size());
  ZeModuleDescExp.pInputModules = CodeBufs.data();
  ZeModuleDescExp.pBuildFlags = BuildFlagPtrs.data();
printf("%s %d\n", __FILE__, __LINE__);
  ZeStruct<ze_module_desc_t> ZeModuleDesc;
  ZeModuleDesc.pNext = &ZeModuleDescExp;
  ZeModuleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
printf("%s %d\n", __FILE__, __LINE__);
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
  printf("%s %d\n", __FILE__, __LINE__);
  ZeModuleDesc.pNext = nullptr;
  ZeModuleDesc.inputSize = ZeModuleDescExp.inputSizes[0];
  ZeModuleDesc.pInputModule = ZeModuleDescExp.pInputModules[0];
  ZeModuleDesc.pBuildFlags = ZeModuleDescExp.pBuildFlags[0];
#if 0
  ZeModuleDesc.pConstants = ZeModuleDescExp.pConstants[0];
#endif
  printf("%s %d\n", __FILE__, __LINE__);
  // Call the Level Zero API to compile, link, and create the module.
  _ur_context_handle_t *UrContext = reinterpret_cast<_ur_context_handle_t *>(hContext);
  ze_device_handle_t ZeDevice = UrContext->Devices[0]->ZeDevice;
  ze_context_handle_t ZeContext = UrContext->ZeContext;
  ze_module_handle_t ZeModule = nullptr;
  ze_module_build_log_handle_t ZeBuildLog = nullptr;
  ze_result_t ZeResult = zeModuleCreate(ZeContext,
                              ZeDevice, &ZeModuleDesc,
                              &ZeModule, &ZeBuildLog);
  printf("%s %d\n", __FILE__, __LINE__);
  // The call to zeModuleCreate does not report an error if there are
  // unresolved symbols because it thinks these could be resolved later via a
  // call to zeModuleDynamicLink.  However, modules created with piProgramLink
  // are supposed to be fully linked and ready to use.  Therefore, do an extra
  // check now for unresolved symbols.  Note that we still create a
  // _pi_program if there are unresolved symbols because the ZeBuildLog tells
  // which symbols are unresolved.
  ur_result_t UrResult = UR_RESULT_SUCCESS;
  if (ZeResult == ZE_RESULT_SUCCESS) {
    ZeResult = checkUnresolvedSymbols(ZeModule, &ZeBuildLog);
    if (ZeResult == ZE_RESULT_ERROR_MODULE_LINK_FAILURE) {
      UrResult = UR_RESULT_ERROR_MODULE_LINK_FAILURE;
    } else if (ZeResult != ZE_RESULT_SUCCESS) {
      return ze2urResult(ZeResult);
    }
  }
  _ur_program_handle_t *UrProgram = new _ur_program_handle_t(_ur_program_handle_t::Exe,
                                         hContext,
                                         ZeModule,
                                         nullptr);

  *phProgram = reinterpret_cast<ur_program_handle_t>(UrProgram);

  printf("%s %d\n", __FILE__, __LINE__);
  return UrResult;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramCreateWithBinary(
    ur_context_handle_t hContext, ///< [in] handle of the context instance
    ur_device_handle_t
        hDevice,            ///< [in] handle to device associated with binary.
    size_t size,            ///< [in] size in bytes.
    const uint8_t *pBinary, ///< [in] pointer to binary.
    ur_program_handle_t
        *phProgram ///< [out] pointer to handle of Program object created.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramRetain(
    ur_program_handle_t hProgram ///< [in] handle for the Program to retain
) {
  hProgram->RefCount.increment();
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramRelease(
    ur_program_handle_t Program ///< [in] handle for the Program to release
) {
  if (!Program->RefCount.decrementAndTest())
    return UR_RESULT_SUCCESS;
  
  printf("%s %d Program %lx\n", __FILE__, __LINE__, (unsigned long int)Program);
  delete Program;

  printf("%s %d\n", __FILE__, __LINE__);
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramGetFunctionPointer(
    ur_device_handle_t
        hDevice, ///< [in] handle of the device to retrieve pointer for.
    ur_program_handle_t
        hProgram, ///< [in] handle of the program to search for function in.
                  ///< The program must already be built to the specified
                  ///< device, or otherwise
                  ///< ::UR_RESULT_ERROR_INVALID_PROGRAM_EXECUTABLE is returned.
    const char *pFunctionName, ///< [in] A null-terminates string denoting the
                               ///< mangled function name.
    void **ppFunctionPointer   ///< [out] Returns the pointer to the function if
                               ///< it is found in the program.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramGetInfo(
    ur_program_handle_t hProgram, ///< [in] handle of the Program object
    ur_program_info_t propName, ///< [in] name of the Program property to query
    size_t propSize,            ///< [in] the size of the Program property.
    void *pProgramInfo,  ///< [in,out][optional] array of bytes of holding the
                         ///< program info property. If propSize is not equal to
                         ///< or greater than the real number of bytes needed to
                         ///< return the info then the
                         ///< ::UR_RESULT_ERROR_INVALID_SIZE error is returned
                         ///< and pProgramInfo is not used.
    size_t *pPropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data copied to propName.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramGetBuildInfo(
    ur_program_handle_t hProgram, ///< [in] handle of the Program object
    ur_device_handle_t hDevice,   ///< [in] handle of the Device object
    ur_program_build_info_t
        propName,     ///< [in] name of the Program build info to query
    size_t propSize,  ///< [in] size of the Program build info property.
    void *pPropValue, ///< [in,out][optional] value of the Program build
                      ///< property. If propSize is not equal to or greater than
                      ///< the real number of bytes needed to return the info
                      ///< then the ::UR_RESULT_ERROR_INVALID_SIZE error is
                      ///< returned and pKernelInfo is not used.
    size_t *pPropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data being queried by propName.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramSetSpecializationConstant(
    ur_program_handle_t hProgram, ///< [in] handle of the Program object
    uint32_t specId,              ///< [in] specification constant Id
    size_t specSize,       ///< [in] size of the specialization constant value
    const void *pSpecValue ///< [in] pointer to the specialization value bytes
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramGetNativeHandle(
    ur_program_handle_t hProgram,       ///< [in] handle of the program.
    ur_native_handle_t *phNativeProgram ///< [out] a pointer to the native
                                        ///< handle of the program.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramCreateWithNativeHandle(
    ur_native_handle_t
        hNativeProgram,            ///< [in] the native handle of the program.
    ur_context_handle_t hContext,  ///< [in] handle of the context instance
    ur_program_handle_t *phProgram ///< [out] pointer to the handle of the
                                   ///< program object created.
) {
  zePrint("[UR][L0] %s function not implemented!\n", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

_ur_program_handle_t::~_ur_program_handle_t() {
  // According to Level Zero Specification, all kernels and build logs
  // must be destroyed before the Module can be destroyed.  So, be sure
  // to destroy build log before destroying the module.
  printf("ZeBuildLog %lx\n", (unsigned long int)ZeBuildLog);
  if (ZeBuildLog) {
    printf("%s %d\n", __FILE__, __LINE__);
    ZE_CALL_NOCHECK(zeModuleBuildLogDestroy, (ZeBuildLog));
  }

  printf("ZeModule %lx OwnZeModule %d\n", (unsigned long int)ZeModule, OwnZeModule);
  if (ZeModule && OwnZeModule) {
    printf("%s %d\n", __FILE__, __LINE__);
    ZE_CALL_NOCHECK(zeModuleDestroy, (ZeModule));
  }
}

