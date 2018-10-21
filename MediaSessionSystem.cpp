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
#include "CriticalSection.h" 

namespace CDMi {

CriticalSection g_lock;
std::map<void*, MediaSessionSystem*> g_ApplicationSessionMap;

/* static */ void MediaSessionSystem::OnRenewal(void* appSession) {
    std::map<void*, MediaSessionSystem*>::iterator index (g_ApplicationSessionMap.find(appSession));

    if (index != g_ApplicationSessionMap.end()) {
        index->second->OnRenewal(); 
    }
}

/* static */ void MediaSessionSystem::OnNeedKey(void* appSession, void* dsmSession, void* content) {
    std::map<void*, MediaSessionSystem*>::iterator index (g_ApplicationSessionMap.find(appSession));

    if (index != g_ApplicationSessionMap.end()) {
        index->second->OnNeedKey(dsmSession, content); 
    }
}

/* static */ void MediaSessionSystem::OnDeliveryCompleted(void* deliverySession) {
    std::map<void*, MediaSessionSystem*>::iterator index (g_ApplicationSessionMap.find(deliverySession));

    if (index != g_ApplicationSessionMap.end()) {
        index->second->OnDeliveryCompleted(); 
    }
}

void MediaSessionSystem::OnRenewal() {

   g_lock.Lock();

   if (_callback != nullptr) {
       string challenge = CreateRenewalExchange();
       _callback->OnKeyStatusUpdate(_T("RENEWAL", challenge.c_str(), challenge.length());
   }
   else {
       _request |= RENEWAL;
   }

   g_lock.Unlock();
}

void MediaSessionSystem::OnNeedKey(void* dsmSession, void* content) {

   g_lock.Lock();

   if (_callback != nullptr) {
       _callback->OnKeyStatusUpdate(_T("KEYNEEDED", nullptr, 0);
   }
   else {
       _request |= KEYNEEDED;
   }

   g_lock.Unlock();
}

void MediaSessionSystem::OnDelivery() {

   g_lock.Lock();

   if (_callback != nullptr) {
       _callback->OnKeyReady();
       nvLdsClose(_deliverySession);
       _deliverySession = nullptr;
   }
   else {
       _request |= RENEWALREADY;
   }

   g_lock.Unlock();
}

std::string MediaSessionSystem::CreateRenewalExchange() {

    std::string challenge;

    if (_deliverySession == nullptr) {
 
        g_lock.Lock();
        _deliverySession = nvLdsOpen();
        g_ApplicationSessionMap[_deliverySession] = this;
        nvLdsSetOnCompleteListener(_deliverySession, onDeliveryCompleted);
        g_lock.Unlock();
    }        

    return(nvLdsExportMessage());
}

MediaSessionSystem::MediaSessionSystem(const uint8_t *f_pbInitData, uint32_t f_cbInitData)
    : _sessionId()
    , _callback(nullptr)
    , _applicationSession(nullptr)
    , _inbandSession(nullptr)
    , _deliverySession(nullptr)
    , _renewelChallenge()
    , _keyNeededChallenge()
    , _filters() {

    _applicationSession = nvAsmOpen();
    g_lock.Lock();
    g_ApplicationSessionMap[_applicationSession] = this;
    g_lock.Unlock();
    nvAsmSetOnRenewalListener(_applicationSession, onRenewal);
    nvAsmSetOnNeedKeyListener(_applicationSession, onNeedKey);

    _inbandSession = nvImsmOpen();
    _filters = nvImsmGetFilters();
}

MediaSessionSystem::~MediaSessionSystem() {
  g_lock.Lock();
  std::map<void*, MediaSessionSystem*>::iterator index (g_ApplicationSessionMap.find(_applicationSession));

  if (index != g_ApplicationSessionMap.end()) {
    g_ApplicationSessionMap.erase(index);
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

  assert ((callback == nullptr) ^ (_callback == nullptr));

  g_lock.Lock();

  _callback = callback;   

  if (_callback != nullptr) {
    if (_request != 0) {

      _callback->OnKeyStatusUpdate(_T("FILTERS", _filters.c_str(), _filters.length());

      if ((_request & KEYNEEDED) == KEYNEEDED) {
        _callback->OnKeyStatusUpdate(_T("KEYNEEDED", nullptr, 0);
        _request &= (~KEYNEEDED);
      }
      if ((_request & RENEWAL) == RENEWAL) {
        string challenge = CreateRenewalExchange();
        _callback->OnKeyStatusUpdate(_T("RENEWAL", challenge.c_str(), challenge.length());
        _request &= (~RENEWAL);
      }
      if ((_request & RENEWALREADY) == RENEWALREADY) {
        _callback->OnKeyReady();
        _request &= (~KEYREADY);
        nvLdsClose(_deliverySession);
        _deliverySession = nullptr;
      }
    }
  }

  g_lock.Unlock();
}

void MediaSessionSystem::Update(const uint8_t *data, uint32_t  length) {
   if (length >= 2) {
      request value (static_cast<request>(data[0] | (data[1] << 8)));
      switch (value) {
      case KEYREADY:
      {
        break;
      }
      case KEYNEEDED:
      {
        break;
      }
      case RENEWAL:
      {
        nvLdsImportMessage(&(data[2]), length - 2);
        break;
      }
      case EMMDELIVERY:
      {
        nvImsmDecryptEMM(&(data[2]), length - 2);
        break;
      }
      default: /* WTF */
               break;

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
  assert(false);

  return CDMi_S_FALSE;
}

CDMi_RESULT MediaSessionSystem::ReleaseClearContent(const uint8_t*, uint32_t, const uint32_t, uint8_t*) {

  // System sessions should *NOT* be used for decrypting !!!!
  assert(false);

  return CDMi_S_FALSE;
}

}  // namespace CDMi
