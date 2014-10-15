#include "tsocket_util.h"

namespace tsocket_util {

TcpSocketUtil::TcpSocketUtil() : socket_state_(NET_INITED), url_(), fd_(-1),
    last_request_timestamp_(0), last_response_timestamp_(0),
    tcp_pkg_parser_imp_(NULL)
{
}

TcpSocketUtil::~TcpSocketUtil()
{
    Fini();
}

int TcpSocketUtil::Init(ITcpPkgParser* tcp_pkg_parser_imp, size_t buff_size)
{
    if (NULL == tcp_pkg_parser_imp || 0 == buff_size)
    {
        return INVLD_ARGUMENT;
    }

    tcp_pkg_parser_imp_ = tcp_pkg_parser_imp;
    if (send_buf_.Malloc(buff_size))
    {
        send_buf_.buff_size = buff_size;
    }
    else
    {
        return MALLOC_FAILED;
    }

    if (recv_buf_.Malloc(buff_size))
    {
        recv_buf_.buff_size = buff_size;
    }
    else
    {
        send_buf_.Free();
        return MALLOC_FAILED;
    }

    return NORMAL;
}

void TcpSocketUtil::Fini()
{
    recv_buf_.Free();
    send_buf_.Free();

    Close();
}

void TcpSocketUtil::Reset()
{
    last_request_timestamp_ = 0;
    last_response_timestamp_ = 0;
    send_buf_.begin = 0;
    send_buf_.end = 0;
    recv_buf_.begin = 0;
    recv_buf_.end = 0;
    socket_state_ = NET_INITED;
}

// �ڲ�˽�к���������ֵֻ��0��-1֮��
int TcpSocketUtil::Url2InetAddr(const char* url, TNETADDR& tnet_addr)
{
    if (NULL == url)
    {
        socket_state_ = NET_ASSERT_FAIL;
        return ASSERT_FAILED;
    }

    TNETOPT tnet_opt;
    if (-1 == tnet_get_opt(&tnet_opt, url))
    {
        return -1;
    }

    const TNETPROTODEF* proto = tnet_find_proto(tnet_opt.szProto);
    if (NULL == proto)
    {
        return -1;
    }

    //������ַ
    if (proto->iID != TNET_ID_TCP && proto->iID != TNET_ID_UDP)
    {
        return -1;
    }

    if (0 != tnet_str2inet(tnet_opt.szAddr, &tnet_addr.stIn))
    {
        return -1;
    }

    if (0 == tnet_addr.stIn.sin_port)
    {
        return -1;
    }

    return 0;
}

int TcpSocketUtil::ConnectNonBlock(const char* url)
{
    if (NULL == url)
    {
        return INVLD_ARGUMENT;
    }

    TNETADDR tnet_addr;
    int ret = Url2InetAddr(url, tnet_addr);
    if (ret != 0)
    {
        return INVLD_URL;
    }

    SOCKET fd = tnet_open(url);
    if (fd < 0)
    {
        return CONNECT_FAILED;
    }

    tnet_set_nonblock(fd, 1);

    ret = tsocket_start_connect(fd, (struct sockaddr*)&tnet_addr.stIn,
                                    sizeof(tnet_addr.stIn));
    if (ret < 0)
    {
        tnet_close(fd);
        return CONNECT_FAILED;
    }

    fd_ = fd;
    socket_state_ = NET_CONNECTING;
    url_ = url;
    return NORMAL;
}

int TcpSocketUtil::CheckNonBlock()
{
    if (fd_ < 0)
    {
        socket_state_ = NET_ASSERT_FAIL;
        return ASSERT_FAILED;
    }

    int ret = tsocket_check_connect(fd_, 0);
    if (0 == ret)
    {
        // TCP 3 handshake success
        socket_state_ = NET_CONNECTED;
    }
    else if (ret < 0)
    {
        Close();
        return CONNECT_FAILED;
    }

    return NORMAL;
}

int TcpSocketUtil::Connect(const char* url, size_t timeout)
{
    if (NULL == url)
    {
        return INVLD_ARGUMENT;
    }

    fd_ = tnet_connect(url, timeout);
    if (fd_ < 0)
    {
        return CONNECT_FAILED;
    }

    socket_state_ = NET_CONNECTED;
    url_ = url;
    return NORMAL;
}

void TcpSocketUtil::Close()
{
    if (fd_ >= 0)
    {
        tnet_close(fd_);
        fd_ = -1;
    }

    socket_state_ = NET_INITED;
}

int TcpSocketUtil::Send()
{
    if (NET_ASSERT_FAIL == socket_state_)
    {
        return ASSERT_FAILED;
    }
    if (NET_SEND_FAILED == socket_state_)
    {
        return NORMAL;
    }
    else if (socket_state_ != NET_CONNECTED &&
             socket_state_ != NET_RECV_FAILED &&
             socket_state_ != NET_PEER_CLOSED)
    {
        return NOT_CONNECTED;
    }

    if (send_buf_.Datalen() < 0)
    {
        socket_state_ = NET_ASSERT_FAIL;
        return ASSERT_FAILED;
    }
    else if (0 == send_buf_.Datalen())
    {
        return NORMAL;
    }

    int len = send(fd_, send_buf_.Ptr() + send_buf_.begin,
                   send_buf_.Datalen(), 0);
    if (len != (int)send_buf_.Datalen())
    {
        if (len < 0)
        {
#if defined(_WIN32) || defined(_WIN64)
            if(WSAEWOULDBLOCK == WSAGetLastError())
#else
            if (SOCKETEWOULDBLOCK == errno || SOCKETEAGAIN == errno)
#endif
            {
                // socket send buff is full
                return SEND_UNCOMPLETED;
            }
            else
            {
                socket_state_ = NET_SEND_FAILED;
                return SEND_FAILED;
            }
        }
        else if (0 == len)
        {
            // sent data length is 0
            return NORMAL;
        }
        else if (len < (int)send_buf_.Datalen())
        {
            // send incompleted
            send_buf_.begin += len;
            send_buf_.Recycle_ATOMIC();

            return SEND_UNCOMPLETED;
        }
    }

    // send has been over
    send_buf_.begin += len;
    send_buf_.Recycle_ATOMIC();

    return NORMAL;
}

int TcpSocketUtil::Recv()
{
    if (NET_ASSERT_FAIL == socket_state_)
    {
        return ASSERT_FAILED;
    }
    else if (NET_RECV_FAILED == socket_state_ || NET_PEER_CLOSED == socket_state_)
    {
        return NORMAL;
    }
    else if (socket_state_ != NET_CONNECTED && socket_state_ != NET_SEND_FAILED)
    {
        return NOT_CONNECTED;
    }

    if (recv_buf_.Avaiable() < 0)
    {
        socket_state_ = NET_ASSERT_FAIL;
        return ASSERT_FAILED;
    }
    else if (0 == recv_buf_.Avaiable())
    {
        recv_buf_.Recycle_ATOMIC();
        if (0 == recv_buf_.Avaiable())
        {
            return NORMAL;
        }
    }

#ifdef TSOCKET_UTIL_ATOMIC
    if (recv_buf_.Locked())
    {
        return RECV_BUFF_BUSY;
    }

    ++recv_buf_.atomic_in;
#endif

    int ret = NORMAL;
    int len = recv(fd_, recv_buf_.Ptr() + recv_buf_.end,
                   recv_buf_.Avaiable(), 0);
    if (len < 0)
    {
#if defined(_WIN32) || defined(_WIN64)
        if(WSAEWOULDBLOCK == WSAGetLastError())
#else
        if (SOCKETEWOULDBLOCK == errno || SOCKETEAGAIN == errno)
#endif
        {
            ret = NORMAL;
        }
        else
        {
            socket_state_ = NET_RECV_FAILED;
            ret =  RECV_FAILED;
        }
    }
    else if (0 == len)
    {
        // close connection
        socket_state_ = NET_PEER_CLOSED;
        ret = PEER_CLOSED;
    }
    else
    {
        recv_buf_.end += len;
        ret = NORMAL;
    }

#ifdef TSOCKET_UTIL_ATOMIC
    ++recv_buf_.atomic_out;
#endif
    return ret;
}

int TcpSocketUtil::PushToSendQ(const char* buf, size_t buf_len)
{
    if ((int)buf_len > send_buf_.Avaiable())
    {
        return SEND_BUFF_FULL;
    }

#ifdef TSOCKET_UTIL_ATOMIC
    if (send_buf_.Locked())
    {
        return SEND_BUFF_BUSY;
    }

    ++send_buf_.atomic_in;
#endif

    memcpy(send_buf_.Ptr() + send_buf_.end, buf, buf_len);
    send_buf_.end += buf_len;

#ifdef TSOCKET_UTIL_ATOMIC
    ++send_buf_.atomic_out;
#endif

    last_request_timestamp_ = time(NULL);
    return NORMAL;
}

bool TcpSocketUtil::HasNewPkg()
{
    if (NULL == tcp_pkg_parser_imp_)
    {
        socket_state_ = NET_ASSERT_FAIL;
        return false;
    }

    int len = tcp_pkg_parser_imp_->GetPkgLen(recv_buf_.Ptr() + recv_buf_.begin,
                                        recv_buf_.Datalen());
    if (len > 0 && len <= recv_buf_.Datalen())
    {
        return true;
    }

    return false;

}

int TcpSocketUtil::PeekFromRecvQ(const char** buf, int* buf_len)
{
    if (NULL == tcp_pkg_parser_imp_)
    {
        socket_state_ = NET_ASSERT_FAIL;
        return ASSERT_FAILED;
    }

    if (NULL == buf || NULL == buf_len)
    {
        return INVLD_ARGUMENT;
    }

    *buf = NULL;
    *buf_len = 0;

    int len = tcp_pkg_parser_imp_->GetPkgLen(recv_buf_.Ptr() + recv_buf_.begin,
                                        recv_buf_.Datalen());
    if (len > 0 && len <= recv_buf_.Datalen())
    {
        *buf = recv_buf_.Ptr() + recv_buf_.begin;
        *buf_len = len;
        last_response_timestamp_ = time(NULL);
    }

    return NORMAL;
}

int TcpSocketUtil::PopFromRecvQ()
{
    if (NULL == tcp_pkg_parser_imp_)
    {
        socket_state_ = NET_ASSERT_FAIL;
        return ASSERT_FAILED;
    }

    int len = tcp_pkg_parser_imp_->GetPkgLen(recv_buf_.Ptr() + recv_buf_.begin,
                                        recv_buf_.Datalen());
    if (len <= 0 || len > recv_buf_.Datalen())
    {
        recv_buf_.Recycle_ATOMIC();
        return NORMAL;
    }

    recv_buf_.begin += len;
    return NORMAL;
}

}
