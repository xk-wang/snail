//
// Created by xianke on 2021/11/20.
//
#ifndef SPRINGSNAIL_LOG_H
#define SPRINGSNAIL_LOG_H

#include<syslog.h>
#include<cstdarg>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>
#include <cstdio>

using std::string;
using std::vector;

class Logger{
private:
    Logger(int log_level=LOG_DEBUG, int buffer_size=4096): level(log_level), LOG_BUFFER_SIZE(buffer_size){};
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
public:
    static Logger& create_logger(){
        static Logger instance;
        return instance;
    }
    ~Logger(){
    }
    void log(int log_level, const char* file_name, int line_num, const char* format, ...) const{
        if(log_level>level) return;
        time_t tmp = time(NULL);
        struct tm* cur_time = localtime(&tmp);
        if(!cur_time) return;

        char* arg_buffer = new char[LOG_BUFFER_SIZE]; //智能指针只能用于单个对象，不能用于对象数组

        memset( arg_buffer, '\0', LOG_BUFFER_SIZE );
        strftime( arg_buffer, LOG_BUFFER_SIZE - 1, "[ %x %X ] ", cur_time );
        printf( "%s", arg_buffer );
        printf( "%s:%04d ", file_name, line_num );
        printf( "%s ", loglevels[ log_level - LOG_EMERG ].c_str());

        va_list arg_list;
        va_start(arg_list, format);
        memset(arg_buffer, '\0', LOG_BUFFER_SIZE);
        vsnprintf(arg_buffer, LOG_BUFFER_SIZE-1, format, arg_list);
        printf("%s\n", arg_buffer);
        fflush(stdout);
        va_end(arg_list);

        delete []arg_buffer;
    }
    void set_loglevel(int log_level){
        level = log_level;
    }
public:
    const vector<string> loglevels {
        "emerge!", "alert!", "critical!", "error!", "warn!", "notice:", "info:", "debug:"
    };
    int LOG_BUFFER_SIZE;
    int level;
};

#endif //SPRINGSNAIL_LOG_H
