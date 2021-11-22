//
// Created by xianke on 2021/11/20.
//

#ifndef SPRINGSNAIL_MGR_H
#define SPRINGSNAIL_MGR_H

#include <map>
#include <arpa/inet.h>
#include "conn.h"

using std::map;
class host{
public:
    char m_hostname[1024];
    int m_port;
    int m_connect;
};

class mgr{
public:
    mgr(int epollfd, const host& srv);
    ~mgr();
    int conn2srv(const sockaddr_in& address) const;
    conn* pick_conn(int sockfd);
    void free_conn(conn* connection);
    int get_used_conn_cnt() const;
    void recycle_conns();
    RET_CODE process(int fd, OP_TYPE type);

private:
    static int m_epollfd;
    map<int, conn*> m_conns;
    map<int, conn*> m_used;
    map<int, conn*> m_freed;
    host m_logical_srv;
    const Logger& logger = Logger::create_logger();
};

#endif //SPRINGSNAIL_MGR_H
