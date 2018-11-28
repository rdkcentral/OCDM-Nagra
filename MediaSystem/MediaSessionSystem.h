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

#include <nagra/nagra_cma_platf.h>
#include <nagra/prm_asm.h>
#include <nagra/nv_imsm.h>

#include <vector>
#include <set>
#include <map>

#include "../IMediaSessionSystem.h"
#include "../IMediaSessionConnect.h"
#include "../MediaRequest.h"

namespace CDMi {

class MediaSessionSystem : public IMediaKeySession, public IMediaSessionSystem {
private:
    MediaSessionSystem(const uint8_t *data, uint32_t length, const std::string& operatorvault, const std::string& licensepath);
    ~MediaSessionSystem();

public:    
    MediaSessionSystem(const MediaSessionSystem&) = delete;
    MediaSessionSystem& operator=(const MediaSessionSystem&) = delete;

    static IMediaKeySession* CreateMediaSessionSystem(const uint8_t *f_pbInitData, uint32_t f_cbInitData, const std::string& operatorvault, const std::string& licensepath);
    static void DestroyMediaSessionSystem(IMediaKeySession* session);

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

    // IMediaSessionSystem overrides
    TNvSession OpenDescramblingSession(IMediaSessionConnect* session, const uint32_t TSID, const uint16_t Emi) override;
    void CloseDescramblingSession(TNvSession session, const uint32_t TSID) override;
    void SetPrmContentMetadata(TNvSession descamblingsession, TNvBuffer* data, TNvStreamType streamtype) override;
    void SetPlatformMetadata(TNvSession descamblingsession, const uint32_t TSID, uint8_t *data, size_t size) override;


    virtual void Addref() const override;
    virtual uint32_t Release() const override;


private:
    using FilterStorage = std::vector<TNvFilter>;
    using ConnectSessionStorage = std::map<TNvSession, IMediaSessionConnect*>;
    using DeliverySessionsStorage = std::set<TNvSession>;
    using DataBuffer = std::vector<uint8_t>;

    static bool OnRenewal(TNvSession appSession);
    static bool OnNeedKey(TNvSession appSession, TNvSession descramblingSession, TNvKeyStatus keyStatus,  TNvBuffer* content, TNvStreamType streamtype);
    static bool OnDeliveryCompleted(TNvSession deliverySession);

    void OnNeedKey(TNvSession descramblingSession, TNvKeyStatus keyStatus,  TNvBuffer* content, TNvStreamType streamtype);
    void OnRenewal();
    void OnDeliverySessionCompleted(TNvSession deliverySession);
    void GetFilters(FilterStorage& filters);
    void GetProvisionChallenge(DataBuffer& buffer);
    void InitializeWhenProvisoned();
    void HandleFilters();

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

    inline TNvSession OpenKeyNeedSession();
    inline void OpenRenewalSession();

    inline TNvSession OpenDeliverySession();
    inline void CloseDeliverySession(TNvSession session);

    void CreateRenewalExchange(DataBuffer& buffer);

    constexpr static const char* const g_NAGRASessionIDPrefix = { "NAGRA_SESSIONSYSTEM_ID:" };

    std::string _sessionId;
    IMediaKeySessionCallback* _callback;
    Request _requests;
    TNvSession _applicationSession;
    TNvSession  _inbandSession;
    DeliverySessionsStorage _needKeySessions;
    TNvSession  _renewalSession;
    TNvSession  _provioningSession;
    ConnectSessionStorage _connectsessions;
    std::string _licensepath;
    mutable uint32_t _referenceCount;
    
};

} // namespace CDMi
