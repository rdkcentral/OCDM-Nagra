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

#include <interfaces/IDRM.h> 
#include "MediaSessionSystem.h"

#include <core/core.h>
#include "../Report.h"

namespace CDMi {

namespace {

    class CCLInitialize {
        CCLInitialize(const CCLInitialize&) = delete;
        CCLInitialize& operator= (const CCLInitialize&) = delete;

    public:
        CCLInitialize() {
            int rc = nagra_cma_platf_init();
            if ( rc == NAGRA_CMA_PLATF_OK ) {
                bool result = nvInitialize();
                if ( result == false ) {
                    REPORT("Call to nvInitialize failed");
                }
            } else {
                REPORT_EXT("Call to nagra_cma_platf_init failed (%d)", rc);
            }
        }

        ~CCLInitialize() {
            REPORT("Calling nvTerminate");
            nvTerminate();
            int rc = nagra_cma_platf_term();
            if ( rc != NAGRA_CMA_PLATF_OK ) {
                REPORT_EXT("Call to nagra_cma_platf_term failed (%d)", rc);
            }
        }

    };

    static CCLInitialize g_CCLInit;

}

class NagraSystem : public IMediaKeys {
private:

    class Config : public WPEFramework::Core::JSON::Container {
    private:
        Config& operator= (const Config&);

    public:
        Config () 
            : OperatorVaultPath()
            , LicensePath() {
            Add("operatorvault", &OperatorVaultPath);
            Add("licensepath", &LicensePath);
        }
        Config (const Config& copy) 
            : OperatorVaultPath(copy.OperatorVaultPath)
            , LicensePath(copy.LicensePath) {
            Add("operatorvault", &OperatorVaultPath);
            Add("licensepath", &LicensePath);
        }
        virtual ~Config() {
        }

    public:
        WPEFramework::Core::JSON::String OperatorVaultPath;
        WPEFramework::Core::JSON::String LicensePath;
    };

    NagraSystem& operator= (const NagraSystem&) = delete;

public:
    NagraSystem(const NagraSystem& system)
    : _operatorvaultpath(system._operatorvaultpath)
    , _licensepath(system._licensepath) {
    }

    NagraSystem() 
    : _operatorvaultpath()
    , _licensepath() {
    }
    ~NagraSystem() {
    }

   void OnSystemConfigurationAvailable(const std::string& configline) {
        Config config; 
        config.FromString(configline);
        _operatorvaultpath = config.OperatorVaultPath.Value();
        _licensepath = config.LicensePath.Value();
    }

    CDMi_RESULT CreateMediaKeySession(
        const std::string& keySystem,
        int32_t licenseType,
        const char *f_pwszInitDataType,
        const uint8_t *f_pbInitData,
        uint32_t f_cbInitData, 
        const uint8_t *f_pbCDMData,
        uint32_t f_cbCDMData, 
        IMediaKeySession **f_ppiMediaKeySession);

    CDMi_RESULT SetServerCertificate(
        const uint8_t *f_pbServerCertificate,
        uint32_t f_cbServerCertificate) {

        return CDMi_S_FALSE;
    }

    CDMi_RESULT DestroyMediaKeySession(IMediaKeySession* f_piMediaKeySession) {

        CDMi::MediaSessionSystem::DestroyMediaSessionSystem(f_piMediaKeySession);

        return CDMi_SUCCESS; 
    }

    private:
    std::string _operatorvaultpath;
    std::string _licensepath;
};

static SystemFactoryType<NagraSystem> g_instanceSystem({"video/x-h264", "audio/mpeg"});

CDMi_RESULT NagraSystem::CreateMediaKeySession(
    const std::string& /* keySystem */,
    int32_t licenseType,
    const char *f_pwszInitDataType,
    const uint8_t *f_pbInitData,
    uint32_t f_cbInitData, 
    const uint8_t *f_pbCDMData,
    uint32_t f_cbCDMData, 
    IMediaKeySession **f_ppiMediaKeySession) {

    *f_ppiMediaKeySession = CDMi::MediaSessionSystem::CreateMediaSessionSystem(f_pbInitData, f_cbInitData,  _operatorvaultpath, _licensepath);

    return CDMi_SUCCESS; 
}


}  // namespace CDMi

CDMi::ISystemFactory* GetSystemFactory() {

    return (&CDMi::g_instanceSystem); 
}
