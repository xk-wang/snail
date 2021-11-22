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
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <vector>
#include <iostream>
#include <fstream>
#include "json.hpp"
#include "cmdline.h"

#include "conn.h"
#include "mgr.h"
#include "processpool.hpp"
#include "log.hpp"

using std::vector;
using std::string;
using nlohmann::json;

static const char* version = "1.0";

// 后面考虑将easylogging用到我的代码中

string parse_args(int argc, char* argv[]) {
    cmdline::parser parser;
    parser.add<bool>("debug-level", 'x', "debug level", false, false);
    parser.add<string>("cfg_file", 'f', "the config file path", false, "");
    parser.parse_check(argc, argv);

    bool debug_level = parser.get<bool>("debug-level");
    string cfg_file = parser.get<string>("cfg_file");
    if(debug_level) Logger::create_logger().set_loglevel(LOG_DEBUG);
    return cfg_file;
}

void parse_json(string& cfg_file, vector<host>&balance_srv, vector<host>&logical_srv){
    std::ifstream file(cfg_file);
    json data;
    file >> data;
    host
     tmp_host;
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
}

int main(int argc, char* argv[]){

    std::cout<< "snail version: " << version << std::endl;
    string cfg_file=parse_args(argc, argv);
    vector<host>balance_srv;
    vector<host>logical_srv;
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