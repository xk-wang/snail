//
// Created by xianke on 2021/11/20.
//

#ifndef SPRINGSNAIL_CONN_H
#define SPRINGSNAIL_CONN_H

#include <arpa/inet.h>
#include "fdwrapper.h"
#include "log.hpp"

class conn{
public:
    conn();
    ~conn();
    void init_clt(int sockfd, const sockaddr_in& client_addr);
    void init_srv(int sockfd, const sockaddr_in& server_addr);
    void reset();
    RET_CODE read_clt();
    RET_CODE write_clt();
    RET_CODE read_srv();
    RET_CODE write_srv();
    int get_cltfd() const { return m_cltfd; }
    int get_srvfd() const { return m_srvfd; }
    bool get_m_srv_status() const { return m_srv_closed; }
    void close_m_srv_status() { m_srv_closed = true; }
    const sockaddr_in get_m_srv_address() const { return m_srv_address; }

private:
    static const int BUF_SIZE = 2048;
    char* m_clt_buf;
    int m_clt_read_idx;
    int m_clt_write_idx;
    sockaddr_in m_clt_address;
    int m_cltfd;

    char* m_srv_buf;
    int m_srv_read_idx;
    int m_srv_write_idx;
    sockaddr_in m_srv_address;
    int m_srvfd;

    bool m_srv_closed;

    const Logger& logger = Logger::create_logger();
};

#endif //SPRINGSNAIL_CONN_H
