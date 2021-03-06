/* 
 * File:   includes.h
 * Author: rbuck
 *
 * Created on October 22, 2010, 11:05 AM
 */

#ifndef _INCLUDES_H
#define	_INCLUDES_H

#include <typeinfo>
#include <limits.h>
#include <string>
#include <map>
#include "GarbageCollector.h"

#define POOL_INITIAL_SIZE 10


typedef std::basic_string<char, std::char_traits<char>, gc_allocator<char> > gcstring;
typedef std::map<gcstring, gcstring, std::less<gcstring>, gc_allocator<std::pair<gcstring, gcstring> > > gcstring_map;

#define type_name(X) typeid(X).name()
#define SIG_ENQUEUE_EVENT 50
#define SIG_CANCEL_EVENT 51
#define NANOSECONDS_IN_SECONDS 1000000000
#define MICROSECONDS_IN_SECONDS 1000000
#define PAYLOAD_SIZE 1500
#define TWO_MSL 3 // seconds
#define BUFFER_SIZE PAYLOAD_SIZE * 2
#define UNIX_SOCKET_MESSAGE_LENGTH 4096

// message defines
// method names
#define WIFU_SOCKET_NAME "wifu_socket"
#define WIFU_BIND_NAME "wifu_bind"
#define WIFU_LISTEN_NAME "wifu_listen"
#define WIFU_ACCEPT_NAME "wifu_accept"
#define WIFU_SEND_NAME "wifu_send"
#define WIFU_SENDTO_NAME "wifu_sendto"
#define WIFU_RECV_NAME "wifu_recv"
#define WIFU_RECVFROM_NAME "wifu_recvfrom"
#define WIFU_CONNECT_NAME "wifu_connect"
#define WIFU_GETSOCKOPT_NAME "wifu_getsockopt"
#define WIFU_SETSOCKOPT_NAME "wifu_setsockopt"
#define WIFU_CLOSE_NAME "wifu_close"
#define WIFU_PRECLOSE_NAME "wifu_preclose"

#define WIFU_SOCKET 1
#define WIFU_BIND 2
#define WIFU_LISTEN 3
#define WIFU_ACCEPT 4
#define WIFU_SEND 5
#define WIFU_SENDTO 6
#define WIFU_RECV 7
#define WIFU_RECVFROM 8
#define WIFU_CONNECT 9
#define WIFU_GETSOCKOPT 10
#define WIFU_SETSOCKOPT 11
#define WIFU_CLOSE 12
#define WIFU_PRECLOSE 13

// variable names
#define SOCKET_STRING "socket"
#define FILE_STRING "file"
#define NAME_STRING "name"
#define RETURN_VALUE_STRING "return_value"
#define DOMAIN_STRING "domain"
#define TYPE_STRING "type"
#define PROTOCOL_STRING "protocol"
#define ADDRESS_STRING "address"
#define PORT_STRING "port"
#define LENGTH_STRING "length"
#define N_STRING "n"
#define BUFFER_STRING "buffer"
#define FLAGS_STRING "flags"
#define OPTION_NAME_STRING "option_name"
#define OPTION_VALUE_STRING "option_value"
#define LEVEL_STRING "level"
#define ERRNO "errno"

// Wifu defined socket options
#define TCP_TIMESTAMP 20


// WIFU_PORT is created from the phone number representaion of W-I-F-U
// This is deprecated as of Raw Sockets
#define WIFU_PORT 9438

// must fit in 8 bits
#define SIMPLE_TCP 206

#define TCP_TAHOE 6
#define UDP 17
//#define TCP_TAHOE 207
//#define UDP 217

#define TCP_AP 211

// TODO: figure out path MTU on the fly.
// ethernet MTU
#define MTU 1500

// localhost MTU
//#define MTU 16436

#define MAX_TTL 65

// TODO: switch these lines so we can have a bigger/smaller send/receive buffer sizes.
// http://linux.die.net/man/7/tcp
// 131071 is the rmem_max and wmem_max values as defined in /proc/sys/net/core/[r|w]mem_max file as of July 26, 2011 -- RB
// I checked  these values on both the ilab and mesh machines.
#define UNIX_SOCKET_MAX_BUFFER_SIZE 131071

// The TCP send and receive buffers are limited to the values in /proc/sys/net/ipv4/tcp_rmem
// However, we need to be a little smart here.  We need to look into this a bit more.
//#define TCP_SOCKET_MAX_BUFFER_SIZE 4194304
#define TCP_SOCKET_MAX_BUFFER_SIZE UNIX_SOCKET_MAX_BUFFER_SIZE
//#define MAX_BUFFER_SIZE 2

// Clock defines
// RTO values are in seconds
#define INITIAL_RTO 3.0
#define MIN_RTO 1.0



// in seconds
#define INITIAL_PROBE_TIMEOUT_DURATION 120

// RFC 2988
#define ALPHA 0.125
#define BETA 0.25
// arbitrarily choosing a 1 ms granual timer
#define G 0.001
#define K 4


// TODO: if we want it any bigger we need to employ the window scale option
#define MAX_TCP_RECEIVE_WINDOW_SIZE USHRT_MAX

#define SYN_BYTE gcstring(1, 22)
#define FIN_BYTE gcstring(1, 23)

typedef const char * event_name;

//Specific to UDP
#define UDP_HEADER_LENGTH_BYTES 8

#define FAKE_ETHERNET_HEADER_SIZE 14

/*
 * Represents various priorities.
 * The lower the number, the higher the priority.
 * I've left some space between these priorities so others can be added
 */
enum Priority {
    HIGH = 0,
    MEDIUM = 1000,
    LOW = 2000
};

// delayed ack defines
#define TCP_DELAYEDACK 209

// Delayed ACK timeout value in seconds
//#define INITAL_DELAY_TIMEOUT_INTERVAL 0.0001 //if they meant microseconds
#define INITAL_DELAY_TIMEOUT_INTERVAL 0.1 //what they said

//Specific to Delayed ACK
#define DEFAULT_DELAY_COUNT 2

// delayed ack + adaptive pacing defines
#define TCP_AP_DELAYEDACK 213

#endif	/* _INCLUDES_H */

