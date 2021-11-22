//
// Created by xianke on 2021/11/20.
//
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <vector>
#include <iostream>
#include <fstream>
#include "json.hpp"

#include "log.h"
#include "conn.h"
#include "mgr.h"
#include "processpool.h"

using std::vector;
using std::string;
using nlohmann::json;

static const char* version = "1.0";

static void usage(const char* prog){
    log(LOG_INFO, __FILE__, __LINE__, "usage: %s [-h] [-v] [-f config_file]", prog);
}

void parse_args(int argc, char* argv[], char* cfg_file){
    int option;
    while((option=getopt(argc, argv, "f:xvh"))!=-1){
        switch(option){
            case 'x':{
                set_loglevel(LOG_DEBUG);
                break;
            }
            case 'v':{
                log(LOG_INFO, __FILE__, __LINE__, "%s %s", argv[0], version);
                return;
            }
            case 'h':{
                usage(basename(argv[0]));
                return;
            }
            case 'f':{
                memcpy(cfg_file, optarg, strlen(optarg));
                break;
            }
            case '?':{
                log(LOG_ERR, __FILE__, __LINE__, "un-recogonized option %c", option);
                usage(basename(argv[0]));
                return;
            }
        }
    }

    if(strlen(cfg_file)==0){
        log(LOG_ERR, __FILE__, __LINE__, "%s", "please specify the config file");
        return;
    }
}

void parse_xml(char *cfg_file, vector<host>& balance_srv, vector<host>& logical_srv){
    int cfg_fd = open(cfg_file, O_RDONLY);
    if(!cfg_fd){
        log(LOG_ERR, __FILE__, __LINE__, "read config file meet error: %s", strerror(errno));
        return;
    }
    struct stat ret_stat;
    if(fstat(cfg_fd, &ret_stat)<0){
        log(LOG_ERR, __FILE__, __LINE__, "read config file meet error: %s", strerror(errno));
        return;
    }

    char* buf=new char[ret_stat.st_size+1];
    memset(buf, '\0', ret_stat.st_size+1);
    ssize_t read_sz = read(cfg_fd, buf, ret_stat.st_size);
    if(read_sz<0){
        log(LOG_ERR, __FILE__, __LINE__, "read config file meet error: %s", strerror(errno));
        return;
    }

    host tmp_host;
    memset(tmp_host.m_hostname, '\0', 1024);
    char* tmp_hostname, *tmp_port, *tmp_connect, *tmp, *tmp2, *tmp3, *tmp4;
    tmp=buf;
    tmp2=tmp3=tmp4=NULL;
    bool opentag = false;
    //自己来解析这些格式太傻了
    while(tmp2=strpbrk(tmp, "\n")){
        *tmp2++ = '\0';
        if(strstr(tmp, "<logical_host>")){
            if(opentag){
                log(LOG_ERR, __FILE__, __LINE__, "%s", "parse config file failed");
                return;
            }
            opentag = true;
        }else if(strstr(tmp, "</logical_host>")){
            if(!opentag){
                log(LOG_ERR, __FILE__, __LINE__, "%s", "parse config file failed");
                return;
            }
            logical_srv.push_back(tmp_host);
            memset(tmp_host.m_hostname, '\0', 1024);
            opentag=false;
        }else if(tmp3==strstr(tmp, "<name>")){
            tmp_hostname = tmp3+6;
            tmp4 = strstr(tmp_hostname, "</name>");
            if(tmp4){
                log(LOG_ERR, __FILE__, __LINE__, "%s", "parse config file failed");
                return;
            }
            *tmp4 = '\0';
            memcpy(tmp_host.m_hostname, tmp_hostname, strlen(tmp_hostname));
        }else if(tmp3=strstr(tmp, "<port>")){
            tmp_port = tmp3+6;
            tmp4 = strstr(tmp_port, "</port>");
            if(!tmp4){
                log(LOG_ERR, __FILE__, __LINE__, "%s", "parse config file failed");
                return;
            }
            *tmp4 = '\0';
            tmp_host.m_port = atoi(tmp_port);
        }else if(tmp3=strstr(tmp, "<conns>")){
            tmp_connect = tmp+7;
            tmp4 = strstr(tmp_connect, "</conns>");
            if(!tmp4){
                log(LOG_ERR, __FILE__, __LINE__, "%s", "parse config file failed");
                return;
            }
            *tmp4 = '\0';
            tmp_host.m_connect = atoi(tmp_connect);
        }else if(tmp3=strstr(tmp, "Listen")){
            tmp_hostname = tmp3+6;
            tmp4 = strstr(tmp_hostname, ":");
            if(!tmp4){
                log(LOG_ERR, __FILE__, __LINE__, "%s", "parse config file failed");
                return;
            }
            *tmp4++='\0';
            tmp_host.m_port = atoi(tmp4);
            memcpy(tmp_host.m_hostname, tmp3, strlen(tmp3));
            balance_srv.push_back(tmp_host);
            memset(tmp_host.m_hostname, '\0', 1024);
        }
        tmp = tmp2;
    }

    if(balance_srv.size()==0||logical_srv.size()==0){
        log(LOG_ERR, __FILE__, __LINE__, "%s", "parse config file failed");
        return;
    }
    close(cfg_fd);
}

void parse_json(char* cfg_file, vector<host>&balance_srv, vector<host>&logical_srv){
    std::ifstream file(cfg_file);
    json data;
    file >> data;

    host tmp_host;
    string hostname = data["listen"]["hostname"];

    memset(tmp_host.m_hostname, '\0', 1024);
    memcpy(tmp_host.m_hostname, hostname.c_str(), hostname.size());
    tmp_host.m_port = data["listen"]["port"];
    tmp_host.m_connect = data["listen"]["connect"];
    balance_srv.push_back(tmp_host);

    for(json tmp: data["logical_host"]){
        hostname = tmp["hostname"];
        memset(tmp_host.m_hostname, '\0', 1024);
        memcpy(tmp_host.m_hostname, hostname.c_str(), hostname.size());
        tmp_host.m_port = tmp["port"];
        tmp_host.m_connect = tmp["connect"];
        logical_srv.push_back(tmp_host);
    }
//    std::cout<<logical_srv.size()<<" "<<balance_srv[0].m_hostname<<std::endl;
}

int main(int argc, char* argv[]){
    char cfg_file[1024];
    memset(cfg_file, '\0', 100);
    parse_args(argc, argv, cfg_file);

    vector<host>balance_srv;
    vector<host>logical_srv;
//    parse_xml(cfg_file, balance_srv, logical_srv);
    parse_json(cfg_file, balance_srv, logical_srv);


    const char* ip = balance_srv[0].m_hostname;
    int port = balance_srv[0].m_port;
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd>=0);

    int ret=0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int opt = 1;
    setsockopt( listenfd, SOL_SOCKET,SO_REUSEADDR, (const void *)&opt, sizeof(opt) );

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret!=-1);
    ret = listen(listenfd, 5);
    assert(ret!=-1);
    processpool<conn, host, mgr>*pool = processpool<conn, host, mgr>::create(listenfd, logical_srv.size());
    if(pool){
        pool->run(logical_srv);
        delete pool;
    }
    close(listenfd);
    return 0;
}