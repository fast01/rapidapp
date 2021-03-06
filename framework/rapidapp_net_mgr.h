#ifndef RAPIDAPP_NET_MGR_H_
#define RAPIDAPP_NET_MGR_H_

#include "event2/event.h"
#include "event2/bufferevent.h"
#include "rapidapp_easy_net.h"
#include <cstdio>
#include <tr1/unordered_map>

namespace rapidapp {

class IWalkEach {
    public:
        IWalkEach(){}
        virtual ~IWalkEach(){}

    public:
        ///< return !0:erase 0:no erase
        virtual int DoSomething(EasyNet* net) = 0;
};

struct RapBuffer {
    char* buffer;
    size_t size;
};

typedef std::tr1::unordered_map<evutil_socket_t, EasyNet*> HandlerPool;

class AppFrameWork;
class NetHandlerMgr {
    public:
        NetHandlerMgr();
        ~NetHandlerMgr();

    public:
        int Init(size_t recv_buff_size);
        void CleanUp();

    public:
        EasyNet* AddHandlerByUri(const char* uri, int type,
                                 struct event_base* event_base);
        EasyNet* AddHandlerBySocket(evutil_socket_t sock_fd, int type,
                                    struct event_base* event_base);
        int AddHandlerToMap(EasyNet* easy_net_handler);

        int RemoveHandler(EasyNet* easy_net_handler);
        int RemoveHandlerByEvent(struct bufferevent* event);

        EasyNet* GetHandlerByEvent(struct bufferevent* event);
        int ChangeNetStateByEvent(struct bufferevent* event, enum NetState state);

        EasyNet* GetHandlerByAsyncIds(uint32_t fd, uint64_t nid);

    public:
        void WalkThrough(IWalkEach* act);

    private:
        HandlerPool handler_pool_;
        struct RapBuffer recv_buffer_;
        HandlerPool::iterator it_cursor_;

        friend class AppFrameWork;
};

}

#endif
