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

namespace CDMi {

MediaSessionConnect::MediaSessionConnect(const uint8_t *data, uint32_t length)
    : _sessionId(g_NAGRASessionIDPrefix)
    , _callback(nullptr)
    , _applicationSession(0)
    , _descramblingSession(0) {

    if( length >= 6 ) {
        WPEFramework::Core::FrameType<0>::Reader reader(WPEFramework::Core::FrameType<0>(const_cast<uint8_t *>(data), length), 0);

       _applicationSession = reader.Number<uint32_t>();
       uint32_t TSID = reader.Number<uint32_t>();
       uint16_t Emi = reader.Number<uint16_t>();

        uint32_t result = nvDsmOpen(&_descramblingSession, _applicationSession, TSID, Emi);
        REPORT_DSM(result, "nvDsmOpen");

        _sessionId += std::to_string(_applicationSession);
    }
    else {
      REPORT("Could not initialize MediaSessionConnect, incorrect initialization data length");
    }
}

MediaSessionConnect::~MediaSessionConnect() {
}

const char *MediaSessionConnect::GetSessionId() const {
  return _sessionId.c_str();
}

const char *MediaSessionConnect::GetKeySystem(void) const {
  return _sessionId.c_str(); // FIXME : replace with keysystem and test.
}

void MediaSessionConnect::Run(const IMediaKeySessionCallback* callback) {

  ASSERT ((callback == nullptr) ^ (_callback == nullptr));

  _callback = callback;   
}

void MediaSessionConnect::Update(const uint8_t *data, uint32_t length) {
  if (length >= 2) {
    request value (static_cast<request>((data[0] | (data[1] << 8)) << 16));
    switch (value) {
      case ECMDELIVERY:
      {
       // nvImsmDecryptEMM(&(data[2]), length - 2);
        break;
      }
      default: /* WTF */
               break;

    }
  }
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

}  // namespace CDMi
