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

#include "MediaSessionConnect.h"

#include "../Report.h"
#include "../ParsePSSHHeader.h"
#include "../MediaRequest.h"

namespace {

class MediaSystemLoader {
    public:
    MediaSystemLoader() 
        : _SessionSystem(nullptr)
        , _syslib("DRMNagraSystem.drm") {

        if (_syslib.IsLoaded() == true) {
            _SessionSystem = reinterpret_cast<CDMi::IMediaSessionSystem*(*)(const char*)>(_syslib.LoadFunction(_T("GetMediaSessionSystemInterface")));
        }
        else {
            REPORT("NagraConnect going to look for NagraSsystem access entry : lib was NOT loaded");
        }
  }

  CDMi::IMediaSessionSystem* SessionSystem(const char* systemsessionid) {
      CDMi::IMediaSessionSystem* retval(nullptr);

      if( _SessionSystem != nullptr ) {
          retval = _SessionSystem(systemsessionid);
      }
      return retval;
  }

  private:
    WPEFramework::Core::Library _syslib;
    CDMi::IMediaSessionSystem* (*_SessionSystem)(const char* systemsessionid);
};

CDMi::IMediaSessionSystem* SessionSystem(const char* systemsessionid) {
  static MediaSystemLoader loader;
  return loader.SessionSystem(systemsessionid);
}

}

namespace CDMi {

MediaSessionConnect::MediaSessionConnect(const uint8_t *data, uint32_t length)
    : _sessionId(g_NAGRASessionIDPrefix)
    , _callback(nullptr)
    , _descramblingSession(0)
    , _TSID(0)
    , _systemsession(nullptr)
    , _lock() {

    REPORT("enter MediaSessionConnect::MediaSessionConnect"); 

    // DumpData("MediaSessionConnect::MediaSessionConnect", data, length);

    uint16_t Emi = 0;

    REPORT("parsing pssh header");

    // parse pssh header
    std::string systemsessionid;
    const uint8_t *privatedata = data;
    int32_t result = FindPSSHHeaderPrivateData(privatedata, length);

    if( result > 0 ) {
        WPEFramework::Core::FrameType<0> frame(const_cast<uint8_t *>(privatedata), result, result);
        WPEFramework::Core::FrameType<0>::Reader reader(frame, 0);

        constexpr uint8_t privatedatapart1size = sizeof(uint32_t) + sizeof(uint16_t);

        if( result >= privatedatapart1size ) {
            REPORT("parsing pssh private data part1");
            _TSID = reader.Number<uint32_t>();
            Emi = reader.Number<uint16_t>();
            if( result > privatedatapart1size ) {
                systemsessionid = std::string(reinterpret_cast<const char*>(&(privatedata[privatedatapart1size])), result-privatedatapart1size);   
            }
        }

    }
    else {
        REPORT_EXT("incorrect pssh header or no private data: %i", result);
    }
     
    _systemsession =  SessionSystem(systemsessionid.empty() ? nullptr : systemsessionid.c_str());
    ASSERT( _systemsession != nullptr );

      if( _systemsession != nullptr ) {

        REPORT_EXT("ConnectSession TSID used; %u", _TSID);
        REPORT_EXT("ConnectSession Emi used; %u", Emi);

        _descramblingSession = _systemsession->OpenDescramblingSession(this, _TSID, Emi);

        _sessionId += std::to_string(_descramblingSession);

          if( _descramblingSession == 0 ) {
              REPORT("Failed to create descrambling sesssion");
          }
          else {
              REPORT_EXT("MediaSessionConnect created descrambling sesssion succesfully %u", _descramblingSession);
          }
    }
    else {
      REPORT("Could not get MediaSessionSystem from ConnectSession. ConnectSession cannot be used without an active SystemSession");
    }

    REPORT("leave MediaSessionConnect::MediaSessionConnect");
}

MediaSessionConnect::~MediaSessionConnect() {

     REPORT("enter MediaSessionConnect::~MediaSessionConnect");

    if( _systemsession != nullptr ) {

        if ( _descramblingSession != 0 ) {
          _systemsession->CloseDescramblingSession(_descramblingSession, _TSID);
        }

        _systemsession->Release();
    }
     REPORT("leave MediaSessionConnect::~MediaSessionConnect");

}

const char *MediaSessionConnect::GetSessionId() const {
  return _sessionId.c_str();
}

const char *MediaSessionConnect::GetKeySystem(void) const {
  return _sessionId.c_str(); // FIXME : replace with keysystem and test.
}

void MediaSessionConnect::Run(const IMediaKeySessionCallback* callback) {

    ASSERT ((callback == nullptr) ^ (_callback == nullptr));

    _lock.Lock();

    _callback = const_cast<IMediaKeySessionCallback*>(callback);  

   _lock.Unlock();
}

void MediaSessionConnect::Update(const uint8_t *data, uint32_t length) {
    REPORT("enter MediaSessionConnect::Update");

    WPEFramework::Core::FrameType<0> frame(const_cast<uint8_t *>(data), length, length);
    WPEFramework::Core::FrameType<0>::Reader reader(frame, 0);

    REPORT("NagraSytem update triggered");

 
   if( reader.HasData() == true ) {

        Request value = static_cast<Request>(reader.Number<requestsSize>());

        REPORT_EXT("NagraSytem update triggered with %d", value);

        switch (value) {
        case Request::ECMDELIVERY:
        {
            REPORT("NagraSytem importing ECM response");
            ASSERT( reader.HasData() == true );
            TNvBuffer buf = { nullptr, 0 }; 
            const uint8_t* pbuffer;
            buf.size = reader.LockBuffer<uint16_t>(pbuffer);
            buf.data = const_cast<uint8_t*>(pbuffer);
            // DumpData("NagraSystem::ECMResponse", (const uint8_t*)buf.data, buf.size);
            if( _systemsession != nullptr ) {
                _systemsession->SetPrmContentMetadata(_descramblingSession, &buf, ::NV_STREAM_TYPE_DVB);
            }
            else {
              REPORT("could not handle ECMDELIVERY, no system available");
            }
            reader.UnlockBuffer(buf.size);
            break;
        }
        case Request::PLATFORMDELIVERY:
        {
            REPORT("NagraSytem importing PLATFORM Delivery");
            ASSERT( reader.HasData() == true );
            const uint8_t * pbuffer;
            size_t size = reader.LockBuffer<uint16_t>(pbuffer);
            uint8_t *data = const_cast<uint8_t *>(pbuffer);
           /* DumpData("NagraSystem::PLATFORMDelivery",
                     (const uint8_t*) data, size); */
            if( _systemsession != nullptr ) {
                _systemsession->SetPlatformMetadata(_descramblingSession, _TSID,
                                                    data, size);
            }
            else {
              REPORT("could not handle PLATFORMDELIVERY, no system available");
            }
            reader.UnlockBuffer(size);
            break;
        }
        default: /* WTF */
            break;
        }
    }
    else {
       REPORT("MediaSessionConnect::Update: expected more data");
    }
        REPORT("leave MediaSessionConnect::Update");
}

CDMi_RESULT MediaSessionConnect::Load() {
  return CDMi_S_FALSE;
}

CDMi_RESULT MediaSessionConnect::Remove() {
  return CDMi_S_FALSE;
}

CDMi_RESULT MediaSessionConnect::Close() {
  return CDMi_S_FALSE;
}

CDMi_RESULT MediaSessionConnect::Decrypt(
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
    const uint8_t* /* keyId */,
    bool /* initWithLast15 */)
{
  ASSERT(false);

  return CDMi_S_FALSE;
}

CDMi_RESULT MediaSessionConnect::ReleaseClearContent(const uint8_t*, uint32_t, const uint32_t, uint8_t*) {

  ASSERT(false);

  return CDMi_S_FALSE;
}

void MediaSessionConnect::OnKeyMessage(const uint8_t *f_pbKeyMessage, const uint32_t f_cbKeyMessage, const char *f_pszUrl)  {
    REPORT("MediaSessionConnect::OnKeyMessage triggered...");

    _lock.Lock();

    if( _callback != nullptr ) {
        _callback->OnKeyMessage(f_pbKeyMessage, f_cbKeyMessage, const_cast<char*>(f_pszUrl));
    }

    _lock.Unlock();

}


}  // namespace CDMi
