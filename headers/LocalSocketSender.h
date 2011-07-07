/* 
 * File:   LocalSocketSender.h
 * Author: rbuck
 *
 * Created on October 19, 2010, 2:43 PM
 */

#ifndef _LOCALSOCKETSENDER_H
#define	_LOCALSOCKETSENDER_H

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/un.h>
#include <map>
#include <assert.h>

#include "Semaphore.h"
#include "defines.h"

using namespace std;

class LocalSocketSender {
public:
    LocalSocketSender();
    virtual ~LocalSocketSender();

    ssize_t send_to(gcstring& socket_file, gcstring& message);
private:
    map<gcstring, struct sockaddr_un*> destinations_;
    struct sockaddr_un* create_socket(gcstring& socket_file);

    int socket_;

    void init();

};

#endif	/* _LOCALSOCKETSENDER_H */

