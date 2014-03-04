#include "LocalSocketSender.h"
#include "Utils.h"
#include "MessageStructDefinitions.h"

#define NETLINK_WIFU	29  // 29 wifu, 31 is test app


LocalSocketSender::LocalSocketSender() {
    init();
}

LocalSocketSender::~LocalSocketSender() {
perror("destructing LocalSocketSender");
    close(socket_);
}

ssize_t LocalSocketSender::send_to(struct sockaddr_un* destination, void* buffer, size_t length) {
	printf("LocalSocketSender::send_to I SHOULD NOT BE CALLED\n");
	int ret;
    ret = sendto(socket_, buffer, length, 0, (struct sockaddr*) destination, SUN_LEN(destination));
if(ret == -1){
	perror("Error LocalSocketSender::send_to");
}
return ret;

//	return sendmsg(socket_, buffer, 0);
}

void LocalSocketSender::init() {
	printf("LocalSocketSender::init()\n");
    socket_ = socket(AF_NETLINK, SOCK_RAW, NETLINK_WIFU);//socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (!socket_) {
        perror("Error Creating Socket");
        exit(-1);
    }

// Are these necessary for netlink??
/*
    socklen_t optval = UNIX_SOCKET_MAX_BUFFER_SIZE;
    int value = setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, &optval, sizeof (optval));
    if (value) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
*/

}
