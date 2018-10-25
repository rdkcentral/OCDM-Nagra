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

namespace CDMi {

class OperatorVault {
public:
    OperatorVault(const OperatorVault&) = delete;
    OperatorVault& operator=(const OperatorVault&) = delete;

    explicit OperatorVault(const string& path);
    ~OperatorVault() = default;

    // IMediaSessionConnect overrides
    string LoadOperatorVault() const;

private:
  WPEFramework::Core::DataElementFile _file;
};

} // namespace CDMi
