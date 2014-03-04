/* 
 * File:   MessageStructDefinitionsCStyleForWedge.h
 * Author: rbuck
 *
 * Created on August 8, 2011, 10:28 AM
 */

#ifndef MESSAGESTRUCTDEFINITIONSCSTYLEFORWEDGE_H
#define	MESSAGESTRUCTDEFINITIONSCSTYLEFORWEDGE_H

//#include <sys/un.h>
//#include <sys/socket.h>
#include <linux/un.h>


/*struct sockaddr_un
{
        sa_family_t sun_family;      // AF_UNIX 
        char        sun_path[108];   // pathname 
};*/

// find and replaces socklen_t with int....didn't compile.
// all c++ inheritance replaced with flat.

struct GenericMessage {
    u_int32_t message_type;
    u_int32_t length;
    u_int32_t fd;
};

struct FrontEndMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;
};

struct SocketMessage {
    u_int32_t message_type;
    u_int32_t length;
    u_int32_t fd;

    struct sockaddr_un source;

    int domain;
    int type;
    int protocol;
};

struct AddressMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    struct sockaddr_in addr;
    int len;
};

struct BindMessage  {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    struct sockaddr_in addr;
    int len;
    
};

struct SockOptMessage{
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    int level;
    int optname;
    int optlen;
};

struct GetSockOptMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    int level;
    int optname;
    int optlen;

};

struct SetSockOptMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    int level;
    int optname;
    int optlen;

    // Data of the optval goes after the header.
};

struct ListenMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    int n;
};

struct AcceptMessage  {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    struct sockaddr_in addr;
    int len;

    int secondfd;

    // Has the same parameters as Bind
};

struct DataMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    struct sockaddr_in addr;
    int len;

    size_t buffer_length;
    int flags;
};
// 160.... 4 4 4 ? ? 4 4 4 = 160 - 24 = 136 = 118.
struct SendToMessage  {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    struct sockaddr_in addr;
    int len;

    size_t buffer_length;
    int flags;

    // Buffer data goes at the end
};

struct RecvFromMessage  {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    struct sockaddr_in addr;
    int len;

    size_t buffer_length;
    int flags;

    // same parameters as sendto, except we don't pass in buffer
};

struct ConnectMessage  {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    struct sockaddr_in addr;
    int len;

    // same parameters as bind
};

struct CloseMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    // no extra params needed
};

// Response Messages

struct GenericResponseMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;
};

struct SocketResponseMessag {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;

    // return value is the new socket
};

struct BindResponseMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;

    // just need return value and errno
};

struct GetSockOptResponseMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;

    int optlen;
    // optval goes after this header
};

struct SetSockOptResponseMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;

    // just need return value (possibly errno?)
};

struct ListenResponseMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;

    // just need return value and errno
};

struct AddressResponseMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;

    struct sockaddr_in addr;
    int addr_len;
};

struct AcceptResponseMessage  {

    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;

    struct sockaddr_in addr;
    int addr_len;
    
};

struct SendToResponseMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;

    // just need return value (probably need errno too...)
};

struct RecvFromResponseMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;

    struct sockaddr_in addr;
    int addr_len;

    // size of buffer returned is in the return value
    // buffer of data goes after this header
};

struct ConnectResponseMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;

    // just need return value (probably need errno too...)
};

struct CloseResponseMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    int return_value;
    // to set errno if needed
    int error;

    // just need return value (probably need errno too...)
};


#endif	/* MESSAGESTRUCTDEFINITIONSCSTYLEFORWEDGE_H */

