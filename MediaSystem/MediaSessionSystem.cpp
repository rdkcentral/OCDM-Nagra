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
#include "../ParsePSSHHeader.h"

#include <memory>
#include <queue>
#include <functional>
#include <utility>
#include <algorithm>

#include <nagra/nv_dpsc.h>
#include <nagra/prm_dsm.h>

namespace {

    WPEFramework::Core::CriticalSection g_lock;
/*
    Noit needed for now as we do not send a OnKeyReady message

    using DeliverySessionLookupMap = std::map<TNvSession, CDMi::MediaSessionSystem*>;
    DeliverySessionLookupMap g_DeliverySessionMap;
*/
    //note: first element is reserved for default system, CDMi::MediaSessionSystem* is nullptr if not created (so element is always there)
    using MediaSessionSystemStorageElement = std::pair<std::string, CDMi::MediaSessionSystem* >;
    using MediaSessionSystemStorage = std::forward_list< MediaSessionSystemStorageElement >;
    MediaSessionSystemStorage g_MediaSessionSystems;

    struct CreateDefaultMediaSystemSession {
        explicit CreateDefaultMediaSystemSession(const std::string& defaultoperatorvault) {
           ASSERT( g_MediaSessionSystems.empty() == true );
            g_MediaSessionSystems.push_front( MediaSessionSystemStorageElement(defaultoperatorvault, nullptr) );
        }
        ~CreateDefaultMediaSystemSession() {
            ASSERT( g_MediaSessionSystems.empty() == false );
            ASSERT( g_MediaSessionSystems.front().second == nullptr );
            g_MediaSessionSystems.pop_front();
            ASSERT( g_MediaSessionSystems.empty() == true );
        }
    };

    // we of course don't want to create a thread per system so we only have one...

    class CommandHandler : virtual public WPEFramework::Core::Thread {
    public:
        CommandHandler();
        ~CommandHandler();

        CommandHandler(const CommandHandler&) = delete;
        CommandHandler& operator=(const CommandHandler&) = delete;

        // no capture by move in C++11 yet, let's do it ourselves
        class Data {
            public:

            explicit Data(CDMi::MediaSessionSystem::DataBuffer&& buffer)
            : _databuffer(std::move(buffer)) {
            }

            Data(Data&& other) 
            : _databuffer(std::move(other._databuffer)) {

            }

            const CDMi::MediaSessionSystem::DataBuffer& DataBuffer() const {
                return _databuffer;
            }

            Data& operator=(Data&& other) {
                if( this != &other ) {
                    _databuffer = std::move(other._databuffer);
                }
                return *this;
            }

            private:

            CDMi::MediaSessionSystem::DataBuffer _databuffer;  
        };

        using Command = std::function<void(const CDMi::MediaSessionSystem::DataBuffer&)>;
        
        void PostCommand(Command&& command, Data&& data);
        
    protected:
        uint32_t Worker() override;

        bool CommandVailable() const {
            return ( _commands.empty() == false );
        }

    private:
        using CommandPair = std::pair<Command, Data>;
        using CommandsContainer = std::queue< CommandPair >;
        CommandsContainer _commands;
        WPEFramework::Core::CriticalSection _lock;
    };

    void PostCommandJob(CommandHandler::Command&& command, CDMi::MediaSessionSystem::DataBuffer&& data) { // this makes sure we do not start the thread before it is actually needed, not just when the drm is loaded
        static CommandHandler commandhandler;

        TRACE_L1("Posting a command job, native buffer %p", data.data());

        commandhandler.PostCommand(std::move(command), CommandHandler::Data(std::move(data)));
    }
}

#ifdef __cplusplus
extern "C" {
#endif


CDMi::IMediaSessionSystem* GetMediaSessionSystemInterface(const char* systemsessionid) {

    TRACE_L1("Getting MediaSessionSystemInterface for %s", systemsessionid);

    CDMi::IMediaSessionSystem* result( nullptr );

    g_lock.Lock();

    if( systemsessionid == nullptr ) { // note: nice and fast for the default case
        TRACE_L1("Getting MediaSessionSystemInterface default one");
        ASSERT(g_MediaSessionSystems.empty() == false);
        result = g_MediaSessionSystems.front().second;
    }
    else{
        //in case of the sessionid we now have to search for the right proxy systemid (please note systemid needs to be unique for higher OCDM layers otherwise tou get into trouble). 
        // Now we just use the poxy ptr aqnd do a lookup here. There will not be that many proxies and connectsessions so it will not be a real bottleneck but perhaps we should have 
        // scheme where the sessionid of the system itself is also part of the proxyid (but you do not want to make the proxyif too large so for now this will work)
        TRACE_L1("Getting MediaSessionSystemInterface specific one");
        auto index( g_MediaSessionSystems.begin() ); // do not skip the default one, no problem if we look for it explicitely
        while( index != g_MediaSessionSystems.end() ) {
            CDMi::MediaSessionSystem* system = index->second;
            if( ( system != nullptr ) && ( system->HasProxyWithSessionID(systemsessionid) ) ) { // system == nullptr is valid for default system
                result = system;
                TRACE_L1("Getting MediaSessionSystemInterface specific one found! selected sessionid %s", system->GetSessionId());
                break;
            }
            ++index;
        }
    }

    if( result != nullptr ) {
        result->Addref();
    }

    g_lock.Unlock();

    return result;
}

#ifdef __cplusplus
}
#endif

namespace CDMi {

void MediaSessionSystem::MediaSessionSystemProxy::Run(const IMediaKeySessionCallback* f_piMediaKeySessionCallback) {
    ASSERT ((f_piMediaKeySessionCallback == nullptr) ^ (_callback == nullptr));
    g_lock.Lock(); // note changing the callback needs to be protected (certainly for setting it to nullptr as it can be called from a different thread
    if( f_piMediaKeySessionCallback != nullptr ) {
        _callback = const_cast<IMediaKeySessionCallback*>( f_piMediaKeySessionCallback );
        _system.Run(*_callback);
    }
    else {
        _callback = nullptr;
    }
    g_lock.Unlock();
} 


/* static */ IMediaKeySession* MediaSessionSystem::CreateMediaSessionSystem(const uint8_t *f_pbInitData, const uint32_t f_cbInitData, const std::string& defaultoperatorvault, const std::string& licensepath) {
        TRACE_L1("Create MediaSessionSystem called");

    return new MediaSessionSystem::MediaSessionSystemProxy( AddMediaSessionInstance(f_pbInitData, f_cbInitData, defaultoperatorvault, licensepath) );
}

/* static */ void MediaSessionSystem::DestroyMediaSessionSystem(IMediaKeySession* systemsession) {
    ASSERT( systemsession != nullptr );
    TRACE_L1("Destroy MediaSessionSystem called");
    delete systemsession;
}

/* static */ MediaSessionSystem& MediaSessionSystem::AddMediaSessionInstance(const uint8_t *f_pbInitData, const uint32_t f_cbInitData, const std::string& defaultoperatorvault, const std::string& licensepath) {
    static CreateDefaultMediaSystemSession createdefaultmediasession(defaultoperatorvault);

    // DumpData("MediaSessionSystem::CreateMediaSessionSystem", f_pbInitData, f_cbInitData);
    MediaSessionSystem* system = nullptr;

    g_lock.Lock();

    if( f_cbInitData == 0 ) { //we are the default media session
        ASSERT( g_MediaSessionSystems.empty() == false );
        if( g_MediaSessionSystems.front().second == nullptr ) { // default session was not there yet
 
            ASSERT( g_MediaSessionSystems.front().first == defaultoperatorvault );
            system = new MediaSessionSystem(nullptr, 0, defaultoperatorvault, licensepath);
            g_MediaSessionSystems.front().second = system;
        }
        else {
            ASSERT(g_MediaSessionSystems.front().first == defaultoperatorvault);
            system = g_MediaSessionSystems.front().second;
            system->Addref();
        }
    }
    else {
        // session created on operator vault
        // now we should have a pssh header 
        const uint8_t *privatedata = f_pbInitData;
        int32_t result = FindPSSHHeaderPrivateData(privatedata, f_cbInitData);
        if( result > 0 ) {

            std::string operatorvault(reinterpret_cast<const char*>(privatedata), result);

            auto index( g_MediaSessionSystems.begin() ); //let's also take the default into account, if it is the same as the explicit file they are the same system
            while( index != g_MediaSessionSystems.end() ) {
                if( index->first == operatorvault ) {
                    system = index->second;    
                    system->Addref();
                    break;
                }
                ++index;
            }
            if( system == nullptr ) {
                // okay, system was not created for this operator vault yet
                system = new MediaSessionSystem(nullptr, 0, operatorvault, licensepath);
             g_MediaSessionSystems.insert_after(g_MediaSessionSystems.begin(), MediaSessionSystemStorageElement(operatorvault, system));
            }
        }
        else {
            REPORT_EXT("incorrect pssh header or no private data: %i", result);
        }
    }

    g_lock.Unlock();

    ASSERT( system != nullptr ); // we should have a system now...

    return *system;
}

/* static */ void MediaSessionSystem::RemoveMediaSessionInstance(MediaSessionSystem* session) {
    // should already be in the lock...

    for( auto index = g_MediaSessionSystems.begin(), previousindex = g_MediaSessionSystems.before_begin(); index != g_MediaSessionSystems.end(); previousindex = index, ++index ) {
        if( index->second == session)  {
            if( index == g_MediaSessionSystems.begin() ) { //first index is default system, only remove pointer
                index->second = nullptr;
            }
            else {
                g_MediaSessionSystems.erase_after(previousindex);
            }
            break;
        }
    }
}

/* static */ bool MediaSessionSystem::OnRenewal(TNvSession appSession) {
    REPORT("static NagraSystem::OnRenewal triggered");

    g_lock.Lock(); //mhmm, is this necessary? Would it be possible if Nagra still calls back when we are destructing the system? Guess so... Then we assume MediaSessionSystemFromAsmHandle to return  afterwards, should we set the Nagra callback to null to prevent this, then we can get rid of the lock. But then we need to protect the proxies registered, don't forget 

    MediaSessionSystem* session = MediaSessionSystemFromAsmHandle(appSession);
    if( session != nullptr ) {
        session->OnRenewal();
    }

    g_lock.Unlock();


    return true;
}

void MediaSessionSystem::OnRenewal() {
    REPORT("NagraSystem::OnRenewal triggered");

    if ( AnyCallBackSet() == true ) {
        PostRenewalJob();
    }
    else {
       RequestReceived(Request::RENEWAL);
    }
}

/* static */ bool MediaSessionSystem::OnNeedKey(TNvSession appSession, TNvSession descramblingSession, TNvKeyStatus keyStatus,  TNvBuffer* content, TNvStreamType streamtype) {

    REPORT("static NagraSystem::OnNeedkey triggered");

    g_lock.Lock();

    MediaSessionSystem* session = MediaSessionSystemFromAsmHandle(appSession);
    if( session != nullptr ) {
        session->OnNeedKey(descramblingSession, keyStatus, content, streamtype);
    }

    g_lock.Unlock();

    return true;
}

void MediaSessionSystem::OnNeedKey(const TNvSession descramblingSession, const TNvKeyStatus keyStatus, const TNvBuffer* content, const TNvStreamType streamtype) {

    REPORT_EXT("NagraSystem::OnNeedkey triggered for descrambling session %u", descramblingSession);

//    if(content != nullptr) {
//        DumpData("NagraSystem::OnNeedKey", (const uint8_t*)(content->data), content->size);
//    }

    if ( AnyCallBackSet() == true || descramblingSession != 0 ) {

        // TNvSession deliverysession = OpenKeyNeedSession();
      
        TNvSession deliverysession = _renewalSession;

        uint32_t result = nvLdsUsePrmContentMetadata(deliverysession, content, streamtype);
        REPORT_LDS(result,"nvLdsUsePrmContentMetadata");
        REPORT("NagraSystem::OnNeedkey ContextMetadata set");

        TNvBuffer buf = { NULL, 0 };

        result = nvLdsExportMessage(deliverysession, &buf);
        REPORT_LDS(result, "nvLdsExportMessage");

        if( result == NV_LDS_SUCCESS ) {

            DataBuffer buffer(buf.size);
            buf.data = static_cast<void*>(buffer.data());
            buf.size = buffer.size(); // just too make sure...
            result = nvLdsExportMessage(deliverysession, &buf);
            REPORT_LDS(result, "nvLdsExportMessage");

            if( result == NV_LDS_SUCCESS ) {
                // DumpData("NagraSystem::OnNeedKey export message", buffer.data(), buffer.size());

                if( descramblingSession == 0 ) {
                    REPORT("NagraSystem::OnNeedkey triggered for system session");

                    Addref(); // make sure we keep this alive for the lambda
                    PostCommandJob([=](const DataBuffer& data){
                        g_lock.Lock(); // could now better be lock per system
                        for( auto proxy : _systemproxies ) {
                            IMediaKeySessionCallback* callback( proxy->IMediaKeyCallback() );
                            if( callback != nullptr ) {
                                    callback->OnKeyMessage(reinterpret_cast<const uint8_t*>(data.data()), data.size(), const_cast<char*>("KEYNEEDED"));
                            }
                        }
                        g_lock.Unlock();
                        Release();
                    }
                    , std::move(buffer));
                }
                else {
                    REPORT("NagraSystem::OnNeedkey triggered for connect session");

                    Addref(); // make sure we keep this alive for the lambda
                    PostCommandJob([=](const DataBuffer& data){
                        g_lock.Lock(); // could now better be lock per system
                        auto it = _connectsessions.find(descramblingSession); 
                        if( it != _connectsessions.end() ) {
                            it->second->OnKeyMessage(reinterpret_cast<const uint8_t*>(data.data()), data.size(), const_cast<char*>("KEYNEEDED"));
                        }
                        g_lock.Unlock();
                        Release();
                    }
                    , std::move(buffer));
                }
            }
        }
    }
}

/* static */ bool MediaSessionSystem::OnDeliveryCompleted(TNvSession deliverySession) {

// we'll leave this in right now for logging, if we are not going to respond with KeyReady or KeyError we'll remove it
// See if this is the right callback for that

    g_lock.Lock(); // lock not really needed anymore as we are not doing something usefull with the system

    REPORT("MediaSessionSystem::OnDeliveryCompleted");

    TNvLdsStatus status;
    uint32_t result = nvLdsGetResults(deliverySession, &status);
    REPORT_LDS(result,"nvLdsGetResults");

    REPORT_EXT("OnDeliveryCompleted result %i", status.status);

//    DeliverySessionLookupMap::iterator index (g_DeliverySessionMap.find(deliverySession));

//    if ( index != g_DeliverySessionMap.end() ) {
//        index->second->OnDeliverySessionCompleted(deliverySession); 
//    }

    g_lock.Unlock();

    return true;
}

/*

// this should be rerouted to the connectsession and also guess also OnKeyError added I guess. Note that the code below does not use the separation by the CommandHanlder, change that before uncommenting
void MediaSessionSystem::OnDeliverySessionCompleted(const TNvSession deliverySession) {

    for( auto proxy : _systemproxies ) {
        IMediaKeySessionCallback* callback( proxy->IMediaKeyCallback() );
        if( callback != nullptr ) {
                callback->OnKeyReady();
        }
    }

//   CloseDeliverySession(deliverySession);
}

*/

void MediaSessionSystem::GetFilters(FilterStorage& filters) {
    filters.clear();
    if( _applicationSession != 0 ) {
        uint8_t numberOfFilters = 0;
        uint32_t result = nvImsmGetFilters(_applicationSession, nullptr, &numberOfFilters);
        REPORT_IMSM(result, "nvImsmGetFilters");
        if( result == NV_IMSM_SUCCESS ) {
            filters.resize(numberOfFilters * sizeof(TNvFilter));
            result = nvImsmGetFilters(_applicationSession, reinterpret_cast<TNvFilter*>(filters.data()), &numberOfFilters); 
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

        // DumpData("System::ProvisioningParameters", buffer.data(), buffer.size());


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
                      // DumpData("NagraSystem::ProvisioningExportMessage", buffer.data(), buffer.size());
                  }
                  else {
                      buffer.clear();
                  }
              }
            }
        }
    }
}

/*

TNvSession MediaSessionSystem::OpenKeyNeedSession() {
    TNvSession session = OpenDeliverySession();
    if( session != 0 ) {
        _needKeySessions.insert(session);
    }
    return session;
}

*/
void MediaSessionSystem::OpenRenewalSession() {
    ASSERT( _renewalSession == 0 );
    _renewalSession = OpenDeliverySession();
}

TNvSession MediaSessionSystem::OpenDeliverySession() {   
    TNvSession session(0);
    uint32_t result = nvLdsOpen(&session, _applicationSession);
    REPORT_LDS(result,"nvLdsOpen");
    if( result == NV_LDS_SUCCESS ) {
  //      g_DeliverySessionMap[session] = this;
        nvLdsSetOnCompleteListener(session, OnDeliveryCompleted);
    }
    return session;
}

/*
void MediaSessionSystem::CloseDeliverySession(const TNvSession session) {

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
*/

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
            // DumpData("NagraSystem::RenewalExportMessage", buffer.data(), buffer.size());
        }
        else {
            buffer.clear();
        }
    }
}

void MediaSessionSystem::InitializeWhenProvisoned() {
    REPORT("enter MediaSessionSystem::MediaSessionSystem");

    //protect as much as possible to an unexpected update(provioning) call ;)

    uint32_t result = nvAsmSetContext(_applicationSession, this);
    REPORT_ASM(result, "nvAsmSetContext");

    if( _renewalSession == 0 ) {
        OpenRenewalSession(); //do before callbacks are set, so no need to do this insside the lock
    }
    result = nvAsmSetOnRenewalListener(_applicationSession, OnRenewal);
    REPORT_ASM(result, "nvAsmSetOnRenewalListener");
    result = nvAsmSetOnNeedKeyListener(_applicationSession, OnNeedKey);
    REPORT_ASM(result, "nvAsmSetOnNeedKeyListener");
    if( _inbandSession == 0 ) {
        result = nvImsmOpen(&_inbandSession, _applicationSession);
        REPORT_IMSM(result, "nvImsmOpen");
    }

    result = nvAsmUseStorage(_applicationSession, const_cast<char *>(_licensepath.c_str()));
    REPORT_ASM(result, "nvAsmUseStorage");

    REPORT("enter MediaSessionSystem::MediaSessionSystem");
}

void MediaSessionSystem::HandleFilters(IMediaKeySessionCallback* callback) {
    REPORT("MediaSessionSystem::Run checking filters");
    FilterStorage filters;
    GetFilters(filters);

    REPORT_EXT("MediaSessionSystem::Run %i filter found", filters.size());
    if( filters.size() > 0 ) {
        REPORT("MediaSessionSystem::Run firing filters ");
        Addref(); // keep session alive for callback
        PostCommandJob([=](const DataBuffer& data){
            g_lock.Lock(); // could now better be lock per system

            TRACE_L1("Handle filters: in filter callback job, native buffer ptr %p:", data.data());

            if( callback == nullptr ) { //triggered only after Provisioing complete, so now we will sent out the first filter results to all registered callbacks
                ASSERT( AnyCallBackSet() == true ); //at least one should be set as the Run was already triggered
                for( auto proxy : _systemproxies ) {
                    IMediaKeySessionCallback* proxycallback( proxy->IMediaKeyCallback() );
                    if( proxycallback != nullptr ) {
                        proxycallback->OnKeyMessage(data.data(), data.size(), const_cast<char*>("FILTERS"));
                    }
                }
            }
            else { // in this case we already sent the filters to the previous registering callbacks, now only update the new one
                //as we are doing this on another thread at a later moment let's check if the callback is still registered
                auto it = std::find_if(_systemproxies.begin(), _systemproxies.end(), [=](const MediaSessionSystemProxy* proxy){ return (proxy == nullptr ? false : proxy->IMediaKeyCallback() == callback); } );
                if( it != _systemproxies.end()) {
                    callback->OnKeyMessage(data.data(), data.size(), const_cast<char*>("FILTERS"));
                }
            }
            g_lock.Unlock();
            Release();
        }
        , std::move(filters));
    }
 }

MediaSessionSystem::MediaSessionSystem(const uint8_t *data, const uint32_t length, const std::string& operatorvault, const std::string& licensepath)
    : _sessionId(g_NAGRASessionIDPrefix)
    , _requests(Request::NONE)
    , _applicationSession(0)
    , _inbandSession(0) 
 //   , _needKeySessions()
    , _renewalSession(0)
    , _provioningSession(0)
    , _connectsessions()
    , _licensepath(licensepath)
    , _systemproxies()
    , _referenceCount(1) {

    REPORT_EXT("operator vault location %s", operatorvault.c_str());
   REPORT_EXT("license path location %s", _licensepath.c_str());

    REPORT("enter MediaSessionSystem::MediaSessionSystem");

    ::ThreadId tid =  WPEFramework::Core::Thread::ThreadId();
    REPORT_EXT("MediaSessionSystem threadid = %u", tid);


    REPORT_EXT("going to test data access %u", length);

    REPORT("date access tested");

    OperatorVault vault(operatorvault.c_str());
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
 
    if( result == NV_ASM_SUCCESS ) {
        InitializeWhenProvisoned();
    }

    REPORT("leave MediaSessionSystem::MediaSessionSystem");
}

MediaSessionSystem::~MediaSessionSystem() {

    // note: should be in lock

    REPORT("enter MediaSessionSystem::~MediaSessionSystem");

    // note will correctly handle if InitializeWhenProvisoned() was never called

    if( _inbandSession != 0 ) {
        nvImsmClose(_inbandSession);
    }

    CloseProvisioningSession();

    nvLdsClose(_renewalSession);
    _renewalSession = 0;


  //  CloseDeliverySession(_renewalSession);

 //   for(TNvSession session : _needKeySessions) {
 //       CloseDeliverySession(session);
 //   }  

    nvAsmClose(_applicationSession);

    RemoveMediaSessionInstance(this);
    
    REPORT("enter MediaSessionSystem::~MediaSessionSystem");

}

const char *MediaSessionSystem::GetSessionId() const {
  return SessionId().c_str();
}

const char *MediaSessionSystem::GetKeySystem(void) const {
  return SessionId().c_str(); // FIXME : replace with keysystem and test.
}

void MediaSessionSystem::Run(IMediaKeySessionCallback& callback) {

    // Already in lock

    REPORT("MediaSessionSystem::Run");

    ASSERT(AnyCallBackSet() == true);

    if (WasRequestReceived(Request::PROVISION)) {
        REPORT("MediaSessionSystem::Run firing provisoning ");

        // we just post the job without taking into regards the first requester, all registered callbacks at the time the callback is called will be notified
        PostProvisionJob();
        RequestHandled(Request::PROVISION);
    } 
    else {
        // no provisioning needed, we can already fire the filters now...
        HandleFilters(&callback);
    }

    if (WasRequestReceived(Request::RENEWAL)) {
        REPORT("MediaSessionSystem::Run firing renewal ");

        // we just post the job without taking into regards the first requester, all registered callbacks at the time the callback is called will be notified
        PostRenewalJob();
        RequestHandled(Request::RENEWAL);
    }

}

void MediaSessionSystem::Update(const uint8_t *data, uint32_t  length) {

    REPORT("enter MediaSessionSystem::Update");

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
            ASSERT( reader.HasData() == true );
            string response = reader.Text();
            TNvBuffer buf = { const_cast<char*>(response.c_str()), response.length() + 1 }; 
            // DumpData("NagraSystem::RenewalResponse|Keyneeded", (const uint8_t*)buf.data, buf.size);
            uint32_t result = nvLdsImportMessage(_renewalSession, &buf); 
            REPORT_LDS(result, "nvLdsImportMessage");
            break;
        }
        case Request::EMMDELIVERY:
        {
            REPORT("NagraSytem importing EMM response");
            ASSERT( reader.HasData() == true );
            TNvBuffer buf = { nullptr, 0 }; 
            const uint8_t* pbuffer;
            buf.size = reader.LockBuffer<uint16_t>(pbuffer);
            buf.data = const_cast<uint8_t*>(pbuffer);
            // DumpData("NagraSystem::EMMResponse", (const uint8_t*)buf.data, buf.size);
            uint32_t result = nvImsmDecryptEMM(_inbandSession, &buf); 
            REPORT_IMSM(result, "nvImsmDecryptEMM");
            reader.UnlockBuffer(buf.size);
            break;
        }
        case Request::PROVISION:
        {
            REPORT("NagraSytem importing provsioning response");
            ASSERT( reader.HasData() == true );
            string response = reader.Text();
            TNvBuffer buf = { const_cast<char*>(response.c_str()), response.length() + 1 }; 
            //DumpData("NagraSystem::ProvisionResponse", (const uint8_t*)buf.data, buf.size);
            uint32_t result = nvDpscImportMessage(_provioningSession, &buf);
            REPORT_DPSC(result, "nvDpscImportMessage");
            CloseProvisioningSession();
            InitializeWhenProvisoned();
            // handle the filters as that was postponed untill provisioning was complete...
            g_lock.Lock();           
            HandleFilters(nullptr);
            g_lock.Unlock();           
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
    const uint8_t* /* keyId */,
    bool /* initWithLast15 */)
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

    g_lock.Lock(); // note:we could use a more find grained locking to only protect the _connectsessions

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

    g_lock.Lock(); // note:we could use a more find grained locking to only protect the _connectsessions

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

    if (WPEFramework::Core::InterlockedDecrement(_referenceCount) == 0) {
        delete this;
        REPORT("MediaSessionSystem::Release deleted");

        uint32_t retval = WPEFramework::Core::ERROR_DESTRUCTION_SUCCEEDED;
    }

    g_lock.Unlock();

     REPORT("leave MediaSessionSystem::Release");
    return retval;
}

void MediaSessionSystem::RegisterMediaSessionSystemProxy(MediaSessionSystemProxy* proxy) {
    ASSERT(proxy != nullptr);
    g_lock.Lock(); // note:we could use a more find grained locking to only protect the _systemproxies but would not make that big of a differce as you need g_lock anyway when creating a poxy

    _systemproxies.push_front(proxy); 

    g_lock.Unlock(); 
}

void MediaSessionSystem::DeregisterMediaSessionSystemProxy(MediaSessionSystemProxy* proxy) {
    ASSERT(proxy != nullptr);
    g_lock.Lock(); 

    _systemproxies.remove( proxy ); 

    g_lock.Unlock(); 

}

void MediaSessionSystem::PostProvisionJob() {
    DataBuffer buffer;
    GetProvisionChallenge(buffer);
    Addref(); // make sure we keep this alive for the lambda
    PostCommandJob([=](const DataBuffer& data){
        g_lock.Lock(); // could now better be lock per system
        for( auto proxy : _systemproxies ) {
            IMediaKeySessionCallback* callback( proxy->IMediaKeyCallback() );
            if( callback != nullptr ) {
                callback->OnKeyMessage(data.data(), data.size(), const_cast<char*>("PROVISION"));
            }
        }
        g_lock.Unlock();
        Release();
    }
    , std::move(buffer));
}

void MediaSessionSystem::PostRenewalJob() {
    DataBuffer buffer;
    CreateRenewalExchange(buffer);
    Addref(); // make sure we keep this alive for the lambda
    PostCommandJob([=](const DataBuffer& data){
        g_lock.Lock(); // could now better be lock per system
        for( auto proxy : _systemproxies ) {
            IMediaKeySessionCallback* callback( proxy->IMediaKeyCallback() );
            if( callback != nullptr ) {
                callback->OnKeyMessage(data.data(), data.size(), const_cast<char*>("RENEWAL"));
            }
        }
        g_lock.Unlock();
        Release();
    }
    , std::move(buffer));
}


}  // namespace CDMi

// ---------------------------------------------------
// CommandHandler 
// ---------------------------------------------------

namespace {

    CommandHandler::CommandHandler()
        : WPEFramework::Core::Thread(WPEFramework::Core::Thread::DefaultStackSize(), "Nagra DRM Session Commandhandler")
        , _commands()
        ,_lock() {
    }

    CommandHandler::~CommandHandler() {
       Stop();
            
        Wait(Thread::STOPPED,  WPEFramework::Core::infinite);
    }

    void CommandHandler::PostCommand(Command&& command, Data&& data) {
        _lock.Lock();           
        _commands.push(CommandPair(std::move(command), std::move(data)));
        if( _commands.size() == 1 ) {
            Run();
        }
        _lock.Unlock();
    }

    uint32_t CommandHandler::Worker() {
        while( IsRunning() == true ) {
            _lock.Lock();
            if( CommandVailable() == true) {
                Data data(std::move(std::get<1>(_commands.front())));
                Command command(std::move(std::get<0>(_commands.front())));
                _commands.pop();
                _lock.Unlock();

                command(data.DataBuffer());
            }
            else {
                Block(); //needs to be in lock to prevent racecondition with Run()  
                _lock.Unlock();
            }
        }
        return WPEFramework::Core::infinite;
    }
}
