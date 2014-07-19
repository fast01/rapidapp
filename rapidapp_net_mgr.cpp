#include "rapidapp_net_mgr.h"
#include <glog/logging.h>
#include <cstdlib>

namespace rapidapp {

const int kDefaultUpSize = 1024 * 1024;  // 1M

NetHandlerMgr::NetHandlerMgr()
{
    recv_buffer_.buffer = NULL;
    recv_buffer_.size = 0;
}

NetHandlerMgr::~NetHandlerMgr()
{}

int NetHandlerMgr::Init(size_t recv_buff_size)
{
    if (0 == recv_buff_size)
    {
        recv_buff_size = kDefaultUpSize;
    }

    recv_buffer_.buffer = static_cast<char*>(calloc(recv_buff_size, 1));
    if (NULL == recv_buffer_.buffer)
    {
        PLOG(ERROR)<<"calloc size:"<<recv_buff_size<<" failed";
        return -1;
    }
    recv_buffer_.size = recv_buff_size;

    return 0;
}

void NetHandlerMgr::CleanUp()
{
    HandlerPool::iterator it = handler_pool_.begin();
    for (; it != handler_pool_.end();)
    {
        EasyNet* net = it->second;
        if (net != NULL)
        {
            net->CleanUp();
            delete net;
        }

        ++it;
    }

    handler_pool_.clear();

    if (recv_buffer_.buffer != NULL)
    {
        free(recv_buffer_.buffer);
        recv_buffer_.buffer = NULL;
        recv_buffer_.size = 0;
    }
}

EasyNet* NetHandlerMgr::AddHandlerByUri(const char* uri, int type,
                                        struct event_base* event_base)
{
    if (NULL == uri || NULL == event_base)
    {
        PLOG(ERROR)<<"null uri || null event_base";
        return NULL;
    }

    EasyNet* easy_net_handler = new(std::nothrow) EasyNet();
    if (NULL == easy_net_handler)
    {
        PLOG(ERROR)<<"new EasyNet failed";
        return NULL;
    }

    int ret = easy_net_handler->Connect(uri, type, event_base);
    if (ret != 0)
    {
        LOG(ERROR)<<"connect failed, uri:"<<uri;
        delete easy_net_handler;
        return NULL;
    }

    AddHandlerToMap(easy_net_handler);

    return easy_net_handler;
}

EasyNet* NetHandlerMgr::AddHandlerBySocket(evutil_socket_t sock_fd, int type,
                                           struct event_base* event_base)
{
    if (sock_fd < 0)
    {
        PLOG(ERROR)<<"sock fd:("<<sock_fd<<") < 0 || null event_base";
        return NULL;
    }

    EasyNet* easy_net_handler = new(std::nothrow) EasyNet();
    if (NULL == easy_net_handler)
    {
        PLOG(ERROR)<<"new EasyNet failed";
        return NULL;
    }

    int ret = easy_net_handler->Init(sock_fd, type, event_base);
    if (ret != 0)
    {
        LOG(ERROR)<<"init EasyNet failed by sock:"<<sock_fd;
        delete easy_net_handler;
        return NULL;
    }

    AddHandlerToMap(easy_net_handler);

    return easy_net_handler;
}

int NetHandlerMgr::AddHandlerToMap(EasyNet* easy_net_handler)
{
    if (NULL == easy_net_handler)
    {
        LOG(ERROR)<<"null easy_net handler";
        return -1;
    }

    struct bufferevent* event = easy_net_handler->bufferevent();
    if (NULL == event)
    {
        LOG(ERROR)<<"null bufferevent handler";
        return -1;
    }

    evutil_socket_t fd = bufferevent_getfd(event);
    handler_pool_[fd] = easy_net_handler;

    return 0;
}

int NetHandlerMgr::RemoveHandler(EasyNet* easy_net_handler)
{
    if (NULL == easy_net_handler)
    {
        LOG(ERROR)<<"null easy_net handler";
        return -1;
    }

    return RemoveHandlerByEvent(easy_net_handler->bufferevent());
}

/*
 * 在系统内部，fd是唯一的，并且系统对fd变化是敏感的。
 * 因而内部接口以fd为key，是可以保证一致的
 * */
int NetHandlerMgr::ChangeNetStateByEvent(struct bufferevent* event, enum NetState state)
{
    if (NULL == event)
    {
        LOG(ERROR)<<"null bufferevent handler";
        return -1;
    }

    evutil_socket_t fd = bufferevent_getfd(event);
    HandlerPool::iterator it = handler_pool_.find(fd);
    if (it != handler_pool_.end())
    {
        EasyNet* easy_net_handler = it->second;
        if (easy_net_handler != NULL)
        {
            easy_net_handler->state_ = state;
        }
    }
    else
    {
        LOG(INFO)<<"no handler found by fd:"<<fd;
    }

    return 0;
}

int NetHandlerMgr::RemoveHandlerByEvent(struct bufferevent* event)
{
    if (NULL == event)
    {
        LOG(ERROR)<<"null bufferevent handler";
        return -1;
    }

    evutil_socket_t fd = bufferevent_getfd(event);
    HandlerPool::iterator it = handler_pool_.find(fd);
    if (it != handler_pool_.end())
    {
        EasyNet* easy_net_handler = it->second;
        if (easy_net_handler != NULL)
        {
            easy_net_handler->DestroyUserContext();
            easy_net_handler->CleanUp();
            delete easy_net_handler;
        }

        handler_pool_.erase(it);
    }
    else
    {
        LOG(INFO)<<"no handler found by fd:"<<fd;
    }

    return 0;
}

EasyNet* NetHandlerMgr::GetHandlerByEvent(struct bufferevent* event)
{
    if (NULL == event)
    {
        LOG(ERROR)<<"null bufferevent handler";
        return NULL;
    }

    evutil_socket_t fd = bufferevent_getfd(event);
    HandlerPool::iterator it = handler_pool_.find(fd);
    if (it != handler_pool_.end())
    {
        return it->second;
    }
    else
    {
        return NULL;
    }
}

/*
 * 当外部异步事件到达后，需要通过fd 和 nid两个因子来找到对应的net context
 * 以下接口保证能够通过异步回调数据来找到正确的net context
 * */
EasyNet* NetHandlerMgr::GetHandlerByAsyncIds(uint32_t fd, uint64_t nid)
{
    HandlerPool::iterator it = handler_pool_.find(fd);
    if (it != handler_pool_.end() && it->second != NULL &&
        nid == (it->second)->nid())
    {
        return it->second;
    }
    else
    {
        return NULL;
    }
}

}
