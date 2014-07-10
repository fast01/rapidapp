#include "connector_session.h"
#include <glog/logging.h>

const int kDefaultOverloadLimit = 1024;

ConnectorSession::ConnectorSession() :
    state_(STATE_INIT), net_stub_(NULL), channel_id_(-1)
{}

ConnectorSession::~ConnectorSession()
{
    CleanUp();
}

int ConnectorSession::Init(EasyNet* net)
{
    if (NULL == net)
    {
        return -1;
    }

    net_stub_ = net;
    return 0;
}

void ConnectorSession::CleanUp()
{}

void ConnectorSession::ChangeState(int status_code)
{}

void ConnectorSession::SetChannelID(int channel_id)
{}


ConnectorSessionMgr::ConnectorSessionMgr() :
    overload_limit_(kDefaultOverloadLimit), session_pool_()
{}

ConnectorSessionMgr::ConnectorSessionMgr(unsigned int overload_limit) :
    overload_limit_(overload_limit), session_pool_()
{}

ConnectorSessionMgr::~ConnectorSessionMgr()
{
    for (SessionPool::iterator it = session_pool_.begin();
         it != session_pool_.end(); ++it)
    {
        if (it->second != NULL)
        {
            delete it->second;
            it->second = NULL;
        }
    }

    session_pool_.clear();
}

ConnectorSession* ConnectorSessionMgr::CreateInstance(EasyNet* net)
{
    if (NULL == net)
    {
        return NULL;
    }

    int nid = net->nid();
    SessionPool::iterator it = session_pool_.find(nid);
    if (it != session_pool_.end())
    {
        LOG(WARNING)<<"new session instance id:"<<nid<<" has existed";
        return NULL;
    }

    ConnectorSession* session = new(std::nothrow) ConnectorSession();
    if (NULL == session)
    {
        return NULL;
    }

    if (0 != session->Init(net))
    {
        delete session;
        return NULL;
    }

    session_pool_.insert(std::make_pair(nid, session));

    return session;
}

void ConnectorSessionMgr::DestroyInstance(ConnectorSession** session)
{
    if (NULL == session || NULL == *session || NULL == (*session)->net_stub_)
    {
        LOG(WARNING)<<"invalid session";
        return;
    }

    int nid = (*session)->net_stub_->nid();
    SessionPool::iterator it = session_pool_.find(nid);
    if (it == session_pool_.end())
    {
        LOG(WARNING)<<"session id:"<<nid<<" not existed";
        return;
    }

    session_pool_.erase(it);
}
