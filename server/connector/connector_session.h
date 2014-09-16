#ifndef CONNECTOR_SESSION_H_
#define CONNECTOR_SESSION_H_

#include "rapidapp_easy_net.h"
#include <tr1/unordered_map>

using namespace rapidapp;

enum SessionState {
    STATE_INIT   = 0,
    STATE_AUTH   = 1,
    STATE_SYNING = 2,
    STATE_OK     = 3,
};

enum OperationCode {
    OPERATION_NONE  = 0,
    OPERATION_CSTOP = 1,
    OPERATION_SSTOP = 2,
};

class ConnectorSession {
    public:
        ConnectorSession();
        ~ConnectorSession();

    public:
        int Init(EasyNet* net);
        void CleanUp();

    public:
        int DriveStateMachine();

        int HandShake_StopSession();

    public:
        inline int state() const {
            return state_;
        }

        inline void set_sid(uint32_t sid) {
            sid_ = sid;
        }

        inline uint32_t sid() const {
            return sid_;
        }

    private:
        int DoAuthRequest();

        int HandShake_StartSession();
        int HandShake_OnStartAcked();
        int HandShake_OnStopNotify();

    private:
        void SetChannelID(int channel_id);

    private:
        int state_;         // 状态机状态
        EasyNet* net_stub_; // 网络连接stub
        int channel_id_;    // 后端服务器channel id
        uint32_t sid_;

        friend class ConnectorSessionMgr;
};

#endif
