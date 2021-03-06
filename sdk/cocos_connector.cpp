#include "cocos_connector.h"
#include "connector_client_api.h"
#include <stdio.h>

using namespace hmoon_connector_api;

class ConnectorApiImp : public IProtocolEventListener, public IWorkerThreadListener {
    public:
        static ConnectorApiImp& GetInstance() {
            static ConnectorApiImp api_imp;
            return api_imp;
        }

        int Start();
        void Stop();
        void Resume();

    private:
        ConnectorApiImp();

    public:
        virtual void OnGetSettings(std::string& appid, std::string& openid,
                                   std::string& token,
                                   int& encrypt_mode, int& auth_type,
                                   std::string& server_uri);

        virtual int OnHandShakeSucceed();
        virtual int OnHandShakeFailed();
        virtual int OnServerClose();
        virtual int OnQueuing();

        virtual int OnIncoming();

    public:
        virtual void OnWorkerThreadExit();

    public:
        inline bool ok() const {
            return state_ok_;
        }

        inline bool thread_exited() const {
            return exit_;
        }

        inline bool has_pkg() const {
            return has_pkg_;
        }

    private:
      bool exit_;
      bool state_ok_;

      bool has_pkg_;
};

// Logger
class Logger : public ILoggable {
    public:
        Logger(){};
        ~Logger(){};

    public:
        virtual void Log(int level, const char* log_msg) {
            cocos2d::log(log_msg);
        }
};

Logger g_logger;

ConnectorApiImp::ConnectorApiImp()
{
    exit_ = false;
    state_ok_ = false;
    has_pkg_ = false;
}

int ConnectorApiImp::Start()
{
    cocos2d::log("start connector api client\n");
    ConnectorClientProtocolThread::Default().StartThread(this, this, &g_logger);

    return 0;
}

void ConnectorApiImp::Stop()
{
}

void ConnectorApiImp::Resume()
{
}

void ConnectorApiImp::OnGetSettings(std::string& appid,
                                    std::string& openid,
                                    std::string& token,
                                    int& encrypt_mode, int& auth_type,
                                    std::string& server_uri)
{
    appid = "TEST123456";
    openid = "GUEST0987654321";
    token = "TOKEN3388";
    encrypt_mode = NOT_ENCRYPT;
    auth_type = NONE_AUTHENTICATION;
    server_uri = "tcp://172.16.11.135:8888";
}

// TODO Dispatch Event
int ConnectorApiImp::OnHandShakeSucceed()
{
    state_ok_ = true;
    return 0;
}

int ConnectorApiImp::OnHandShakeFailed()
{
    cocos2d::log("HandShake Failed\n");
    exit_ = true;

    return 0;
}

int ConnectorApiImp::OnServerClose()
{
    cocos2d::log("ServerClose Actively\n");
    exit_ = true;

    return 0;
}

int ConnectorApiImp::OnQueuing()
{
    return 0;
}

int ConnectorApiImp::OnIncoming()
{
    has_pkg_ = true;
    // TODO dispatch event
    return 0;
}

void ConnectorApiImp::OnWorkerThreadExit()
{
    cocos2d::log("WorkerThreadExit\n");
    exit_ = true;
}

// ConnectorApi
int ConnectorApi::Start()
{
    ConnectorApiImp::GetInstance().Start();
    return 0;
}

int ConnectorApi::Resume()
{
    return 0;
}

void ConnectorApi::Stop()
{
    ConnectorClientProtocolThread::Default().TerminateThread();
}

int ConnectorApi::Send(const std::string& bin)
{
    ConnectorClientProtocolThread::Default().PushMessageToSendQ(bin.c_str(), bin.size());

    return 0;
}

int ConnectorApi::Recv(std::string& bin)
{
    if (!ConnectorApiImp::GetInstance().has_pkg())
    {
        return 0;
    }

    // 1M
    static char data[1024000];
    size_t len = sizeof(data);
    ConnectorClientProtocolThread::Default().PopMessageFromRecvQ(data, &len);
    bin.assign(data, len);

    return len;
}
