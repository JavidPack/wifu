#ifndef WIFU_WEDGE_H_
#define WIFU_WEDGE_H_


//commenting stops debug printout
#define DEBUG
#define ERROR

#ifdef DEBUG
#define PRINT_DEBUG(format, args...) printk("WIFU: DEBUG: %s, %d: "format"\n", __FUNCTION__, __LINE__, ##args);
#else
#define PRINT_DEBUG(format, args...)
#endif

#ifdef ERROR
#define PRINT_ERROR(format, args...) printk("WIFU: ERROR: %s, %d: "format"\n", __FUNCTION__, __LINE__, ##args);
#else
#define PRINT_ERROR(format, args...)
#endif

/*
* NETLINK_WIFU must match a corresponding constant in the userspace daemon program that is to talk to this module.
* NETLINK_ constants are normally defined in <linux/netlink.h> although adding a constant here would necessitate a
* full kernel rebuild in order to change it.  This is not necessary, as long as the constant matches in both LKM and
* userspace program.  To choose an appropriate value, view the linux/netlink.h file and find an unused value
* (probably best to choose one under 32) following the list of NETLINK_ constants and define the constant here to
* match that value as well as in the userspace program.
*/
#define NETLINK_WIFU    29    // must match userspace definition
#define KERNEL_PID      0    // This is used to identify netlink traffic into and out of the kernel

// These are for wifu, use instead of fins defines.
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
//#define WIFU_PRECLOSE_NAME "wifu_preclose"

#define WIFU_SOCKET 1		// Implemented and tested
#define WIFU_BIND 2		// Implemented , untested...seems ok
#define WIFU_LISTEN 3		// Implemented , untested....seems ok
#define WIFU_ACCEPT 4		// Implemented , untested...not working.
	//#define WIFU_SEND 5 		// unused
#define WIFU_SENDTO 6		// Implemented and tested
	//#define WIFU_RECV 7  		// unused
#define WIFU_RECVFROM 8		// to imp
#define WIFU_CONNECT 9		// Implemented and tested
#define WIFU_GETSOCKOPT 10	// to imp
#define WIFU_SETSOCKOPT 11	// Implemented, but useless maybe
#define WIFU_CLOSE 12		// Implemented and tested
	//#define WIFU_PRECLOSE 13 	// unused 

/** Socket related calls and their codes */
//#define socket_call 1 	== WIFU_SOCKET
//#define bind_call 2 
//#define listen_call 3
//#define connect_call 4
//#define accept_call 5
	#define getname_call 6  //?
	#define ioctl_call 7  //?
//#define sendmsg_call 8 //?  	== WIFU_SENDTO
#define recvmsg_call 9  //?
//#define getsockopt_call 10
//#define setsockopt_call 11  	== WIFU_SETSOCKOPT
	#define release_call 12  //?
    	#define poll_call 13//?`
    	#define mmap_call 14//?
    	#define socketpair_call 15//?
    	#define shutdown_call 16//?
//#define close_call 17  	== WIFU_CLOSE
    	#define sendpage_call 18//?

//only sent from daemon to wedge
#define daemon_start_call 19
#define daemon_stop_call 20
#define poll_event_call 21

/** Additional calls
* To hande special cases
* overwriting the generic functions which write to a socket descriptor
* in order to make sure that we cover as many applications as possible
* This range of these functions will start from 30
*/
#define MAX_CALL_TYPES 22

#define ACK     200
#define NACK     6666
#define MAX_SOCKETS 100
#define MAX_CALLS 100
//#define LOOP_LIMIT 10

/* Data for protocol registration */
static struct proto_ops wifu_proto_ops;
static struct proto wifu_proto;
static struct net_proto_family wifu_net_proto;
/* Protocol specific socket structure */
struct wifu_sock {
    /* struct sock MUST be the first member of wifu_sock */
    struct sock sk;
    /* Add the protocol implementation specific members per socket here from here on */
    // Other stuff might go here, maybe look at IPX or IPv4 registration process
};

// Function prototypes:
static int wifu_create(struct net *net, struct socket *sock, int protocol, int kern);
static int wifu_bind(struct socket *sock, struct sockaddr *addr, int addr_len);
static int wifu_listen(struct socket *sock, int backlog);
static int wifu_connect(struct socket *sock, struct sockaddr *addr, int addr_len, int flags);
static int wifu_accept(struct socket *sock, struct socket *newsock, int flags);
static int wifu_getname(struct socket *sock, struct sockaddr *addr, int *len, int peer);
static int wifu_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *m, size_t len);
static int wifu_recvmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len, int flags);
static int wifu_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg);
static int wifu_release(struct socket *sock);
static unsigned int wifu_poll(struct file *file, struct socket *sock, poll_table *table);
static int wifu_shutdown(struct socket *sock, int how);

static int wifu_getsockopt(struct socket *sock, int level, int optname, char __user *optval, int __user *optlen);
static int wifu_setsockopt(struct socket *sock, int level, int optname, char __user *optval, unsigned int optlen);

static int wifu_socketpair(struct socket *sock1, struct socket *sock2);
static int wifu_mmap(struct file *file, struct socket *sock, struct vm_area_struct *vma);
static ssize_t wifu_sendpage(struct socket *sock, struct page *page, int offset, size_t size, int flags);

/* wifu netlink functions*/
int nl_send(int pid, void *buf, ssize_t len, int flags);
int nl_send_msg(int pid, unsigned int seq, int type, void *buf, ssize_t len, int flags);
void nl_data_ready(struct sk_buff *skb);

// This function extracts a unique ID from the kernel-space perspective for each socket
//inline unsigned long long get_unique_sock_id(struct sock *sk);
inline int get_unique_sock_id(struct sock *sk);

struct nl_wedge_to_daemon {
    unsigned long long sock_id;
    int sock_index;
    
    u_int call_type;
    int call_pid;
    
    u_int call_id;
    int call_index;
};


// What are the companions in the responses from the Backend
//  
//hdr->call_type,         		-->  seems to be the type of message, like message_type
//hdr->call_id, hdr->sock_id,		--> maybeunique identifier....like fd
//hdr->call_index, hdr->sock_index, 	--> maybe index into array
//hdr->ret, 				-->  seems to indicate success or not....ACK and NACK
//hdr->msg, 				-->  this seems to be like return_value...weird. but negative.
//len   == same, no effort needed.
//  				WHere does error fit in here????

struct nl_daemon_to_wedge {
    u_int call_type;
    
    union {
        u_int call_id;
        unsigned long long sock_id; //TODO currently unused, remove if never needed
    };
    union {
        int call_index;
        int sock_index; //TODO currently unused, remove if never needed
    };
    
    u_int ret;
    u_int msg;
};

struct wifu_wedge_call {
    int running; //TODO remove?
    
    u_int call_id;
    u_int call_type;
    
    unsigned long long sock_id;
    int sock_index;
    //TODO timestamp? so can remove after timeout/hit MAX_CALLS cap
    
    //struct semaphore sem; //TODO remove? might be unnecessary
    struct semaphore wait_sem;
    
    u_char reply;
    u_int ret;
    u_int msg;
    u_char *buf;
    int len;
};

void wedge_calls_init(void);
int wedge_calls_insert(u_int id, unsigned int sock_id, int sock_index, u_int type);
int wedge_calls_find(unsigned int sock_id, /*int sock_index,*/ u_int type);
int wedge_calls_remove(u_int id);
void wedge_calls_remove_all(void);

struct wifu_wedge_socket {
    int running; //TODO remove? merge with release_flag
    
    unsigned long long sock_id;
    struct socket *sock;
    struct sock *sk;
    
    int threads[MAX_CALL_TYPES];
    
    int release_flag;
    struct socket *sock_new;
    struct sock *sk_new;
};

void wedge_sockets_init(void);
int wedge_sockets_insert(unsigned int sock_id, struct sock *sk);
int wedge_sockets_find(unsigned int sock_id);
int wedge_sockets_remove(unsigned int sock_id, int sock_index, u_int type);
void wedge_socket_remove_all(void);
int wedge_sockets_wait(unsigned int sock_id, int sock_index, u_int calltype);
int checkConfirmation(int sock_index);

/* This is a flag to enable or disable the wifu stack passthrough */
int wifu_passthrough_enabled;
EXPORT_SYMBOL (wifu_passthrough_enabled);












#endif /* wifu_WEDGE_H_ */
