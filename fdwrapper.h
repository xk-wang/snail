//
// Created by xianke on 2021/11/20.
//

#ifndef SPRINGSNAIL_FDWRAPPER_H
#define SPRINGSNAIL_FDWRAPPER_H

enum RET_CODE {OK = 0, NOTHING = 1, IOERR = -1, CLOSED = -2, BUFFER_FULL = -3, BUFFER_EMPTY = -4, TRY_AGAIN};
enum OP_TYPE { READ=0, WRITE, ERROR};
int setnonblocking(int fd);
void add_read_fd(int epollfd, int fd);
void add_write_fd(int epollfd, int fd);
void removefd(int epollfd, int fd);
void closefd(int epollfd, int fd);
void modfd(int epollfd, int fd, int ev);

#endif //SPRINGSNAIL_FDWRAPPER_H
