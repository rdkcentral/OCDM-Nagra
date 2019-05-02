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

#include <interfaces/IDRM.h> 

#include "../IMediaSessionConnect.h"
#include "../IMediaSessionSystem.h"

namespace CDMi {

class MediaSessionConnect : public IMediaKeySession, public IMediaSessionConnect {
public:
    enum request {
        ECMDELIVERY  = 0x10000
    };

public:
    //static const std::vector<std::string> m_mimeTypes;

    MediaSessionConnect(const uint8_t *f_pbInitData, uint32_t f_cbInitData);
    ~MediaSessionConnect();

    MediaSessionConnect(const MediaSessionConnect&) = delete;
    MediaSessionConnect& operator=(const MediaSessionConnect&) = delete;

    // IMediaSessionConnect overrides
    virtual void Run(const IMediaKeySessionCallback *callback) override;
    virtual void Update( const uint8_t *response, uint32_t responseLength) override;
    virtual CDMi_RESULT Load() override;
    virtual CDMi_RESULT Remove() override;
    virtual CDMi_RESULT Close() override;
    virtual const char *GetSessionId() const override;
    virtual const char *GetKeySystem() const override;
    virtual CDMi_RESULT Decrypt(
        const uint8_t *f_pbSessionKey,
        uint32_t f_cbSessionKey,
        const uint32_t *f_pdwSubSampleMapping,
        uint32_t f_cdwSubSampleMapping,
        const uint8_t *f_pbIV,
        uint32_t f_cbIV,
        const uint8_t *f_pbData,
        uint32_t f_cbData,
        uint32_t *f_pcbOpaqueClearContent,
        uint8_t **f_ppbOpaqueClearContent,
        const uint8_t keyIdLength,
        const uint8_t* keyId,
        bool initWithLast15);
    virtual CDMi_RESULT ReleaseClearContent(
        const uint8_t *f_pbSessionKey,
        uint32_t f_cbSessionKey,
        const uint32_t  f_cbClearContentOpaque,
        uint8_t  *f_pbClearContentOpaque );

    // Note: the KeyUsable (and other) events are currently not (yet) triggered as it is not required by the current nagra client. 
    //       In the future (e.g. before connect it to the WPEWebkit) this should be connected to the appicable Nagra callback (we are not sure
    //       at this moment which one) to make this work with Clients that expect this callback
    // virtual void OnKeyStatusUpdate(const char* keyMessage, const uint8_t* buffer, const uint8_t length) = 0;


    // IMediaSessionConnect overrides
    void OnKeyMessage(const uint8_t *f_pbKeyMessage, const uint32_t f_cbKeyMessage, const char *f_pszUrl) override;

private:
    constexpr static  const char* const g_NAGRASessionIDPrefix = { "NSCID:" };
    
    std::string _sessionId;
    IMediaKeySessionCallback* _callback; //note the Run() changing this member can be from another thread then the callback is being called. The MediaSessionConnect itself is protected in the lock in the systemsession. But we should protect againts the callback being deleted while called
    TNvSession _descramblingSession;
    uint32_t _TSID;
    IMediaSessionSystem* _systemsession;
    WPEFramework::Core::CriticalSection _lock;
};

} // namespace CDMi
