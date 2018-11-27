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

#include <core/core.h>

#include "MediaSessionConnect.h"

#include "../Report.h"

#include "../MediaRequest.h"

namespace {

class MediaSystemLoader {
    public:
    MediaSystemLoader() 
        : _SessionSystem(nullptr)
        , _syslib("DRMNagraSystem.drm") {

        REPORT("NagraConnect going to look for NagraSsystem access entry");

        if (_syslib.IsLoaded() == true) {
            REPORT("NagraConnect going to look for NagraSsystem access entry : lib was loaded");
            _SessionSystem = reinterpret_cast<CDMi::IMediaSessionSystem*(*)()>(_syslib.LoadFunction(_T("GetMediaSessionSystemInterface")));
            REPORT_EXT("NagraConnect going to look for NagraSsystem access entry result = %u", _SessionSystem);
        }
        else {
            REPORT("NagraConnect going to look for NagraSsystem access entry : lib was NOT loaded");
        }
  }

  CDMi::IMediaSessionSystem* SessionSystem() {
      CDMi::IMediaSessionSystem* retval(nullptr);

      if( _SessionSystem != nullptr ) {
          retval = _SessionSystem();
      }
      return retval;
  }

  private:
    WPEFramework::Core::Library _syslib;
    CDMi::IMediaSessionSystem* (*_SessionSystem)();
};

CDMi::IMediaSessionSystem* SessionSystem() {
  static MediaSystemLoader loader;
  return loader.SessionSystem();
}

}

namespace CDMi {

MediaSessionConnect::MediaSessionConnect(const uint8_t *data, uint32_t length)
    : _sessionId(g_NAGRASessionIDPrefix)
    , _callback(nullptr)
    , _descramblingSession(0)
    , _TSID(0)
    , _systemsession(nullptr) {

    REPORT("enter MediaSessionConnect::MediaSessionConnect"); 

    ::ThreadId tid =  WPEFramework::Core::Thread::ThreadId();
    REPORT_EXT("MediaSessionConnect threadid = %u", tid);

    DumpData("MediaSessionConnect::MediaSessionConnect", data, length);

    REPORT_EXT("going to test data access %u", length);

    REPORT("date access tested");

    if( length >= 4 ) {
        WPEFramework::Core::FrameType<0> frame(const_cast<uint8_t *>(data), length, length);
        WPEFramework::Core::FrameType<0>::Reader reader(frame, 0);

        uint16_t Emi = 0;

        REPORT("parsing pssh header");

        // parse pssh header
        int32_t error = -1; //not enough data
        uint32_t remainingsize = reader.Number<uint32_t>();
        REPORT_EXT("Found %u bytes of pssh header data", remainingsize);
        ASSERT(remainingsize >= 4); 
        remainingsize -= 4;

        if( remainingsize >= 4 ) {
            REPORT("parsing pssh id");
            uint32_t psshident = reader.Number<uint32_t>();
            error = psshident == 0x70737368 ? error : -2;   
            remainingsize -= 4;     

            if ( error == -1 && remainingsize >= 4 ) {
                REPORT("parsing pssh header");
                uint32_t header = reader.Number<uint32_t>();
                remainingsize -= 4;     
                constexpr uint16_t buffersize = 16;

                if ( remainingsize >= buffersize ) {
                    REPORT("parsing pssh systemid");
                    uint8_t buffer[buffersize];
                    reader.Copy(buffersize, buffer);
                    error = ( memcmp (buffer, CommonEncryption, buffersize)  == 0 ) ? error : -3;
                    remainingsize -= buffersize;     

                    if ( error == -1 && remainingsize >= 4 ) {
                        REPORT("parsing pssh kids");

                        const uint8_t* buffer = nullptr;
                        uint32_t KIDcount = reader.LockBuffer<uint32_t>(buffer);
                        remainingsize -= 4;
                        
                        //for now we are not interested in the KIDs
                        REPORT_EXT("Found %u of KIDs", KIDcount);
                        if( remainingsize >= ( KIDcount * 16 ) ) {
                            reader.UnlockBuffer(KIDcount * 16);    
                            remainingsize -= ( KIDcount * buffersize );

                            //now we got to the private data we are looking for...
                            if( remainingsize >= 4 ) {
                                REPORT("parsing pssh private data length");
                                uint32_t datalength = reader.Number<uint32_t>();
                                REPORT_EXT("Found %u bytes of private data", datalength);
                                remainingsize -= 4;

                                if( datalength >= 6 && remainingsize >= 6 ) {
                                  REPORT("parsing pssh private data");
                                  _TSID = reader.Number<uint32_t>();
                                  Emi = reader.Number<uint16_t>();
                                  error = 0;
                                }
                            }
                        }
                    }
                }
            }
        }

        REPORT_EXT("Read pssh header, result: %i", error);

        _systemsession =  SessionSystem();
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

    }
    else {
      REPORT("Could not initialize MediaSessionConnect, incorrect initialization data length");
    }
        REPORT("leave MediaSessionConnect::MediaSessionConnect");
}

MediaSessionConnect::~MediaSessionConnect() {

     REPORT("enter MediaSessionConnect::~MediaSessionConnect");

    if( _systemsession != nullptr ) {

      REPORT("MediaSessionConnect::~MediaSessionConnect cleaning up");
        if ( _descramblingSession != 0 ) {
          REPORT("MediaSessionConnect::~MediaSessionConnect closing session");

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

  _callback = const_cast<IMediaKeySessionCallback*>(callback);  
}

void MediaSessionConnect::Update(const uint8_t *data, uint32_t length) {
        REPORT("enter MediaSessionConnect::Update");

    REPORT_EXT("going to test data access %u", length);

    REPORT("date access tested");


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
            assert( reader.HasData() == true );
            TNvBuffer buf = { nullptr, 0 }; 
            const uint8_t* pbuffer;
            buf.size = reader.LockBuffer<uint16_t>(pbuffer);
            buf.data = const_cast<uint8_t*>(pbuffer);
            DumpData("NagraSystem::ECMResponse", (const uint8_t*)buf.data, buf.size);
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
            assert( reader.HasData() == true );
            const uint8_t * pbuffer;
            size_t size = reader.LockBuffer<uint16_t>(pbuffer);
            uint8_t *data = const_cast<uint8_t *>(pbuffer);
            DumpData("NagraSystem::PLATFORMDelivery",
                     (const uint8_t*) data, size);
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
        if( reader.HasData() ) {
        REPORT("MediaSessionConnect::Update: more data than expected");
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
    const uint8_t* /* keyId */)
{
  // System sessions should *NOT* be used for decrypting !!!!
  ASSERT(false);

  return CDMi_S_FALSE;
}

CDMi_RESULT MediaSessionConnect::ReleaseClearContent(const uint8_t*, uint32_t, const uint32_t, uint8_t*) {

  // System sessions should *NOT* be used for decrypting !!!!
  ASSERT(false);

  return CDMi_S_FALSE;
}

void MediaSessionConnect::OnNeedKey() {
     _callback->OnKeyMessage(nullptr, 0, const_cast<char*>("KEYNEEDED"));
}


}  // namespace CDMi
