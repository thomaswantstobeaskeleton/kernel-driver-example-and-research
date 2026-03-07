#pragma once
/* Mapper anti-detection config. Include before LoadDriver in includes.h */

/* Mapper output name: auto-generated from mapper_config.props (MapperOutputName).
 * Edit dependencies/configs/mapper_config.props - MapperOutputName=kdmapper (default) or IntelCpHDCP.
 * Both kdmapper build and loader use the same value. */
#if __has_include("mapper_output.h")
#include "mapper_output.h"
#endif
#ifndef MAPPER_EXE_BENIGN_NAME
#define MAPPER_EXE_BENIGN_NAME ""
#endif

/* Download URL: obfuscated via OBF_STR in includes.h. Override with #define MAPPER_DOWNLOAD_URL
 * before includes.h to use different URL (also wrap in OBF_STR for obfuscation). */
