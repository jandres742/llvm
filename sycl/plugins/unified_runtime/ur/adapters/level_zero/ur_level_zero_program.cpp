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
  // _ur_program_handle_t if there are unresolved symbols because the ZeBuildLog tells
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
    ur_context_handle_t Context, ///< [in] handle of the context instance
    ur_device_handle_t Device,            ///< [in] handle to device associated with binary.
    size_t Size,            ///< [in] size in bytes.
    const uint8_t *Binary, ///< [in] pointer to binary.
    ur_program_handle_t *Program ///< [out] pointer to handle of Program object created.
) {

  size_t Length = Size;

  // In OpenCL, clCreateProgramWithBinary() can be used to load any of the
  // following: "program executable", "compiled program", or "library of
  // compiled programs".  In addition, the loaded program can be either
  // IL (SPIR-v) or native device code.  For now, we assume that
  // piProgramCreateWithBinary() is only used to load a "program executable"
  // as native device code.
  // If we wanted to support all the same cases as OpenCL, we would need to
  // somehow examine the binary image to distinguish the cases.  Alternatively,
  // we could change the PI interface and have the caller pass additional
  // information to distinguish the cases.

  try {
    auto RetProgram = new _ur_program_handle_t(_ur_program_handle_t::Native,
                               Context,
                               Binary,
                               Length);
    *Program = reinterpret_cast<ur_program_handle_t>(RetProgram);
  } catch (const std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  return UR_RESULT_SUCCESS;
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

// Function gets characters between delimeter's in str
// then checks if they are equal to the sub_str.
// returns true if there is at least one instance
// returns false if there are no instances of the name
static bool is_in_separated_string(const std::string &str, char delimiter,
                                   const std::string &sub_str) {
  size_t beg = 0;
  size_t length = 0;
  for (const auto &x : str) {
    if (x == delimiter) {
      if (str.substr(beg, length) == sub_str)
        return true;

      beg += length + 1;
      length = 0;
      continue;
    }
    length++;
  }
  if (length != 0)
    if (str.substr(beg, length) == sub_str)
      return true;

  return false;
}


UR_APIEXPORT ur_result_t UR_APICALL urProgramGetFunctionPointer(
    ur_device_handle_t Device, ///< [in] handle of the device to retrieve pointer for.
    ur_program_handle_t Program, ///< [in] handle of the program to search for function in.
                  ///< The program must already be built to the specified
                  ///< device, or otherwise
                  ///< ::UR_RESULT_ERROR_INVALID_PROGRAM_EXECUTABLE is returned.
    const char *FunctionName, ///< [in] A null-terminates string denoting the
                               ///< mangled function name.
    void **FunctionPointerRet   ///< [out] Returns the pointer to the function if
                               ///< it is found in the program.
) {
  std::ignore = Device;

  std::shared_lock<pi_shared_mutex> Guard(Program->Mutex);
  if (Program->State != _ur_program_handle_t::Exe) {
    return UR_RESULT_ERROR_INVALID_PROGRAM_EXECUTABLE;
  }

  ze_result_t ZeResult =
      ZE_CALL_NOCHECK(zeModuleGetFunctionPointer, (Program->ZeModule,
                                                   FunctionName,
                                                   FunctionPointerRet));

  // zeModuleGetFunctionPointer currently fails for all
  // kernels regardless of if the kernel exist or not
  // with ZE_RESULT_ERROR_INVALID_ARGUMENT
  // TODO: remove when this is no longer the case
  // If zeModuleGetFunctionPointer returns invalid argument,
  // fallback to searching through kernel list and return
  // PI_ERROR_FUNCTION_ADDRESS_IS_NOT_AVAILABLE if the function exists
  // or PI_ERROR_INVALID_KERNEL_NAME if the function does not exist.
  // FunctionPointerRet should always be 0
  if (ZeResult == ZE_RESULT_ERROR_INVALID_ARGUMENT) {
    size_t Size;
    *FunctionPointerRet = 0;
    UR_CALL(urProgramGetInfo(Program,
                             UR_PROGRAM_INFO_KERNEL_NAMES,
                             0,
                             nullptr,
                             &Size));

    std::string ClResult(Size, ' ');
    UR_CALL(urProgramGetInfo(Program,
                             UR_PROGRAM_INFO_KERNEL_NAMES,
                             ClResult.size(),
                             &ClResult[0],
                             nullptr));

    // Get rid of the null terminator and search for kernel_name
    // If function can be found return error code to indicate it
    // exists
    ClResult.pop_back();
    if (is_in_separated_string(ClResult, ';', std::string(FunctionName)))
      return UR_RESULT_ERROR_INVALID_FUNCTION_NAME;

    return UR_RESULT_ERROR_INVALID_KERNEL_NAME;
  }

  if (ZeResult == ZE_RESULT_ERROR_INVALID_FUNCTION_NAME) {
    *FunctionPointerRet = 0;
    return UR_RESULT_ERROR_INVALID_KERNEL_NAME;
  }

  return ze2urResult(ZeResult);
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramGetInfo(
    ur_program_handle_t Program, ///< [in] handle of the Program object
    ur_program_info_t PropName, ///< [in] name of the Program property to query
    size_t PropSize,            ///< [in] the size of the Program property.
    void *ProgramInfo,  ///< [in,out][optional] array of bytes of holding the
                         ///< program info property. If propSize is not equal to
                         ///< or greater than the real number of bytes needed to
                         ///< return the info then the
                         ///< ::UR_RESULT_ERROR_INVALID_SIZE error is returned
                         ///< and pProgramInfo is not used.
    size_t *PropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data copied to propName.
) {
  UrReturnHelper ReturnValue(PropSize, ProgramInfo, PropSizeRet);

  switch (PropName) {
  case UR_PROGRAM_INFO_REFERENCE_COUNT:
    return ReturnValue(pi_uint32{Program->RefCount.load()});
  case UR_PROGRAM_INFO_NUM_DEVICES:
    // TODO: return true number of devices this program exists for.
    return ReturnValue(pi_uint32{1});
  case UR_PROGRAM_INFO_DEVICES:
    // TODO: return all devices this program exists for.
    return ReturnValue(Program->Context->Devices[0]);
  case UR_PROGRAM_INFO_BINARY_SIZES: {
    std::shared_lock<pi_shared_mutex> Guard(Program->Mutex);
    size_t SzBinary;
    if (Program->State == _ur_program_handle_t::IL ||
        Program->State == _ur_program_handle_t::Native ||
        Program->State == _ur_program_handle_t::Object) {
      SzBinary = Program->CodeLength;
    } else if (Program->State == _ur_program_handle_t::Exe) {
      ZE2UR_CALL(zeModuleGetNativeBinary, (Program->ZeModule,
                                           &SzBinary,
                                           nullptr));
    } else {
      return UR_RESULT_ERROR_INVALID_PROGRAM;
    }
    // This is an array of 1 element, initialized as if it were scalar.
    return ReturnValue(size_t{SzBinary});
  }
  case UR_PROGRAM_INFO_BINARIES: {
    // The caller sets "ParamValue" to an array of pointers, one for each
    // device.  Since Level Zero supports only one device, there is only one
    // pointer.  If the pointer is NULL, we don't do anything.  Otherwise, we
    // copy the program's binary image to the buffer at that pointer.
    uint8_t **PBinary = ur_cast<uint8_t **>(ProgramInfo);
    if (!PBinary[0])
      break;

    std::shared_lock<pi_shared_mutex> Guard(Program->Mutex);
    if (Program->State == _ur_program_handle_t::IL ||
        Program->State == _ur_program_handle_t::Native ||
        Program->State == _ur_program_handle_t::Object) {
      std::memcpy(PBinary[0], Program->Code.get(), Program->CodeLength);
    } else if (Program->State == _ur_program_handle_t::Exe) {
      size_t SzBinary = 0;
      ZE2UR_CALL(zeModuleGetNativeBinary, (Program->ZeModule,
                                           &SzBinary,
                                           PBinary[0]));
    } else {
      return UR_RESULT_ERROR_INVALID_PROGRAM;
    }
    break;
  }
  case UR_PROGRAM_INFO_NUM_KERNELS: {
    std::shared_lock<pi_shared_mutex> Guard(Program->Mutex);
    uint32_t NumKernels;
    if (Program->State == _ur_program_handle_t::IL ||
        Program->State == _ur_program_handle_t::Native ||
        Program->State == _ur_program_handle_t::Object) {
      return UR_RESULT_ERROR_INVALID_PROGRAM_EXECUTABLE;
    } else if (Program->State == _ur_program_handle_t::Exe) {
      NumKernels = 0;
      ZE2UR_CALL(zeModuleGetKernelNames, (Program->ZeModule,
                                          &NumKernels,
                                          nullptr));
    } else {
      return UR_RESULT_ERROR_INVALID_PROGRAM;
    }
    return ReturnValue(size_t{NumKernels});
  }
  case UR_PROGRAM_INFO_KERNEL_NAMES:
    try {
      std::shared_lock<pi_shared_mutex> Guard(Program->Mutex);
      std::string PINames{""};
      if (Program->State == _ur_program_handle_t::IL ||
          Program->State == _ur_program_handle_t::Native ||
          Program->State == _ur_program_handle_t::Object) {
        return UR_RESULT_ERROR_INVALID_PROGRAM_EXECUTABLE;
      } else if (Program->State == _ur_program_handle_t::Exe) {
        uint32_t Count = 0;
        ZE2UR_CALL(zeModuleGetKernelNames, (Program->ZeModule, &Count, nullptr));
        std::unique_ptr<const char *[]> PNames(new const char *[Count]);
        ZE2UR_CALL(zeModuleGetKernelNames, (Program->ZeModule,
                                            &Count,
                                            PNames.get()));
        for (uint32_t I = 0; I < Count; ++I) {
          PINames += (I > 0 ? ";" : "");
          PINames += PNames[I];
        }
      } else {
        return UR_RESULT_ERROR_INVALID_PROGRAM;
      }
      return ReturnValue(PINames.c_str());
    } catch (const std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }
  default:
    die("urProgramGetInfo: not implemented");
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramGetBuildInfo(
    ur_program_handle_t Program, ///< [in] handle of the Program object
    ur_device_handle_t Device,   ///< [in] handle of the Device object
    ur_program_build_info_t PropName,     ///< [in] name of the Program build info to query
    size_t PropSize,  ///< [in] size of the Program build info property.
    void *PropValue, ///< [in,out][optional] value of the Program build
                      ///< property. If propSize is not equal to or greater than
                      ///< the real number of bytes needed to return the info
                      ///< then the ::UR_RESULT_ERROR_INVALID_SIZE error is
                      ///< returned and pKernelInfo is not used.
    size_t *PropSizeRet ///< [out][optional] pointer to the actual size in
                         ///< bytes of data being queried by propName.
) {
  (void)Device;

  std::shared_lock<pi_shared_mutex> Guard(Program->Mutex);
  UrReturnHelper ReturnValue(PropSize, PropValue, PropSizeRet);
  if (PropName == UR_PROGRAM_BUILD_INFO_BINARY_TYPE) {
    ur_program_binary_type_t Type = UR_PROGRAM_BINARY_TYPE_NONE;
    if (Program->State == _ur_program_handle_t::Object) {
      Type = UR_PROGRAM_BINARY_TYPE_COMPILED_OBJECT;
    } else if (Program->State == _ur_program_handle_t::Exe) {
      Type = UR_PROGRAM_BINARY_TYPE_EXECUTABLE;
    }
    return ReturnValue(ur_program_binary_type_t{Type});
  }
  if (PropName == UR_PROGRAM_BUILD_INFO_OPTIONS) {
    // TODO: how to get module build options out of Level Zero?
    // For the programs that we compiled we can remember the options
    // passed with piProgramCompile/piProgramBuild, but what can we
    // return for programs that were built outside and registered
    // with piProgramRegister?
    return ReturnValue("");
  } else if (PropName == UR_PROGRAM_BUILD_INFO_LOG) {
    // Check first to see if the plugin code recorded an error message.
    if (!Program->ErrorMessage.empty()) {
      return ReturnValue(Program->ErrorMessage.c_str());
    }

    // Next check if there is a Level Zero build log.
    if (Program->ZeBuildLog) {
      size_t LogSize = PropSize;
      ZE2UR_CALL(zeModuleBuildLogGetString, (Program->ZeBuildLog,
                                             &LogSize,
                                             ur_cast<char *>(PropValue)));
      if (PropSizeRet) {
        *PropSizeRet = LogSize;
      }
      return UR_RESULT_SUCCESS;
    }

    // Otherwise, there is no error.  The OpenCL spec says to return an empty
    // string if there ws no previous attempt to compile, build, or link the
    // program.
    return ReturnValue("");
  } else {
    zePrint("urProgramGetBuildInfo: unsupported ParamName\n");
    return UR_RESULT_ERROR_INVALID_VALUE;
  }
  return UR_RESULT_SUCCESS;
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
    ur_program_handle_t Program,       ///< [in] handle of the program.
    ur_native_handle_t *NativeProgram ///< [out] a pointer to the native
                                        ///< handle of the program.
) {
  auto ZeModule = ur_cast<ze_module_handle_t *>(NativeProgram);

  std::shared_lock<pi_shared_mutex> Guard(Program->Mutex);
  switch (Program->State) {
  case _ur_program_handle_t::Exe: {
    *ZeModule = Program->ZeModule;
    break;
  }

  default:
    return UR_RESULT_ERROR_INVALID_OPERATION;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramCreateWithNativeHandle(
    ur_native_handle_t NativeProgram,            ///< [in] the native handle of the program.
    ur_context_handle_t Context,  ///< [in] handle of the context instance
    ur_program_handle_t *Program ///< [out] pointer to the handle of the
                                   ///< program object created.
) {
  auto ZeModule = ur_cast<ze_module_handle_t>(NativeProgram);

  // We assume here that programs created from a native handle always
  // represent a fully linked executable (state Exe) and not an unlinked
  // executable (state Object).

  try {
    _ur_program_handle_t *UrProgram = new _ur_program_handle_t(_ur_program_handle_t::Exe,
                                                               Context,
                                                               ZeModule);
    *Program = reinterpret_cast<ur_program_handle_t>(UrProgram);
  } catch (const std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }
  return UR_RESULT_SUCCESS;
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

