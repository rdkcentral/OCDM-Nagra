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

#include "MediaSessionSystem.h"
#include "OperatorVault.h"

#include <core/core.h>
#include "../Report.h"

#include <memory>
#include <map>

#include <nagra/nv_dpsc.h>
#include <nagra/prm_dsm.h>



namespace {

    static CDMi::MediaSessionSystem* g_instance = nullptr;


    WPEFramework::Core::CriticalSection g_lock;

    using ApplicationSessionLookupMap = std::map<TNvSession, CDMi::MediaSessionSystem*>;
    ApplicationSessionLookupMap g_ApplicationSessionMap;

    using DeliverySessionLookupMap = std::map<TNvSession, CDMi::MediaSessionSystem*>;
    DeliverySessionLookupMap g_DeliverySessionMap;

}

#ifdef __cplusplus
extern "C" {
#endif


CDMi::IMediaSessionSystem* GetMediaSessionSystemInterface() {

    g_lock.Lock();

    CDMi::IMediaSessionSystem* retval = g_instance;
    if( retval != nullptr ) {
        retval->Addref();
    } 

    g_lock.Unlock();

    return retval;
}

#ifdef __cplusplus
}
#endif

namespace CDMi {

/* static */ IMediaKeySession* MediaSessionSystem::CreateMediaSessionSystem(const uint8_t *f_pbInitData, uint32_t f_cbInitData, const std::string& operatorvault) {

    ASSERT(f_pbInitData == nullptr && f_cbInitData == 0); //as this is a singletion we do not expect any parameters as we do not take them into account

    g_lock.Lock();

    if( g_instance == nullptr ) {
        g_instance = new MediaSessionSystem(nullptr, 0, operatorvault);
    }
    else{
        g_instance->Addref();
    }

    g_lock.Unlock();

    return g_instance;
}

/* static */ void MediaSessionSystem::DestroyMediaSessionSystem(IMediaKeySession* systemsession) {

    static_cast<MediaSessionSystem*>(systemsession)->Release();
}

/* static */ bool MediaSessionSystem::OnRenewal(TNvSession appSession) {
    REPORT("static NagraSystem::OnRenewal triggered");

    g_lock.Lock();

    ApplicationSessionLookupMap::iterator index (g_ApplicationSessionMap.find(appSession));

    if (index != g_ApplicationSessionMap.end()) {
        index->second->OnRenewal(); 
    }

    g_lock.Unlock();

    return true;
}

void MediaSessionSystem::OnRenewal() {
    REPORT("NagraSystem::OnRenewal triggered");

   if (_callback != nullptr) {
       DataBuffer buffer;
       CreateRenewalExchange(buffer);
        _callback->OnKeyMessage(buffer.data(), buffer.size(), const_cast<char*>("RENEWAL"));
   }
   else {
       RequestReceived(Request::RENEWAL);
   }

}

/* static */ bool MediaSessionSystem::OnNeedKey(TNvSession appSession, TNvSession descramblingSession, TNvKeyStatus keyStatus,  TNvBuffer* content, TNvStreamType streamtype) {

    REPORT("static NagraSystem::OnNeedkey triggered");

    g_lock.Lock();

    ApplicationSessionLookupMap::iterator index (g_ApplicationSessionMap.find(appSession));

    if (index != g_ApplicationSessionMap.end()) {
        index->second->OnNeedKey(descramblingSession, keyStatus, content, streamtype); 
    }

    g_lock.Unlock();

    return true;
}

void MediaSessionSystem::OnNeedKey(TNvSession descramblingSession, TNvKeyStatus keyStatus,  TNvBuffer* content, TNvStreamType streamtype) {

    REPORT_EXT("NagraSystem::OnNeedkey triggered for descrambling session %u", descramblingSession);

    // ignore decsrambling session
 //   if( descramblingSession == 0 ) {

  //      REPORT("NagraSystem::OnNeedkey triggered for system");

        if(content != nullptr) {
            DumpData("NagraSystem::OnNeedKey", (const uint8_t*)(content->data), content->size);
            }


            if (_callback != nullptr) {

      //          TNvSession deliverysession = OpenKeyNeedSession();
      
               TNvSession deliverysession = _renewalSession;

                uint32_t result = nvLdsUsePrmContentMetadata(deliverysession, content, streamtype);
                REPORT_LDS(result,"nvLdsUsePrmContentMetadata");
                REPORT("NagraSystem::OnNeedkey ContextMetadata set");

                TNvBuffer buf = { NULL, 0 };

                result = nvLdsExportMessage(deliverysession, &buf);
                REPORT_LDS(result, "nvLdsExportMessage");

                if( result == NV_LDS_SUCCESS ) {

                    std::vector<uint8_t> buffer(buf.size);
                    buf.data = static_cast<void*>(buffer.data());
                    buf.size = buffer.size(); // just too make sure...
                    result = nvLdsExportMessage(deliverysession, &buf);
                    REPORT_LDS(result, "nvLdsExportMessage");

                    if( result == NV_LDS_SUCCESS ) {
                        DumpData("NagraSystem::OnNeedKey", buffer.data(), buffer.size());

                        _callback->OnKeyMessage(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size(), const_cast<char*>("KEYNEEDED"));
                    }
                }

            }
            else {
                ASSERT(false);
                REPORT("NagraSystem::OnNeedkey triggerd while no callback set");
            }
//        }
  //  else {
/*
        REPORT("NagraSystem::OnNeedkey triggered for connect session");

        if(content != nullptr) {
            DumpData("NagraSystem::OnNeedKey::Descrambling", (const uint8_t*)(content->data), content->size);
        }

        IMediaSessionConnect* connectsession = nullptr;

        auto it = _connectsessions.find(descramblingSession); 
        if( it != _connectsessions.end() ) {
            it->second->OnNeedKey();
        }
    }
*/
}

/* static */ bool MediaSessionSystem::OnDeliveryCompleted(TNvSession deliverySession) {

    g_lock.Lock();

    REPORT("MediaSessionSystem::OnDeliveryCompleted");

    TNvLdsStatus status;
    uint32_t result = nvLdsGetResults(deliverySession, &status);
    REPORT_LDS(result,"nvLdsGetResults");

    REPORT_EXT("OnDeliveryCompleted result %i", status.status);

//    DeliverySessionLookupMap::iterator index (g_DeliverySessionMap.find(deliverySession));

//    if ( index != g_DeliverySessionMap.end() ) {
//        index->second->OnDeliveryCompleted(deliverySession); 
//    }

    g_lock.Unlock();

    return true;
}

void MediaSessionSystem::OnDeliverySessionCompleted(TNvSession deliverySession) {

   if (_callback != nullptr) {
       _callback->OnKeyReady();
   }

//   CloseDeliverySession(deliverySession);
}

void MediaSessionSystem::GetFilters(FilterStorage& filters) {
    filters.clear();
    if( _applicationSession != 0 ) {
        uint8_t numberOfFilters = 0;
        uint32_t result = nvImsmGetFilters(_applicationSession, nullptr, &numberOfFilters);
        REPORT_IMSM(result, "nvImsmGetFilters");
        if( result == NV_IMSM_SUCCESS ) {
            filters.resize(numberOfFilters);
            result = nvImsmGetFilters(_applicationSession, filters.data(), &numberOfFilters); 
            REPORT_IMSM(result, "nvImsmGetFilters");

//            DumpData("NagraSystem::GetFilters", (const uint8_t*)(filters.data()), filters.size());

            if( result != NV_IMSM_SUCCESS ) {
                filters.clear();
            }

        }
    }
}


void MediaSessionSystem::GetProvisionChallenge(DataBuffer& buffer) {
    buffer.clear();

    TNvBuffer buf = { NULL, 0 }; 
    uint32_t result = nvAsmGetProvisioningParameters(_applicationSession, &buf);
    REPORT_ASM(result, "nvAsmGetProvisioningParameters");

    if( result == NV_ASM_SUCCESS ) {
        buffer.resize(buf.size);
        buf.data = static_cast<void*>(buffer.data());
        buf.size = buffer.size(); // just too make sure...

        uint32_t result = nvAsmGetProvisioningParameters(_applicationSession, &buf);
        REPORT_ASM(result, "nvAsmGetProvisioningParameters");

        DumpData("System::ProvisioningParameters", buffer.data(), buffer.size());


        if( result == NV_ASM_SUCCESS ) {
            result = nvDpscOpen(&_provioningSession);
            REPORT_DPSC(result, "nvDpscOpen");

            if( result == NV_DPSC_SUCCESS ) {
              result = nvDpscSetClientData(_provioningSession, &buf);
              REPORT_DPSC(result, "nvDpscSetClientData");

              buf.data = nullptr;
              buf.size = 0;

              result = nvDpscExportMessage(_provioningSession, &buf);
              REPORT_DPSC(result, "nvDpscExportMessage");

              if( result == NV_DPSC_SUCCESS ) {
                  buffer.resize(buf.size);
                  buf.data = static_cast<void*>(buffer.data());
                  ASSERT(buffer.size() == buf.size); //just to make sure
                  result = nvDpscExportMessage(_provioningSession, &buf);
                  REPORT_DPSC(result, "nvDpscExportMessage");
                  if( result == NV_DPSC_SUCCESS ) {
                      DumpData("NagraSystem::ProvisioningExportMessage", buffer.data(), buffer.size());
                  }
                  else {
                      buffer.clear();
                  }
              }
            }
        }
    }
}

TNvSession MediaSessionSystem::OpenKeyNeedSession() {
    TNvSession session = OpenDeliverySession();
    if( session != 0 ) {
        _needKeySessions.insert(session);
    }
    return session;
}

void MediaSessionSystem::OpenRenewalSession() {
    ASSERT( _renewalSession == 0 );
    _renewalSession = OpenDeliverySession();
}

TNvSession MediaSessionSystem::OpenDeliverySession() {   
    // should be in the lock

    TNvSession session(0);
    uint32_t result = nvLdsOpen(&session, _applicationSession);
    REPORT_LDS(result,"nvLdsOpen");
    if( result == NV_LDS_SUCCESS ) {
        g_DeliverySessionMap[session] = this;
        nvLdsSetOnCompleteListener(session, OnDeliveryCompleted);
    }
    return session;
}

void MediaSessionSystem::CloseDeliverySession(TNvSession session) {
    // already in lock
    
    DeliverySessionLookupMap::iterator index (g_DeliverySessionMap.find(session));

    if (index != g_DeliverySessionMap.end()) {
        g_DeliverySessionMap.erase(index);
    }

    if( _renewalSession == session ) {
        _renewalSession = 0;
    }
    else {
        auto it = _needKeySessions.find(session);
        ASSERT( it != _needKeySessions.end() );
        if( it != _needKeySessions.end() ) {
            _needKeySessions.erase(it);
        }
    }

    nvLdsClose(session);
}

void MediaSessionSystem::CloseProvisioningSession() {
      if(_provioningSession != 0) {
          nvDpscClose(_provioningSession);
          _provioningSession = 0;
      }
}

void MediaSessionSystem::CreateRenewalExchange(DataBuffer& buffer) {
  // already in lock

    buffer.clear();

  //  TNvSession deliverysession = OpenRenewalSession();

    std::string exportMessage;

    TNvBuffer buf = { NULL, 0 };

    uint32_t result = nvLdsExportMessage(_renewalSession, &buf);
    REPORT_LDS(result, "nvLdsExportMessage");

    if( result == NV_LDS_SUCCESS ) {

        buffer.resize(buf.size);
        buf.data = static_cast<void*>(buffer.data());
        buf.size = buffer.size(); // just too make sure...
        result = nvLdsExportMessage(_renewalSession, &buf);
        REPORT_LDS(result, "nvLdsExportMessage");

        if( result == NV_LDS_SUCCESS ) {
            DumpData("NagraSystem::RenewalExportMessage", buffer.data(), buffer.size());
        }
        else {
            buffer.clear();
        }
    }
}

void MediaSessionSystem::InitializeWhenProvisoned() {
    REPORT("enter MediaSessionSystem::MediaSessionSystem");

    //protect as much as possible to an unexpected update(provioning) call ;)

    if( _renewalSession == 0 ) {
        OpenRenewalSession(); //do before callbacks are set, so no need to do this insside the lock
    }
    uint32_t result = nvAsmSetOnRenewalListener(_applicationSession, OnRenewal);
    REPORT_ASM(result, "nvAsmSetOnRenewalListener");
    result = nvAsmSetOnNeedKeyListener(_applicationSession, OnNeedKey);
    REPORT_ASM(result, "nvAsmSetOnNeedKeyListener");
    if( _inbandSession == 0 ) {
        result = nvImsmOpen(&_inbandSession, _applicationSession);
        REPORT_IMSM(result, "nvImsmOpen");
    }

    REPORT("enter MediaSessionSystem::MediaSessionSystem");
}

void MediaSessionSystem::HandleFilters() {
    REPORT("MediaSessionSystem::Run checking filters");
    FilterStorage filters;
    GetFilters(filters);
    REPORT_EXT("MediaSessionSystem::Run %i filter found", filters.size());
    if( filters.size() > 0 ) {
        REPORT("MediaSessionSystem::Run firing filters ");
        _callback->OnKeyMessage(reinterpret_cast<const uint8_t*>(filters.data()), filters.size()*sizeof(TNvFilter), const_cast<char*>("FILTERS"));
    }
    
}

MediaSessionSystem::MediaSessionSystem(const uint8_t *data, uint32_t length, const std::string& operatorvault)
    : _sessionId(g_NAGRASessionIDPrefix)
    , _callback(nullptr)
    , _requests(Request::NONE)
    , _applicationSession(0)
    , _inbandSession(0) 
    , _needKeySessions()
    , _renewalSession(0)
    , _provioningSession(0)
    , _connectsessions()
    , _referenceCount(1) {

 //   REPORT_EXT("operator vault location %s", operatorvault.c_str());

    REPORT("enter MediaSessionSystem::MediaSessionSystem");

    ::ThreadId tid =  WPEFramework::Core::Thread::ThreadId();
    REPORT_EXT("MediaSessionSystem threadid = %u", tid);


    REPORT_EXT("going to test data access %u", length);

    if(length > 0) {
        uint8_t v = data[length-1];
    }

    REPORT("date access tested");

    OperatorVault vault("/etc/nagra/op_vault.json");
    string vaultcontent = vault.LoadOperatorVault();

    TNvBuffer tmp = { const_cast<char*>(vaultcontent.c_str()), vaultcontent.length() + 1 };
    uint32_t result = nvAsmOpen(&_applicationSession, &tmp);

    REPORT_EXT("SystenmSession appsession created; %u", _applicationSession);


    _sessionId += std::to_string(_applicationSession);

    if( result == NV_ASM_ERROR_NEED_PROVISIONING ) {
            REPORT("provisioning needed!!!");

        RequestReceived(Request::PROVISION);
    }
    REPORT_ASM(result, "nvAsmOpen");

    g_lock.Lock();
    g_ApplicationSessionMap[_applicationSession] = this;
    g_lock.Unlock();
 
    if( result == NV_ASM_SUCCESS ) {
        // This should be moved to config, just like the operator vault path.
        string asm_licenses_dir("/mnt/flash/nv_tstore/asm_licenses/");
        result = nvAsmUseStorage(_applicationSession,
                                 const_cast<char *>(asm_licenses_dir.c_str()));
        REPORT_ASM(result, "nvAsmUseStorage");

        InitializeWhenProvisoned();
    }

    REPORT("leave MediaSessionSystem::MediaSessionSystem");
}

MediaSessionSystem::~MediaSessionSystem() {

    REPORT("enter MediaSessionSystem::~MediaSessionSystem");

    // note will correctly handle if InitializeWhenProvisoned() was never called

    if( _inbandSession != 0 ) {
        nvImsmClose(_inbandSession);
    }

    g_lock.Lock();

    CloseProvisioningSession();

    CloseDeliverySession(_renewalSession);

    ApplicationSessionLookupMap::iterator index (g_ApplicationSessionMap.find(_applicationSession));

 //   for(TNvSession session : _needKeySessions) {
 //       CloseDeliverySession(session);
 //   }  

    if (index != g_ApplicationSessionMap.end()) {
        g_ApplicationSessionMap.erase(index);
        nvAsmClose(_applicationSession);
     }

     g_instance == nullptr; // we are closing the singleton

    g_lock.Unlock();
    
    ASSERT( _connectsessions.size() == 0 ); //that would be strange if this would not be the case, problem with the refcounting...

    REPORT("enter MediaSessionSystem::~MediaSessionSystem");

}

const char *MediaSessionSystem::GetSessionId() const {
  return _sessionId.c_str();
}

const char *MediaSessionSystem::GetKeySystem(void) const {
  return _sessionId.c_str(); // FIXME : replace with keysystem and test.
}

void MediaSessionSystem::Run(const IMediaKeySessionCallback* callback) {

  REPORT("MediaSessionSystem::Run");

  ASSERT ((callback == nullptr) ^ (_callback == nullptr));

  g_lock.Lock();

  _callback = const_cast<IMediaKeySessionCallback*>(callback);  

  if (_callback != nullptr) {
        REPORT("MediaSessionSystem::Run callback set");

      if (WasRequestReceived(Request::PROVISION)) {
        REPORT("MediaSessionSystem::Run firing provisoning ");
        DataBuffer buffer;
        GetProvisionChallenge(buffer);
        _callback->OnKeyMessage(buffer.data(), buffer.size(), const_cast<char*>("PROVISION"));
        RequestHandled(Request::PROVISION);
      } 
      else {
          // no provisioning needed, we can already fire the filters now...
          HandleFilters();
      }

        if (WasRequestReceived(Request::KEYNEEDED)) {
            REPORT("MediaSessionSystem::Run firing keyneeded ");
            _callback->OnKeyMessage(nullptr, 0, const_cast<char*>("KEYNEEDED"));
            RequestHandled(Request::KEYNEEDED);

        }
        if (WasRequestReceived(Request::RENEWAL)) {
            REPORT("MediaSessionSystem::Run firing renewal ");
            DataBuffer buffer;
            CreateRenewalExchange(buffer);
            _callback->OnKeyMessage(buffer.data(), buffer.size(), const_cast<char*>("RENEWAL"));
            RequestHandled(Request::RENEWAL);
      }

  }

  g_lock.Unlock();
}

void MediaSessionSystem::Update(const uint8_t *data, uint32_t  length) {

        REPORT("enter MediaSessionSystem::Update");

    REPORT_EXT("going to test data access %u", length);

    if(length > 0) {
        uint8_t v = data[length-1];
    }

    REPORT("date access tested");


    WPEFramework::Core::FrameType<0> frame(const_cast<uint8_t *>(data), length, length);
    WPEFramework::Core::FrameType<0>::Reader reader(frame, 0);

    REPORT("NagraSytem update triggered");

 
   if( reader.HasData() == true ) {

        Request value = static_cast<Request>(reader.Number<requestsSize>());

        REPORT_EXT("NagraSytem update triggered with %d", value);

        switch (value) {
        case Request::KEYREADY:
        {
            break;
        }
        case Request::KEYNEEDED: //fallthrough on purpose
        case Request::RENEWAL: 
        { 
            assert( reader.HasData() == true );
            string response = reader.Text();
            TNvBuffer buf = { const_cast<char*>(response.c_str()), response.length() + 1 }; 
            DumpData("NagraSystem::RenewalResponse|Keyneeded", (const uint8_t*)buf.data, buf.size);
            uint32_t result = nvLdsImportMessage(_renewalSession, &buf); 
            REPORT_LDS(result, "nvLdsImportMessage");
            break;
        }
        case Request::EMMDELIVERY:
        {
            REPORT("NagraSytem importing EMM response");
            assert( reader.HasData() == true );
            TNvBuffer buf = { nullptr, 0 }; 
            const uint8_t* pbuffer;
            buf.size = reader.LockBuffer<uint16_t>(pbuffer);
            buf.data = const_cast<uint8_t*>(pbuffer);
            DumpData("NagraSystem::EMMResponse", (const uint8_t*)buf.data, buf.size);
            uint32_t result = nvImsmDecryptEMM(_inbandSession, &buf); 
            REPORT_IMSM(result, "nvImsmDecryptEMM");
            reader.UnlockBuffer(buf.size);
            break;
        }
        case Request::PROVISION:
        {
            REPORT("NagraSytem importing provsioning response");
            assert( reader.HasData() == true );
            string response = reader.Text();
            TNvBuffer buf = { const_cast<char*>(response.c_str()), response.length() + 1 }; 
            DumpData("NagraSystem::ProvisionResponse", (const uint8_t*)buf.data, buf.size);
            uint32_t result = nvDpscImportMessage(_provioningSession, &buf);
            REPORT_DPSC(result, "nvDpscImportMessage");
            CloseProvisioningSession();
            InitializeWhenProvisoned();
            // handle the filters as that was postponed untill provisioning was complete...
            HandleFilters();
            break;
        }
        default: /* WTF */
            break;
        }
        if( reader.HasData() ) {
        REPORT("MediaSessionSystem::Update: more data than expected");
        }
    }
    else {
       REPORT("MediaSessionSystem::Update: expected more data");
    }
        REPORT("leave MediaSessionSystem::Update");
}

CDMi_RESULT MediaSessionSystem::Load() {
  return CDMi_S_FALSE;
}

CDMi_RESULT MediaSessionSystem::Remove() {
  return CDMi_S_FALSE;
}

CDMi_RESULT MediaSessionSystem::Close() {
  return CDMi_S_FALSE;
}

CDMi_RESULT MediaSessionSystem::Decrypt(
    const uint8_t *f_pbSessionKey,
    uint32_t f_cbSessionKey,
    const uint32_t *f_pdwSubSampleMapping,
    uint32_t f_cdwSubSampleMapping,
    const uint8_t *f_pbIV,
    uint32_t f_cbIV,
    const uint8_t *payloadData,
    uint32_t payloadDataSize,
    uint32_t *f_pcbOpaqueClearContent,
    uint8_t **f_ppbOpaqueClearContent,
    const uint8_t /* keyIdLength */,
    const uint8_t* /* keyId */)
{
  // System sessions should *NOT* be used for decrypting !!!!
  ASSERT(false);

  return CDMi_S_FALSE;
}

CDMi_RESULT MediaSessionSystem::ReleaseClearContent(const uint8_t*, uint32_t, const uint32_t, uint8_t*) {

  // System sessions should *NOT* be used for decrypting !!!!
  ASSERT(false);

  return CDMi_S_FALSE;
}

 TNvSession MediaSessionSystem::OpenDescramblingSession(IMediaSessionConnect* session, const uint32_t TSID, const uint16_t Emi) {

    TNvSession descramblingsession = 0;
    int platStatus;

    g_lock.Lock(); // note: use same lock as registered callbacks!

    platStatus = nagra_cma_platf_dsm_open(TSID);
    REPORT_PRM_EXT(NAGRA_CMA_PLATF_OK, platStatus,
                   "nagra_cma_platf_dsm_open", " tsid=%u", TSID);

    uint32_t result = nvDsmOpen(&descramblingsession, _applicationSession, TSID, Emi);
    REPORT_DSM(result, "nvDsmOpen");

    if( result == NV_DSM_SUCCESS ) {
        _connectsessions[descramblingsession] = session;
    }

    g_lock.Unlock();

    return descramblingsession;
}

void MediaSessionSystem::CloseDescramblingSession(TNvSession session, const uint32_t TSID) {
     REPORT("enter MediaSessionSystem::UnregisterConnectSessionS");

    g_lock.Lock(); // note: use same lock as registered callbacks!

    auto it = _connectsessions.find(session);
    ASSERT( it != _connectsessions.end() );
    if( it != _connectsessions.end() ) {
        int platStatus;
        nvDsmClose(session);

        platStatus = nagra_cma_platf_dsm_close(TSID);
        REPORT_PRM_EXT(NAGRA_CMA_PLATF_OK, platStatus,
                       "nagra_cma_platf_dsm_close", " tsid=%u", TSID);

        _connectsessions.erase(it);
    }

    g_lock.Unlock();
     REPORT("leave MediaSessionSystem::UnregisterConnectSessionS");

}

void MediaSessionSystem::SetPrmContentMetadata(TNvSession descamblingsession, TNvBuffer* data, TNvStreamType streamtype) {
    uint32_t result = nvDsmSetPrmContentMetadata(descamblingsession, data, streamtype);
    REPORT_DSM(result, "nvDsmSetPrmContentMetadata");
}

void MediaSessionSystem::SetPlatformMetadata(TNvSession descamblingsession, const uint32_t TSID, uint8_t *data, size_t size) {
    int result = nagra_cma_platf_dsm_cmd(TSID, data, size);
    REPORT_PRM_EXT(NAGRA_CMA_PLATF_OK, result,
                   "nagra_cma_platf_dsm_cmd", " tsid=%u", TSID);
}

void MediaSessionSystem::Addref() const {
     WPEFramework::Core::InterlockedIncrement(_referenceCount);
}

uint32_t MediaSessionSystem::Release() const {
     REPORT("enter MediaSessionSystem::Release");

    g_lock.Lock(); // need lock here as wel as in CreateMediaSessionSystem(), as the final release can come from external as well as from the connect session

    uint32_t retval = WPEFramework::Core::ERROR_NONE;

     REPORT_EXT("MediaSessionSystem::Release %u", _referenceCount);

    if (WPEFramework::Core::InterlockedDecrement(_referenceCount) == 0) {
        delete this;
     REPORT("leave MediaSessionSystem::Release deleted");

        uint32_t retval = WPEFramework::Core::ERROR_DESTRUCTION_SUCCEEDED;
    }

    g_lock.Unlock();

     REPORT("leave MediaSessionSystem::Release  not deleted");
    return retval;
}


}  // namespace CDMi
