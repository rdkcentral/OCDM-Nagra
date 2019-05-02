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
#include <forward_list>

#include "../IMediaSessionSystem.h"
#include "../IMediaSessionConnect.h"
#include "../MediaRequest.h"
#include "../Report.h"


namespace CDMi {

class MediaSessionSystem : public IMediaSessionSystem {
private:

    class MediaSessionSystemProxy : public IMediaKeySession {
    public:

        explicit MediaSessionSystemProxy(MediaSessionSystem& system) 
        : IMediaKeySession()
        , _system(system)
        , _callback(nullptr)
        , _sessionid(g_NAGRASessionIDPrefix) {
            _sessionid += std::to_string(reinterpret_cast<std::uintptr_t>(this));
            _system.RegisterMediaSessionSystemProxy(this);
            TRACE_L1("system proxy created, %s", _sessionid.c_str());
        }

        virtual ~MediaSessionSystemProxy() {
            _system.DeregisterMediaSessionSystemProxy(this);
            _system.Release();
            TRACE_L1("system proxy destroyed, %s", _sessionid.c_str());
        }

        MediaSessionSystemProxy(const MediaSessionSystemProxy&) = delete;
        MediaSessionSystemProxy& operator=(const MediaSessionSystemProxy&) = delete;

        IMediaKeySessionCallback* IMediaKeyCallback() const {
            return _callback;
        }

        void Run(const IMediaKeySessionCallback* f_piMediaKeySessionCallback) override;

        CDMi_RESULT Load() override {
            return _system.Load();
        }

        void Update(
            const uint8_t *f_pbKeyMessageResponse, 
            uint32_t f_cbKeyMessageResponse) override {
            _system.Update(f_pbKeyMessageResponse, f_cbKeyMessageResponse);
            }

        virtual CDMi_RESULT Remove() override {
            return _system.Remove();
        }

        CDMi_RESULT Close(void) override {
            return _system.Close();
        }

        const char *GetSessionId(void) const override {
            return _sessionid.c_str();
        }

        const char *GetKeySystem(void) const override {
            return _sessionid.c_str();
        }

        CDMi_RESULT Decrypt(
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
            bool initWithLast15) override {
                return _system.Decrypt(f_pbSessionKey, f_cbSessionKey, f_pdwSubSampleMapping, f_cdwSubSampleMapping, f_pbIV, f_cbIV, 
                                        f_pbData, f_cbData, f_pcbOpaqueClearContent, f_ppbOpaqueClearContent, keyIdLength, keyId, initWithLast15);
            }

        CDMi_RESULT ReleaseClearContent(
            const uint8_t *f_pbSessionKey,
            uint32_t f_cbSessionKey,
            const uint32_t  f_cbClearContentOpaque,
            uint8_t  *f_pbClearContentOpaque) override {
                return _system.ReleaseClearContent(f_pbSessionKey, f_cbSessionKey, f_cbClearContentOpaque, f_pbClearContentOpaque);
            }

        const std::string& SessionID() const {
            return _sessionid;
        }

    private:
        MediaSessionSystem& _system;
        IMediaKeySessionCallback *_callback;
        std::string _sessionid;
    };


    MediaSessionSystem(const uint8_t *data, const uint32_t length, const std::string& operatorvault, const std::string& licensepath);
    ~MediaSessionSystem();

public:    
    using DataBuffer = std::vector<uint8_t>;

    MediaSessionSystem(const MediaSessionSystem&) = delete;
    MediaSessionSystem& operator=(const MediaSessionSystem&) = delete;

    static IMediaKeySession* CreateMediaSessionSystem(const uint8_t *f_pbInitData, const uint32_t f_cbInitData, const std::string& defaultoperatorvault, const std::string& licensepath);
    static void DestroyMediaSessionSystem(IMediaKeySession* session);

    // IMediaSessionSystem overrides
    void Run(IMediaKeySessionCallback& callback);
    void Update( const uint8_t *response, uint32_t responseLength);
    CDMi_RESULT Load();
    CDMi_RESULT Remove();
    CDMi_RESULT Close();
    const char *GetSessionId() const;
    const char *GetKeySystem() const;
    CDMi_RESULT Decrypt(
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

    // IMediaSessionSystem overrides
    TNvSession OpenDescramblingSession(IMediaSessionConnect* session, const uint32_t TSID, const uint16_t Emi) override;
    void CloseDescramblingSession(TNvSession session, const uint32_t TSID) override;
    void SetPrmContentMetadata(TNvSession descamblingsession, TNvBuffer* data, TNvStreamType streamtype) override;
    void SetPlatformMetadata(TNvSession descamblingsession, const uint32_t TSID, uint8_t *data, size_t size) override;


    const std::string& SessionId() const {
        return _sessionId;
    }

    virtual void Addref() const override;
    virtual uint32_t Release() const override;

    bool HasProxyWithSessionID(const char* sessionid) const {
        bool result = false;
        for( auto proxy : _systemproxies ) {
            if( ( proxy != nullptr ) && ( proxy->SessionID() == sessionid ) ) {
                result = true;
                break;
            }
        }
        return result;
    }

private:
    using FilterStorage = std::vector<uint8_t>;
    using ConnectSessionStorage = std::map<TNvSession, IMediaSessionConnect*>;
    using DeliverySessionsStorage = std::set<TNvSession>;
    using MediaSessionSystemProxyStorage = std::forward_list<MediaSessionSystemProxy*>;

    static bool OnRenewal(TNvSession appSession);
    static bool OnNeedKey(TNvSession appSession, TNvSession descramblingSession, TNvKeyStatus keyStatus,  TNvBuffer* content, TNvStreamType streamtype);
    static bool OnDeliveryCompleted(TNvSession deliverySession);

    void OnNeedKey(const TNvSession descramblingSession, const TNvKeyStatus keyStatus, const TNvBuffer* content, const TNvStreamType streamtype);
    void OnRenewal();
   // void OnDeliverySessionCompleted(const TNvSession deliverySession); //not needed at the moment, cleanup later
    void GetFilters(FilterStorage& filters);
    void GetProvisionChallenge(DataBuffer& buffer);
    void InitializeWhenProvisoned();
    void HandleFilters(IMediaKeySessionCallback* callback);

    void CloseProvisioningSession();

    void RequestReceived(const Request request) {
        _requests = static_cast<Request>(static_cast<requestsSize>(_requests) | static_cast<requestsSize>(request));
    }

    void RequestHandled(const Request request) {
        _requests = static_cast<Request>(static_cast<requestsSize>(_requests) & (~static_cast<requestsSize>(request)));
    }

    bool WasRequestReceived(const Request request) const {
        return static_cast<Request>(static_cast<requestsSize>(_requests) & static_cast<requestsSize>(request)) == request;
    }

   // inline TNvSession OpenKeyNeedSession(); //not needed at the moment
    inline void OpenRenewalSession();

    inline TNvSession OpenDeliverySession();
  //  inline void CloseDeliverySession(const TNvSession session); // not needed at the moment

    void CreateRenewalExchange(DataBuffer& buffer);

    static MediaSessionSystem* MediaSessionSystemFromAsmHandle(const TNvSession appsession) {
        MediaSessionSystem* system( nullptr );
        uint32_t result = nvAsmGetContext(appsession, reinterpret_cast<TNvHandle*>(&system));
        REPORT_ASM(result, "nvAsmGetContext");
        return ( result == NV_ASM_SUCCESS ? system : nullptr );
    }

    void RegisterMediaSessionSystemProxy(MediaSessionSystemProxy* proxy);
    void DeregisterMediaSessionSystemProxy(MediaSessionSystemProxy* proxy);

    static MediaSessionSystem& AddMediaSessionInstance(const uint8_t *f_pbInitData, const uint32_t f_cbInitData, const std::string& defaultoperatorvault, const std::string& licensepath);
    static void RemoveMediaSessionInstance(MediaSessionSystem* session);


    bool AnyCallBackSet() const {
        bool result = false;
        for ( auto proxy : _systemproxies) {
            if( proxy->IMediaKeyCallback() != nullptr ) {
                result = true;
                break;
            }
        }
        return result;
    }

    void PostProvisionJob();
    void PostRenewalJob();

    constexpr static const char* const g_NAGRASessionIDPrefix = { "NSSID:" };

    std::string _sessionId;
    Request _requests;
    TNvSession _applicationSession;
    TNvSession  _inbandSession;
 //   DeliverySessionsStorage _needKeySessions; At the moment is seems that one delivery session is a correct implementation. As we have not been able to fully test yet if this works in all usage patterns we leave the code to have more in place for now
    TNvSession  _renewalSession; // if we do not need the _needKeySessions we should rename this deliverySession
    TNvSession  _provioningSession;
    ConnectSessionStorage _connectsessions;
    std::string _licensepath;
    MediaSessionSystemProxyStorage _systemproxies;
    mutable uint32_t _referenceCount;
    
};

} // namespace CDMi
