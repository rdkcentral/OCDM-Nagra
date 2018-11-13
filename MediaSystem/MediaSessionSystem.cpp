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
#include "../Report.h"
#include "OperatorVault.h"

#include <core/core.h>

#include <memory>
#include <map>

#include <Nagra/nv_dpsc.h>
#include <Nagra/prm_dsm.h>

namespace CDMi {

namespace {
    static MediaSessionSystem* g_instance = nullptr;
}

WPEFramework::Core::CriticalSection g_lock;

using ApplicationSessionLookupMap = std::map<TNvSession, MediaSessionSystem*>;
ApplicationSessionLookupMap g_ApplicationSessionMap;

using DeliverySessionLookupMap = std::map<TNvSession, MediaSessionSystem*>;
DeliverySessionLookupMap g_DeliverySessionMap;

/* static */ IMediaSessionSystem* IMediaSessionSystem::SystemSession() {
    g_lock.Lock();

    IMediaSessionSystem* retval = g_instance;
    if( retval != nullptr ) {
        retval->Addref();
    } 

    g_lock.Unlock();

    return retval;
}


/* static */ IMediaKeySession* MediaSessionSystem::CreateMediaSessionSystem(const uint8_t *f_pbInitData, uint32_t f_cbInitData) {

    // create outside lock to not keep the locm during all the API calls
    MediaSessionSystem* newsesssion = new MediaSessionSystem(f_pbInitData, f_cbInitData);

    g_lock.Lock();

    g_instance = newsesssion;

    g_lock.Unlock();

    return newsesssion;
}

/* static */ void MediaSessionSystem::DestroyMediaSessionSystem(IMediaKeySession* systemsession) {

    g_lock.Lock();

    if( static_cast<IMediaKeySession*>(g_instance) == systemsession ) {
        g_instance = nullptr;
    }

    g_lock.Unlock();

    static_cast<MediaSessionSystem*>(systemsession)->Release();

}

/* static */ bool MediaSessionSystem::OnRenewal(TNvSession appSession) {

    g_lock.Lock();

    ApplicationSessionLookupMap::iterator index (g_ApplicationSessionMap.find(appSession));

    if (index != g_ApplicationSessionMap.end()) {
        index->second->OnRenewal(); 
    }

    g_lock.Unlock();

    return true;
}

void MediaSessionSystem::OnRenewal() {

   if (_callback != nullptr) {
       std::string challenge = CreateRenewalExchange();
       _callback->OnKeyStatusUpdate("RENEWAL", reinterpret_cast<const uint8_t *>(challenge.c_str()), challenge.length());
   }
   else {
       RequestReceived(Request::RENEWAL);
   }

}

/* static */ bool MediaSessionSystem::OnNeedKey(TNvSession appSession, TNvSession descramblingSession, TNvKeyStatus keyStatus,  TNvBuffer* content, TNvStreamType streamtype) {

    g_lock.Lock();

    ApplicationSessionLookupMap::iterator index (g_ApplicationSessionMap.find(appSession));

    if (index != g_ApplicationSessionMap.end()) {
        index->second->OnNeedKey(descramblingSession, keyStatus, content, streamtype); 
    }

    g_lock.Unlock();

    return true;
}

void MediaSessionSystem::OnNeedKey(TNvSession descramblingSession, TNvKeyStatus keyStatus,  TNvBuffer* content, TNvStreamType streamtype) {

    if( descramblingSession == 0 ) {
        if (_callback != nullptr) {
            _callback->OnKeyStatusUpdate("KEYNEEDED", nullptr, 0);
        }
        else {
            RequestReceived(Request::KEYNEEDED);
        }
    }
    else {
        IMediaSessionConnect* connectsession = nullptr;

        uint32_t result = nvDsmGetContext(descramblingSession, reinterpret_cast<TNvHandle*>(&connectsession));
        REPORT_DSM(result, "nvDsmGetContext");

        auto it = _connectsessions.find(connectsession); //note: do not use the pointer retrieved above direclty, by using the one from the list and this code being in the lpock we know the session still exists
        if( it != _connectsessions.end() ) {
            (*it)->OnNeedKey();
        }
    }

}

/* static */ bool MediaSessionSystem::OnDeliveryCompleted(TNvSession deliverySession) {

    g_lock.Lock();

    DeliverySessionLookupMap::iterator index (g_DeliverySessionMap.find(deliverySession));

    if (index != g_DeliverySessionMap.end()) {
        index->second->OnDeliveryCompleted(); 
    }

    g_lock.Unlock();

    return true;
}

void MediaSessionSystem::OnDeliveryCompleted() {

   if (_callback != nullptr) {
       _callback->OnKeyReady();
       CloseDeliverySession();
   }

}

MediaSessionSystem::FilterStorage MediaSessionSystem::GetFilters() {
    FilterStorage filters;
    if( _applicationSession != 0 ) {
        uint8_t numberOfFilters = 0;
        uint32_t result = nvImsmGetFilters(_applicationSession, nullptr, &numberOfFilters);
        REPORT_IMSM(result, "nvImsmGetFilters");
        if( result == NV_IMSM_SUCCESS ) {
            filters.resize(numberOfFilters);
            result = nvImsmGetFilters(_applicationSession, filters.data(), &numberOfFilters); 
            REPORT_IMSM(result, "nvImsmGetFilters");
        }
    }

    return filters;
}


string MediaSessionSystem::GetProvisionChallenge() {
    string challenge;

    TNvBuffer buf = { NULL, 0 }; 
    uint32_t result = nvAsmGetProvisioningParameters(_applicationSession, &buf);
    REPORT_ASM(result, "nvAsmGetProvisioningParameters");

    if( result == NV_ASM_SUCCESS ) {
        std::vector<uint8_t> buffer(buf.size);
        buf.data = static_cast<void*>(buffer.data());
        buf.size = buffer.size(); // just too make sure...

        uint32_t result = nvAsmGetProvisioningParameters(_applicationSession, &buf);
        REPORT_ASM(result, "nvAsmGetProvisioningParameters");

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
                      challenge = reinterpret_cast<const char*>(buf.data);
                      RequestReceived(Request::PROVISION);
                  }
              }
            }
        }
    }
    return challenge;
}

void MediaSessionSystem::CloseDeliverySession() {
  // already in lock
  
  if(_deliverySession != 0) {
    
    DeliverySessionLookupMap::iterator index (g_DeliverySessionMap.find(_deliverySession));

    if (index != g_DeliverySessionMap.end()) {
      g_DeliverySessionMap.erase(index);
    }
    nvLdsClose(_deliverySession);
    _deliverySession = 0;
  }
}

void MediaSessionSystem::CloseProvisioningSession() {
      if(_provioningSession != 0) {
          nvDpscClose(_provioningSession);
          _provioningSession = 0;
      }
}

std::string MediaSessionSystem::CreateRenewalExchange() {
  // already in lock

    if (_deliverySession == 0) {       
        uint32_t result = nvLdsOpen(&_deliverySession, _applicationSession);
        REPORT_LDS(result,"nvLdsOpen");
        nvLdsSetOnCompleteListener(_deliverySession, OnDeliveryCompleted);
    }        

    std::string exportMessage;

    TNvBuffer buf = { NULL, 0 };

    uint32_t result = nvLdsExportMessage(_deliverySession, &buf);
    REPORT_LDS(result, "nvLdsExportMessage");

    if( result == NV_LDS_SUCCESS ) {

        std::vector<uint8_t> buffer(buf.size);
        buf.data = static_cast<void*>(buffer.data());
        buf.size = buffer.size(); // just too make sure...
        result = nvLdsExportMessage(_deliverySession, &buf);
        REPORT_LDS(result, "nvLdsExportMessage");

        if( result == NV_LDS_SUCCESS ) {
            exportMessage = (const char*)buf.data; 
        }
    }

    return(exportMessage);
}

MediaSessionSystem::MediaSessionSystem(const uint8_t *data, uint32_t length)
    : _sessionId(g_NAGRASessionIDPrefix)
    , _callback(nullptr)
    , _applicationSession(0)
    , _inbandSession(0) 
    , _deliverySession(0)
    , _provioningSession(0)
    , _connectsessions()
    , _casID(0)
    , _referenceCount(1) {

   if( length >= 4 ) {
        WPEFramework::Core::FrameType<0>::Reader reader(WPEFramework::Core::FrameType<0>(const_cast<uint8_t *>(data), length), 0);

        _casID = reader.Number<uint32_t>();
   }

    OperatorVault vault("test.txt");
    string vaultcontent = vault.LoadOperatorVault();

    TNvBuffer tmp = { const_cast<char*>(vaultcontent.c_str()), vaultcontent.length() + 1 };
    uint32_t result = nvAsmOpen(&_applicationSession, &tmp);

    _sessionId += std::to_string(_applicationSession);

    if( result == NV_ASM_ERROR_NEED_PROVISIONING ) {
        RequestReceived(Request::PROVISION);
    }
    REPORT_ASM(result, "nvAsmOpen");

    result = nvAsmSetContext(_applicationSession, static_cast<TNvHandle>(static_cast<IMediaSessionSystem*>(this)));
    REPORT_ASM(result, "nvAsmSetContext");

    g_lock.Lock();
    g_ApplicationSessionMap[_applicationSession] = this;
    g_lock.Unlock();
    result = nvAsmSetOnRenewalListener(_applicationSession, OnRenewal);
    REPORT_ASM(result, "nvAsmSetOnRenewalListener");
    result = nvAsmSetOnNeedKeyListener(_applicationSession, OnNeedKey);
    REPORT_ASM(result, "nvAsmSetOnNeedKeyListener");
    result = nvImsmOpen(&_inbandSession, _applicationSession);
    REPORT_IMSM(result, "nvImsmOpen");
}

MediaSessionSystem::~MediaSessionSystem() {

  nvImsmClose(_inbandSession);

  CloseProvisioningSession();

  g_lock.Lock();

  CloseDeliverySession();

  ApplicationSessionLookupMap::iterator index (g_ApplicationSessionMap.find(_applicationSession));

  if (index != g_ApplicationSessionMap.end()) {
    g_ApplicationSessionMap.erase(index);
    nvAsmClose(_applicationSession);
  }

  g_lock.Unlock();
}

const char *MediaSessionSystem::GetSessionId() const {
  return _sessionId.c_str();
}

const char *MediaSessionSystem::GetKeySystem(void) const {
  return _sessionId.c_str(); // FIXME : replace with keysystem and test.
}

void MediaSessionSystem::Run(const IMediaKeySessionCallback* callback) {

  ASSERT ((callback == nullptr) ^ (_callback == nullptr));

  g_lock.Lock();

  _callback = const_cast<IMediaKeySessionCallback*>(callback);  

  if (_callback != nullptr) {
    if (static_cast<requestsSize>(_requests) != 0) {

      if (WasRequestReceived(Request::PROVISION)) {
        string challenge = GetProvisionChallenge();
        _callback->OnKeyMessage(reinterpret_cast<const uint8_t*>(challenge.c_str()), challenge.length(), const_cast<char*>("PROVISION"));
        RequestHandled(Request::PROVISION);
      }

      //mhmmm, will this work before provisioning is complete....
      FilterStorage filters = GetFilters();
      if( filters.size() > 0 ) {
        _callback->OnKeyMessage(reinterpret_cast<const uint8_t*>(filters.data()), filters.size()*sizeof(TNvFilter), const_cast<char*>("FILTERS"));
      }

      if (WasRequestReceived(Request::KEYNEEDED)) {
        _callback->OnKeyMessage(nullptr, 0, const_cast<char*>("KEYNEEDED"));
        RequestHandled(Request::KEYNEEDED);

      }
      if (WasRequestReceived(Request::RENEWAL)) {
        std::string challenge = CreateRenewalExchange();
        _callback->OnKeyMessage(reinterpret_cast<const uint8_t*>(challenge.c_str()), challenge.length(), const_cast<char*>("RENEWAL"));
        RequestHandled(Request::RENEWAL);
      }
    }
  }

  g_lock.Unlock();
}

void MediaSessionSystem::Update(const uint8_t *data, uint32_t  length) {
  WPEFramework::Core::FrameType<0>::Reader reader(WPEFramework::Core::FrameType<0>(const_cast<uint8_t *>(data), length), 0);

  if( reader.HasData() == true ) {

    Request value = static_cast<Request>(reader.Number<requestsSize>());

    switch (value) {
      case Request::KEYREADY:
      {
        break;
      }
      case Request::KEYNEEDED:
      {
        break;  
      }
      case Request::RENEWAL:
      {
          assert( reader.HasData() == true );
          string response = reader.Text();
          TNvBuffer buf = { const_cast<char*>(response.c_str()), response.length() + 1 }; 
          uint32_t result = nvLdsImportMessage(_deliverySession, &buf); 
          REPORT_LDS(result, "nvLdsImportMessage");
          break;
      }
      case Request::EMMDELIVERY:
      {
          assert( reader.HasData() == true );
          TNvBuffer buf = { nullptr, 0 }; 
          const uint8_t* pbuffer;
          buf.size = reader.LockBuffer<uint16_t>(pbuffer);
          buf.data = const_cast<uint8_t*>(pbuffer);
          uint32_t result = nvImsmDecryptEMM(_inbandSession, &buf); 
          REPORT_IMSM(result, "nvImsmDecryptEMM");
          reader.UnlockBuffer(buf.size);
          break;
      }
      case Request::PROVISION:
      {
            assert( reader.HasData() == true );
            string response = reader.Text();
            TNvBuffer buf = { const_cast<char*>(response.c_str()), response.length() + 1 }; 
            uint32_t result = nvDpscImportMessage(_provioningSession, &buf);
            REPORT_DPSC(result, "nvDpscImportMessage");
            CloseProvisioningSession();
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

void MediaSessionSystem::RegisterConnectSession(IMediaSessionConnect* session) {

    g_lock.Lock(); // note: use same lock as registered callbacks!

    _connectsessions.insert(session);

    g_lock.Unlock();
}

void MediaSessionSystem::UnregisterConnectSession(IMediaSessionConnect* session) {

    g_lock.Lock(); // note: use same lock as registered callbacks!

    auto it = _connectsessions.find(session);
    ASSERT( it != _connectsessions.end() );
    _connectsessions.erase(it);

    g_lock.Unlock();

}

 TNvSession MediaSessionSystem::ApplicationSession() const {
     return _applicationSession;
 }

void MediaSessionSystem::Addref() const {
     WPEFramework::Core::InterlockedIncrement(_referenceCount);
}

uint32_t MediaSessionSystem::Release() const {
    if (WPEFramework::Core::InterlockedDecrement(_referenceCount) == 0) {
        delete this;

        return (WPEFramework::Core::ERROR_DESTRUCTION_SUCCEEDED);
    }
    return (WPEFramework::Core::ERROR_NONE);
}


}  // namespace CDMi
