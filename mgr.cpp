//
// Created by xianke on 2021/11/20.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <cstdio>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string>

#include <exception>
#include "log.hpp"
#include "mgr.h"

int mgr::m_epollfd = -1;

mgr::mgr(int epollfd, const host& srv): m_logical_srv(srv){
    m_epollfd = epollfd;
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, srv.m_hostname, &address.sin_addr);
    address.sin_port = htons(srv.m_port);
    logger.log(LOG_INFO, __FILE__, __LINE__, "logical srv info: (%s, %d)", srv.m_hostname, srv.m_port);

    for(int i=0;i<srv.m_connect;++i){
        sleep(1);
        int sockfd = conn2srv(address);
        if(sockfd<0){
            logger.log(LOG_ERR, __FILE__, __LINE__, "build connection %d failed", i);
        }
        else{
            logger.log(LOG_INFO, __FILE__, __LINE__, "build connection %d to server success", i);
            conn* tmp = nullptr;
            try{
                tmp = new conn;
            }catch(...){
                close(sockfd);
                continue;
            }
            tmp->init_srv(sockfd, address);
            m_conns[sockfd] = tmp;
        }
    }
}

// 资源回收利用
void mgr::recycle_conns(){
    if(m_freed.empty()) return;
    for(auto iter: m_freed){
        sleep(1);
        int srvfd=iter.first;
        conn* tmp=iter.second;
        srvfd = conn2srv(tmp->get_m_srv_address());
        if(srvfd<0) {
            logger.log(LOG_ERR, __FILE__, __LINE__, "%s", "fix connection failed");
        }
        else{
            logger.log(LOG_INFO, __FILE__, __LINE__, "%s", "fix connection success");
            tmp->init_srv(srvfd, tmp->get_m_srv_address());
            m_conns[srvfd] = tmp;
        }
        m_freed.clear();
    }
}

RET_CODE mgr::process(int fd, OP_TYPE type){
    conn* connection = m_used[fd];
    if(!connection) return NOTHING;
    RET_CODE res;
    if(connection->get_srvfd()==fd) {
        int srvfd = connection->get_srvfd();
        switch (type) {
            case READ:{
                res = connection->read_clt();
                switch (res) {
                    case OK:
                        logger.log(LOG_DEBUG, __FILE__, __LINE__, "content read from client: %s", connection->get_cltfd());
                    case BUFFER_FULL: {
                        modfd(m_epollfd, srvfd, EPOLLOUT);
                        break;
                    }
                    case IOERR:
                    case CLOSED: {
                        free_conn(connection);
                        return CLOSED;
                    }
                    default:
                        break;
                }
                if (connection->get_m_srv_status()) {
                    free_conn(connection);
                    return CLOSED;
                }
                break;
            }
            case WRITE: {
                res = connection->write_clt();
                switch (res) {
                    case TRY_AGAIN: {
                        modfd(m_epollfd, fd, EPOLLOUT);
                        break;
                    }
                    case BUFFER_EMPTY: {
                        modfd(m_epollfd, srvfd, EPOLLIN);
                        modfd(m_epollfd, fd, EPOLLIN);
                        break;
                    }
                    case IOERR:
                    case CLOSED: {
                        free_conn(connection);
                        return CLOSED;
                    }
                    default:
                        break;
                }

                if (connection->get_m_srv_status()) {
                    free_conn(connection);
                    return CLOSED;
                }
                break;
            }
            default: {
                logger.log(LOG_ERR, __FILE__, __LINE__, "%s", "other operation not support yet");
                break;
            }
        }
    }else if(connection->get_srvfd()==fd){
        int cltfd = connection->get_cltfd();
        switch(type){
            case READ:{
                res = connection->read_srv();
                switch(res){
                    case OK:
                        logger.log(LOG_DEBUG, __FILE__, __LINE__, "content read from server: %s", connection->get_srvfd());
                    case BUFFER_FULL:{
                            modfd(m_epollfd, cltfd, EPOLLOUT);
                            break;
                    }
                    case IOERR:
                    case CLOSED:{
                            modfd(m_epollfd, cltfd, EPOLLOUT);
                            connection->close_m_srv_status();
                            break;
                    }
                    default: break;
                }
                break;
            }
            case WRITE:{
                res = connection->write_srv();
                switch(res){
                    case TRY_AGAIN:{
                        modfd(m_epollfd, fd, EPOLLOUT);
                        break;
                    }
                    case BUFFER_EMPTY:{
                        modfd(m_epollfd, cltfd, EPOLLIN);
                        modfd(m_epollfd, fd, EPOLLIN);
                        break;
                    }
                    case IOERR:
                    case CLOSED:{
                        modfd(m_epollfd, cltfd, EPOLLOUT);
                        connection->close_m_srv_status();
                        break;
                    }
                    default: break;
                }
                break;
            }
            default:{
                logger.log(LOG_ERR, __FILE__, __LINE__, "%S", "other operation not support yet");
                break;
            }
        }
    }else return NOTHING;
    return OK;
}

// 每个进程是不知道是否自己是最闲的，这件事需要交给父进程才会清楚，因此，父进程了解情况后，
// 会通知一个子进程来进行调用pick_conn函数来处理连接
// 但是子进程接收连接的代码不在这里，是每个pool子进程来实现的
// 相当于说每个manager管理着一些连接资源，管理者缓冲区和逻辑服务器之间的连接+读写缓冲
// pool通过包含manager来管理和服务器之间的连接，同时pool自己负责accept连接并且把socket描述符交给
// 自己的manager
conn* mgr::pick_conn(int cltfd){
    if(m_conns.empty()){
        logger.log(LOG_ERR, __FILE__, __LINE__, "%s", "not enough srv connections to server");
        return nullptr;
    }
    map<int, conn*>::iterator iter = m_conns.begin();
    int srvfd = iter->first;
    conn* tmp = iter->second;
    if(!tmp){
        logger.log(LOG_ERR, __FILE__, __LINE__, "%s", "empty server connection object");
        return nullptr;
    }
    m_conns.erase(iter);
    m_used[srvfd] = tmp;
    m_used[cltfd] = tmp;
    add_read_fd(m_epollfd, cltfd);
    add_read_fd(m_epollfd, srvfd);
    logger.log(LOG_INFO, __FILE__, __LINE__, "bind client sock %d with server sock %d", cltfd, srvfd);
    return tmp;
}

void mgr::free_conn(conn* connection){
    int cltfd = connection->get_cltfd();
    int srvfd = connection->get_srvfd();
    closefd(m_epollfd, cltfd);
    closefd(m_epollfd, srvfd);
    m_used.erase(cltfd);
    m_used.erase(srvfd);
    connection->reset();
    m_freed[srvfd] = connection;
    logger.log(LOG_ERR, __FILE__, __LINE__, "the connection is freed %d", 1);
}

int mgr::conn2srv(const sockaddr_in& address) const{
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if(sockfd<0) return -1;
    if(connect(sockfd, (struct sockaddr*)&address, sizeof(address))!=0){
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int mgr::get_used_conn_cnt() const{
    return m_used.size();
}


