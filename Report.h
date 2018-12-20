/*
 * Copyright 2018 Metrological
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <core/core.h>

#define REPORT_PRM_EXT(success, result, callname, x, ...) 							\
    if( result != success ) { \
        fprintf(stderr, "Call to %s failed, result = [%d]" #x "\n", callname, result, __VA_ARGS__);	\
        fflush(stderr);  \
    }  

#define REPORT_PRM(success, result, callname) 							\
    if( result != success ) { \
        fprintf(stderr, "Call to %s failed, result = [%d]\n", callname, result);	\
        fflush(stderr);  \
    }  

#define REPORT(x) 							\
    fprintf(stderr, #x "\n");	\
    fflush(stderr);  

#define REPORT_EXT(x, ...) 							\
    fprintf(stderr, #x "\n", __VA_ARGS__);	\
    fflush(stderr);  

#define REPORT_ASM_EXT(result, callname, x, ...) REPORT_PRM_EXT(NV_ASM_SUCCESS, callname, x, ...)

#define REPORT_ASM(result, callname) REPORT_PRM(NV_ASM_SUCCESS, result, callname)

#define REPORT_DSM_EXT(result, callname, x, ...) REPORT_PRM_EXT(NV_DSM_SUCCESS, callname, x, ...)

#define REPORT_DSM(result, callname) REPORT_PRM(NV_DSM_SUCCESS, result, callname)

#define REPORT_LDS_EXT(result, callname, x, ...) REPORT_PRM_EXT(NV_LDS_SUCCESS, callname, x, ...)

#define REPORT_LDS(result, callname) REPORT_PRM(NV_LDS_SUCCESS, result, callname)

#define REPORT_IMSM_EXT(result, callname, x, ...) REPORT_PRM_EXT(NV_IMSM_SUCCESS, callname, x, ...)

#define REPORT_IMSM(result, callname) REPORT_PRM(NV_IMSM_SUCCESS, result, callname)

#define REPORT_DPSC_EXT(result, callname, x, ...) REPORT_PRM_EXT(NV_DPSC_SUCCESS, callname, x, ...)

#define REPORT_DPSC(result, callname) REPORT_PRM(NV_DPSC_SUCCESS, result, callname)


static void DumpData(const char* buffername, const uint8_t data[], const uint16_t size) {
  printf("Data for [%s] with length [%d]:\n", buffername, size);
  if( size != 0 && data != nullptr) {
    for (uint16_t teller = 0; teller < size; teller++) {
        printf("%02X ", data[teller]);
        if (((teller + 1) & 0x7) == 0) {
        printf("\n");
        }
    }
  }
  printf("\n\n");
  fflush(stdout);
}