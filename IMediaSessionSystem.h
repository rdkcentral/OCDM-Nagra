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

#include <nagra/prm_asm.h>
#include <nagra/prm_dsm.h>

namespace CDMi {

struct IMediaSessionConnect;

struct IMediaSessionSystem {

    virtual TNvSession OpenDescramblingSession(IMediaSessionConnect* session, const uint32_t TSID, const uint16_t Emi) = 0; //returns Descramlbingsession ID
    virtual void CloseDescramblingSession(TNvSession session, const uint32_t TSID) = 0;

    virtual void SetPrmContentMetadata(TNvSession descamblingsession, TNvBuffer* data, TNvStreamType streamtype) = 0;
    virtual void SetPlatformMetadata(TNvSession descamblingsession, const uint32_t TSID, uint8_t *data, size_t size) = 0;

    virtual void Addref() const = 0;
    virtual uint32_t Release() const = 0;

};
  
#ifdef __cplusplus
extern "C" {
#endif

    CDMi::IMediaSessionSystem* GetMediaSessionSystemInterface(const char* systemsessionid);

#ifdef __cplusplus
}
#endif


} // namespace CDMi
