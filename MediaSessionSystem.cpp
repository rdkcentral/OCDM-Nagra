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

namespace CDMi {

MediaSessionSystem::MediaSessionSystem(const uint8_t *f_pbInitData, uint32_t f_cbInitData)
    : _sessionId()
    , _callback(nullptr) {
}

MediaSessionSystem::~MediaSessionSystem() {
}

const char *MediaSessionSystem::GetSessionId() const {
  return _sessionId.c_str();
}

const char *MediaSessionSystem::GetKeySystem(void) const {
  return _sessionId.c_str(); // FIXME : replace with keysystem and test.
}

void MediaSessionSystem::Run(const IMediaKeySessionCallback* callback) {

  assert ((callback == nullptr) ^ (_callback == nullptr));

  _callback = callback;   
}

void MediaSessionSystem::Update(const uint8_t *m_pbKeyMessageResponse, uint32_t  m_cbKeyMessageResponse) {
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
