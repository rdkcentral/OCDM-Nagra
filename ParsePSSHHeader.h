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

#include "Report.h"

#include <core/core.h>

namespace CDMi {
    //input: pointer to pssh header data and length of total buffer, output: pointer at position of private data and returns length of private data and negative value in case of invalid pssh header
    int32_t FindPSSHHeaderPrivateData(const uint8_t*& data, const uint32_t length);  

} // namespace CDMi
