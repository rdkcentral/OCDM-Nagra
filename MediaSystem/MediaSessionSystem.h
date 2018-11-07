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

#include <interfaces/IDRM.h> 

#include <nagra/prm_asm.h>
#include <nagra/nv_imsm.h>

#include <vector>

namespace CDMi {

class MediaSessionSystem : public IMediaKeySession {
private:
    using requestsSize = uint32_t; // do not just increase the size, part of the interface specification!

    enum class Request : requestsSize {
        FILTERS      = 0x0001,
        KEYREADY     = 0x0002,
        KEYNEEDED    = 0x0004,
        RENEWAL      = 0x0008,
        EMMDELIVERY  = 0x0010,
        PROVISION    = 0x0020,
        ECMDELIVERY  = 0x0040,
    };

public:
    //static const std::vector<std::string> m_mimeTypes;

    MediaSessionSystem(const uint8_t *f_pbInitData, uint32_t f_cbInitData);
    ~MediaSessionSystem();

    // IMediaSessionSystem overrides
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
        const uint8_t* keyId);
    virtual CDMi_RESULT ReleaseClearContent(
        const uint8_t *f_pbSessionKey,
        uint32_t f_cbSessionKey,
        const uint32_t  f_cbClearContentOpaque,
        uint8_t  *f_pbClearContentOpaque );

private:
    using FilterStorage = std::vector<TNvFilter>;

    static bool OnRenewal(TNvSession appSession);
    static bool OnNeedKey(TNvSession appSession, TNvSession descramblingSession, TNvKeyStatus keyStatus,  TNvBuffer* content, TNvStreamType streamtype);
    static bool OnDeliveryCompleted(TNvSession deliverySession);

    void OnNeedKey(TNvSession descramblingSession, TNvKeyStatus keyStatus,  TNvBuffer* content, TNvStreamType streamtype);
    void OnRenewal();
    void OnDeliveryCompleted();
    FilterStorage GetFilters();
    std::string GetProvisionChallenge();

    void CloseDeliverySession();
    void CloseProvisioningSession();

    void RequestReceived(Request request) {
        _requests = static_cast<Request>(static_cast<requestsSize>(_requests) | static_cast<requestsSize>(request));
    }

    void RequestHandled(Request request) {
        _requests = static_cast<Request>(static_cast<requestsSize>(_requests) & (~static_cast<requestsSize>(request)));
    }

    bool WasRequestReceived(Request request) const {
        return static_cast<Request>(static_cast<requestsSize>(_requests) & static_cast<requestsSize>(request)) == request;
    }

    std::string CreateRenewalExchange();

    static constexpr const char* const g_NAGRASessionIDPrefix { "NAGRA_SESSIONSYSTEM_ID:" };

    std::string _sessionId;
    IMediaKeySessionCallback* _callback;
    Request _requests;
    TNvSession _applicationSession;
    TNvSession  _inbandSession;
    TNvSession _deliverySession;
    TNvSession  _provioningSession;

};

} // namespace CDMi
