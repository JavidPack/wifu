/*
* wifu_wedge.c -
*/

/* License and signing info */
//#define M_LICENSE    "GPL"    // Most common, but we're releasing under BSD 3-clause, the
#define M_LICENSE    "GPL and additional rights"
#define M_DESCRIPTION    "Unregisters AF_INET and registers the WIFU protocol in its place"
#define M_AUTHOR    "Jonathan Reed <jonathanreed07@gmail.com>"

#include <linux/module.h>    /* Needed by all modules */
//#include <linux/kernel.h>    /* Needed for KERN_INFO */
//#include <linux/init.h>        /* Needed for the macros */
#include <net/sock.h>        /* Needed for proto and sock struct defs, etc. */
//#include <linux/socket.h>    /* Needed for the sockaddr struct def */
//#include <linux/errno.h>    /* Needed for error number defines */
//#include <linux/aio.h>        /* Needed for wifu_sendmsg */
//#include <linux/skbuff.h>    /* Needed for sk_buff struct def, etc. */
//#include <linux/net.h>        /* Needed for socket struct def, etc. */
//#include <linux/netlink.h>    /* Needed for netlink socket API, macros, etc. */
//#include <linux/semaphore.h>    /* Needed to lock/unlock blocking calls with handler */
//#include <asm/uaccess.h>    /** Copy from user */
#include <asm/ioctls.h>        /* Needed for wifu_ioctl */
//#include <linux/sockios.h>
//#include <linux/delay.h>    /* For sleep */
#include <linux/if.h>        /* Needed for wifu_ioctl */

#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/moduleparam.h>

#include "wifu_wedge.h"    /* Defs for this module */
#include "MessageStructDefinitionsCStyleForWedge.h"  // defs for messages.


MODULE_LICENSE("GPL");


#define RECV_BUFFER_SIZE    1024    // Same as userspace, Pick an appropriate value here
#define AF_WIFU 2
#define PF_WIFU AF_WIFU
#define NETLINK_WIFU 29







// Create one semaphore here for every socketcall that is going to block
struct wifu_wedge_socket wedge_sockets[MAX_SOCKETS];
struct semaphore wedge_sockets_sem;

struct wifu_wedge_call wedge_calls[MAX_CALLS];
struct semaphore wedge_calls_sem; //TODO merge with sockets_sem?
u_int call_count; //TODO fix eventual roll over problem

// Data declarations
/* Data for netlink sockets */
struct sock *wifu_nl_sk = NULL;
struct sock *wifu_unix_sk = NULL;

int wifu_daemon_pid; // holds the pid of the WIFU daemon so we know who to send back to
struct semaphore link_sem;

//extern static const struct net_proto_family __rcu *net_families[NPROTO] __read_mostly;

int (*inet_create_function)(struct net *, struct socket *, int ,int ) = NULL;   // 
int* inet_family_ops_address = NULL;  
int (*inet_init_function)(void) = NULL;   // 
int (*proc_pid_cmdline_function)(struct task_struct *, char *) = NULL;

static int
symbol_walk_callback(void *data, const char *name, struct module *mod, 
        unsigned long addr)
{
        /* Skip the symbol if it belongs to a module rather than to 
         * the kernel proper. */
        if (mod != NULL) 
                return 0;
        
        if (strcmp(name, "inet_create") == 0) {
                if (inet_create_function != NULL) {
                      //  pr_warning(KEDR_MSG_PREFIX "Found two \"inet_create\" symbols in the kernel, unable to continue\n");
                        return -EFAULT;
                }
                inet_create_function = (int (*)(struct net *, struct socket *,int,int))addr;

        } 

	if (strcmp(name, "inet_init") == 0) {
                if (inet_init_function != NULL) {
                      //  pr_warning(KEDR_MSG_PREFIX "Found two \"inet_create\" symbols in the kernel, unable to continue\n");
                        return -EFAULT;
                }
                inet_init_function = (int (*)(void))addr;

        } 

	if (strcmp(name, "inet_family_ops") == 0) {
                if (inet_family_ops_address != NULL) {
                      //  pr_warning(KEDR_MSG_PREFIX "Found two \"inet_create\" symbols in the kernel, unable to continue\n");
                        return -EFAULT;
                }
                inet_family_ops_address = (void *)addr;

        } 

	if (strcmp(name, "proc_pid_cmdline") == 0) {
                if (proc_pid_cmdline_function != NULL) {
                      //  pr_warning(KEDR_MSG_PREFIX "Found two \"inet_create\" symbols in the kernel, unable to continue\n");
                        return -EFAULT;
                }
                proc_pid_cmdline_function = (void *)addr;

        } 

        return 0;
}


int print_exit(const char *func, int line, int rc) {
    #ifdef DEBUG
    printk(KERN_DEBUG "WIFU: DEBUG: %s, %d: Exited: %d\n", func, line, rc);
    #endif
    return rc;
}

void wedge_calls_init(void) {
    int i;
    PRINT_DEBUG("Entered");
    
    call_count = 0;
    
    sema_init(&wedge_calls_sem, 1);
    for (i = 0; i < MAX_CALLS; i++) {
        wedge_calls[i].call_id = -1;
    }
    
    PRINT_DEBUG("Exited.");
}

int wedge_calls_insert(u_int call_id, unsigned int sock_id, int sock_index, u_int call_type) { //TODO might not need sock
    int i;
    
    PRINT_DEBUG("Entered: sock_id=%u, sock_index=%d, call_id=%u, call_type=%u", sock_id, sock_index, call_id, call_type);
    
    for (i = 0; i < MAX_CALLS; i++) {
        if (wedge_calls[i].call_id == -1) {
            wedge_calls[i].running = 1;
            
            wedge_calls[i].call_id = call_id;
            wedge_calls[i].call_type = call_type;
            
            wedge_calls[i].sock_id = sock_id;
            wedge_calls[i].sock_index = sock_index;
            
            //sema_init(&wedge_calls[i].sem, 1);
            wedge_calls[i].reply = 0;
            sema_init(&wedge_calls[i].wait_sem, 0);
            
            wedge_calls[i].ret = 0;
            
            return print_exit(__FUNCTION__, __LINE__, i);
        }
    }
    
    return print_exit(__FUNCTION__, __LINE__, -1);
}

int wedge_calls_find(unsigned int sock_id, /*int sock_index,*/ u_int call_type) {
    u_int i;
    
 //   PRINT_DEBUG("Entered: sock_id=%u, /*sock_index=%d, call_type=%u", sock_id, sock_index, call_type);
  PRINT_DEBUG("Entered: sock_id=%u, call_type=%u", sock_id, call_type);
    
    for (i = 0; i < MAX_CALLS; i++) {
        if (wedge_calls[i].call_id != -1 && wedge_calls[i].sock_id == sock_id && /*wedge_calls[i].sock_index == sock_index &&*/ wedge_calls[i].call_type == call_type) { //TODO remove sock_index? maybe unnecessary
            return print_exit(__FUNCTION__, __LINE__, i);
        }
    }
    
    return print_exit(__FUNCTION__, __LINE__, -1);
}

int wedge_calls_remove(u_int call_id) { //TODO remove? not used since id/index typicall tied, & removal doesn't need locking
    int i;
    
    PRINT_DEBUG("Entered: call_id=%u", call_id);
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    for (i = 0; i < MAX_CALLS; i++) {
        if (wedge_calls[i].call_id == call_id) {
            wedge_calls[i].call_id = -1;
            
            up(&wedge_calls_sem);
            
            //TODO finish
            return (1);
        }
    }
    up(&wedge_calls_sem);
    return (-1);
}

void wedge_calls_remove_all(void) {
    u_int i;
    
    PRINT_DEBUG("Entered");
    
    for (i = 0; i < MAX_CALLS; i++) {
        if (wedge_calls[i].call_id != -1) {
            up(&wedge_calls[i].wait_sem);
            
            msleep(1); //TODO may need to change
        }
    }
}

void wedge_sockets_init(void) {
    int i;
    
    PRINT_DEBUG("Entered");
    
    sema_init(&wedge_sockets_sem, 1);
    for (i = 0; i < MAX_SOCKETS; i++) {
        wedge_sockets[i].sock_id = -1;
    }
    
    //PRINT_DEBUG("Exited.");
}

int wedge_sockets_insert(unsigned int sock_id, struct sock *sk) { //TODO might not need sock
    int i;
    int j;
    
    PRINT_DEBUG("Entered: sock_id%llu, sk=%p", sock_id, sk);
    
    for (i = 0; i < MAX_SOCKETS; i++) {
        if ((wedge_sockets[i].sock_id == -1)) {
            wedge_sockets[i].running = 1;
            
            wedge_sockets[i].sock_id = sock_id;
            wedge_sockets[i].sk = sk;
            
            for (j = 0; j < MAX_CALL_TYPES; j++) {
                wedge_sockets[i].threads[j] = 0;
            }
            
            wedge_sockets[i].release_flag = 0;
            wedge_sockets[i].sk_new = NULL;
            
            return print_exit(__FUNCTION__, __LINE__, i);
            //return i;
        }
    }
    
    return print_exit(__FUNCTION__, __LINE__, -1);
    //return -1;
}

int wedge_sockets_find(unsigned int sock_id) {
    int i;
    
    PRINT_DEBUG("Entered: sock_id=%u", sock_id);
    
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (wedge_sockets[i].sock_id == sock_id) {
            return print_exit(__FUNCTION__, __LINE__, i);
        }
    }
    
    return print_exit(__FUNCTION__, __LINE__, -1);
}

int wedge_sockets_remove(unsigned int sock_id, int sock_index, u_int call_type) {
    u_int i;
    int call_index;
    
    PRINT_DEBUG("Entered: sock_id=%u, sock_index=%d, call_type=%u", sock_id, sock_index, call_type);
    
    for (i = 0; i < MAX_CALL_TYPES; i++) {
        while (1) {
            if (wedge_sockets[sock_index].threads[i] < 1 || (i == call_type && wedge_sockets[sock_index].threads[i] < 2)) {
                break;
            }
            up(&wedge_sockets_sem);
            
            if (down_interruptible(&wedge_calls_sem)) {
                PRINT_ERROR("calls_sem acquire fail");
                //TODO error
            }
            call_index = wedge_calls_find(sock_id,/* sock_index,*/ i);
            up(&wedge_calls_sem);
            if (call_index == -1) {
                break;
            }
            up(&wedge_calls[call_index].wait_sem);
            
            msleep(1); //TODO may need to change
            
            if (down_interruptible(&wedge_sockets_sem)) {
                PRINT_ERROR("sockets_sem acquire fail");
                //TODO error
            }
        }
    }
    
    wedge_sockets[sock_index].sock_id = -1;
    
    return 0;
}

void wedge_socket_remove_all(void) {
    u_int i;
    u_int j;
    int call_index;
    
    PRINT_DEBUG("Entered");
    
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (wedge_sockets[i].sock_id != -1) {
            for (j = 0; j < MAX_CALL_TYPES; j++) {
                while (1) {
                    if (wedge_sockets[i].threads[j] < 1) {
                        break;
                    }
                    up(&wedge_sockets_sem);
                    
                    if (down_interruptible(&wedge_calls_sem)) {
                        PRINT_ERROR("calls_sem acquire fail");
                        //TODO error
                    }
                    call_index = wedge_calls_find(wedge_sockets[i].sock_id, /*i,*/ j);
                    up(&wedge_calls_sem);
                    if (call_index == -1) {
                        break;
                    }
                    up(&wedge_calls[call_index].wait_sem);
                    
                    msleep(1); //TODO may need to change
                    
                    if (down_interruptible(&wedge_sockets_sem)) {
                        PRINT_ERROR("sockets_sem acquire fail");
                        //TODO error
                    }
                }
            }
            
            wedge_sockets[i].sock_id = -1;
        }
    }
}

int threads_incr(int sock_index, u_int call) {
    int ret = 1;
    
    return ret;
}

int threads_decr(int sock_index, u_int call) {
    int ret = 0;
    
    return ret;
}

int wedge_sockets_wait(unsigned int sock_id, int sock_index, u_int calltype) {
    //int error = 0;
    
    PRINT_DEBUG("Entered for sock=%llu, sock_index=%d, call=%u", sock_id, sock_index, calltype);
    
    return print_exit(__FUNCTION__, __LINE__, 0);
}

int checkConfirmation(int call_index) {
    PRINT_DEBUG("Entered: call_index=%d", call_index);
    
    //extract msg from reply in wedge_calls[sock_index]
    if (wedge_calls[call_index].ret == ACK) {
        PRINT_DEBUG("recv ACK");
        if (wedge_calls[call_index].len == 0) {
            return 0;
        } else {
            PRINT_ERROR("wedge_calls[sock_index].reply_buf error, wedge_calls[%d].len=%d, wedge_calls[%d].buf=%p",
            call_index, wedge_calls[call_index].len, call_index, wedge_calls[call_index].buf);
            return -1;
        }
    } else if (wedge_calls[call_index].ret == NACK) {
        PRINT_DEBUG("recv NACK msg=%u", wedge_calls[call_index].msg);
        return -wedge_calls[call_index].msg;
    } else {
        PRINT_ERROR("error, acknowledgement: %u", wedge_calls[call_index].ret);
        return -1;
    }
}

/* WIFU Netlink functions  */
/*
* Sends len bytes from buffer buf to process pid, and sets the flags.
* If buf is longer than RECV_BUFFER_SIZE, it's broken into sequential messages.
* Returns 0 if successful or -1 if an error occurred.
*/

//assumes msg_buf is just the msg, does not have a prepended msg_len
//break msg_buf into parts of size RECV_BUFFER_SIZE with a prepended header (header part of RECV...)
//prepend msg header: total msg length, part length, part starting position
int nl_send_msg(int pid, unsigned int seq, int type, void *buf, ssize_t len, int flags) {
    struct nlmsghdr *nlh;
    struct sk_buff *skb;
    int ret_val;
    
    //####################
    u_char *print_buf;
    u_char *print_pt;
    u_char *pt;
    int i;
    
    PRINT_DEBUG("pid=%d, seq=%d, type=%d, len=%d", pid, seq, type, len);
    
    print_buf = (u_char *) kmalloc(5 * len, GFP_KERNEL);
    if (print_buf == NULL) {
        PRINT_ERROR("print_buf allocation fail");
        } else {
        print_pt = print_buf;
        pt = buf;
        for (i = 0; i < len; i++) {
            if (i == 0) {
                sprintf(print_pt, "%02x", *(pt + i));
                print_pt += 2;
                } else if (i % 4 == 0) {
                sprintf(print_pt, ":%02x", *(pt + i));
                print_pt += 3;
                } else {
                sprintf(print_pt, " %02x", *(pt + i));
                print_pt += 3;
            }
        }
        PRINT_DEBUG("buf='%s'", print_buf);
        kfree(print_buf);
    }
    //####################
    
    // Allocate a new netlink message
    skb = nlmsg_new(len, 0); // nlmsg_new(size_t payload, gfp_t flags)
    if (skb == NULL) {
        PRINT_ERROR("netlink Failed to allocate new skb");
        return -1;
    }
    
    // Load nlmsg header
    // nlmsg_put(struct sk_buff *skb, u32 pid, u32 seq, int type, int payload, int flags)
    nlh = nlmsg_put(skb, KERNEL_PID, seq, type, len, flags);
    NETLINK_CB(skb).dst_group = 0; // not in a multicast group
    
    // Copy data into buffer
    memcpy(NLMSG_DATA(nlh), buf, len);
    
    // Send the message
    ret_val = nlmsg_unicast(wifu_nl_sk, skb, pid);
    if (ret_val < 0) {
        PRINT_ERROR("netlink error sending to user");
        return -1;
    }
    
    return 0;
}

int nl_send(int pid, void *msg_buf, ssize_t msg_len, int flags) {
    int ret = nl_send_msg(pid, 0, NLMSG_DONE, msg_buf, msg_len, flags);
    if (ret < 0) {
        PRINT_ERROR("netlink error sending seq %d to user", 0);
        up(&link_sem);
        return -1;
    }
    
    kfree(msg_buf);
    up(&link_sem);
    
    return 0;

/*
    int ret;
    void *part_buf;
    u_char *msg_pt;
    int pos;
    u_int seq;
    u_char *hdr_msg_len;
    u_char *hdr_part_len;
    u_char *hdr_pos;
    u_char *msg_start;
    ssize_t header_size;
    ssize_t part_len;
    
    //#################### Debug
    u_char *print_buf;
    u_char *print_pt;
    u_char *pt;
    int i;
    //####################
    
    if (down_interruptible(&link_sem)) {
        PRINT_ERROR("link_sem acquire fail");
    }
    
    //#################### Debug
    print_buf = (u_char *) kmalloc(5 * msg_len, GFP_KERNEL);
    if (print_buf == NULL) {
        PRINT_ERROR("print_buf allocation fail");
        } else {
        print_pt = print_buf;
        pt = msg_buf;
        for (i = 0; i < msg_len; i++) {
            if (i == 0) {
                sprintf(print_pt, "%02x", *(pt + i));
                print_pt += 2;
                } else if (i % 4 == 0) {
                sprintf(print_pt, ":%02x", *(pt + i));
                print_pt += 3;
                } else {
                sprintf(print_pt, " %02x", *(pt + i));
                print_pt += 3;
            }
        }
        PRINT_DEBUG("msg_buf='%s'", print_buf);
        kfree(print_buf);
    }
    //####################
    
    part_buf = (u_char *) kmalloc(RECV_BUFFER_SIZE, GFP_KERNEL);
    if (part_buf == NULL) {
        PRINT_ERROR("part_buf allocation fail");
        up(&link_sem);
        return -1;
    }
    
    msg_pt = msg_buf;
    pos = 0;
    seq = 0;
    
    hdr_msg_len = part_buf;
    hdr_part_len = hdr_msg_len + sizeof(ssize_t);
    hdr_pos = hdr_part_len + sizeof(ssize_t);
    msg_start = hdr_pos + sizeof(int);
    
    header_size = msg_start - hdr_msg_len;
    part_len = RECV_BUFFER_SIZE - header_size;
    
    *(ssize_t *) hdr_msg_len = msg_len;
    *(ssize_t *) hdr_part_len = part_len;
    
    while (msg_len - pos > part_len) {
        PRINT_DEBUG("pos=%d", pos);
        
        *(int *) hdr_pos = pos;
        
        memcpy(msg_start, msg_pt, part_len);
        
        PRINT_DEBUG("seq=%d", seq);
        
        ret = nl_send_msg(pid, seq, 0x0, part_buf, RECV_BUFFER_SIZE, flags); //| NLM_F_MULTI
        if (ret < 0) {
            PRINT_ERROR("netlink error sending seq %d to user", seq);
            up(&link_sem);
            return -1;
        }
        
        msg_pt += part_len;
        pos += part_len;
        seq++;
    }
    
    part_len = msg_len - pos;
    *(ssize_t *) hdr_part_len = part_len;
    *(int *) hdr_pos = pos;
    
    memcpy(msg_start, msg_pt, part_len);
    
    ret = nl_send_msg(pid, seq, NLMSG_DONE, part_buf, header_size + part_len, flags);
    if (ret < 0) {
        PRINT_ERROR("netlink error sending seq %d to user", seq);
        up(&link_sem);
        return -1;
    }
    
    kfree(part_buf);
    up(&link_sem);
    
    return 0;*/
}

/*
* This function is automatically called when the kernel receives a datagram on the corresponding netlink socket.
*/
void nl_data_ready(struct sk_buff *skb) {
	struct nlmsghdr *nlh;
	u_char *buf; // Pointer to data in payload
	ssize_t len; // Payload length
    	int pid; // pid of sending process
    	struct nl_daemon_to_wedge *hdr;
    
    	u_int reply_call; // a number corresponding to the type of socketcall this packet is in response to
    
    	PRINT_DEBUG("Entered: skb=%p", skb);
    
    	if (skb == NULL) {
        	PRINT_DEBUG("Exiting: skb NULL \n");
        	return;
    	}
    	nlh = (struct nlmsghdr *) skb->data;
    	pid = nlh->nlmsg_pid; // get pid from the header
    
    	// Get a pointer to the start of the data in the buffer and the buffer (payload) length
    	buf = (u_char *) (NLMSG_DATA(nlh));
    	len = NLMSG_PAYLOAD(nlh, 0);

	PRINT_DEBUG("^^^ NLMSG_HDRLEN=%d, nlh->nlmsg_len=%d , reallen?:%d ", NLMSG_HDRLEN, nlh->nlmsg_len, nlh->nlmsg_len - NLMSG_HDRLEN);
    
    	PRINT_DEBUG("nl_pid=%d, nl_len=%d", pid, len);
    


	// I really should change this to be a struct just like the others.
	if (len == sizeof(u_int)) {
		reply_call = *(u_int *) buf;
		if (reply_call == daemon_start_call) {
			if (wifu_daemon_pid != -1) {
				PRINT_DEBUG("Daemon pID changed, old pid=%d", wifu_daemon_pid);
			}
                	//wifu_passthrough_enabled = 1;
                	wifu_daemon_pid = pid;
                	PRINT_DEBUG("Daemon connected, pid=%d", wifu_daemon_pid);
            	} else if (reply_call == daemon_stop_call) {
                	PRINT_DEBUG("Daemon disconnected");
                	//wifu_passthrough_enabled = 0;
                	wifu_daemon_pid = -1; //TODO expand this functionality
                
                	if (down_interruptible(&wedge_sockets_sem)) {
                    		PRINT_ERROR("sockets_sem acquire fail");
                    		//TODO error
                	}
                	wedge_socket_remove_all();
                	up(&wedge_sockets_sem);
                
                	if (down_interruptible(&wedge_calls_sem)) {
                    		PRINT_ERROR("sockets_sem acquire fail");
                    		//TODO error
                	}
                	wedge_calls_remove_all();
                	up(&wedge_calls_sem);
                } else {
                	//TODO drop?
                	PRINT_DEBUG("Dropping...");
		}
	} else {
            	hdr = (struct nl_daemon_to_wedge *) buf;
            	//len -= sizeof(struct SocketResponseMessag); // handle this for each struct.

                if (down_interruptible(&wedge_calls_sem)) {
                    PRINT_ERROR("calls_sem acquire fail");
                    //TODO error
                }

		struct GenericResponseMessage* grm = (struct GenericResponseMessage*) buf;
		PRINT_DEBUG("grm->message_type %i\n",grm->message_type);
		

// If this works, I should  be able to reduce the repeted code by 95%

		switch (grm->message_type) {
        		case WIFU_RECVFROM:
        		//case WIFU_RECV:
			{	// UNTESTED
				PRINT_DEBUG("case WIFU_RECVFROM or RECV -- TODO");

				struct RecvFromResponseMessage* rfrm = (struct RecvFromResponseMessage*) buf;
				PRINT_DEBUG("rfrm->fd %u\n",rfrm->fd);
				PRINT_DEBUG("rfrm->return_value %i\n",rfrm->return_value);
				PRINT_DEBUG("rfrm->error %i\n",rfrm->error);

				int wedge_sockets_index = wedge_sockets_find(rfrm->fd);
				int wedge_calls_index = wedge_calls_find(rfrm->fd, WIFU_RECVFROM);
				
				wedge_calls[wedge_calls_index].ret = ACK;
                   		wedge_calls[wedge_calls_index].msg = rfrm->return_value;
                    		wedge_calls[wedge_calls_index].buf = buf + sizeof(struct RecvFromResponseMessage);
                    		wedge_calls[wedge_calls_index].len = len - sizeof(struct RecvFromResponseMessage);
                    		wedge_calls[wedge_calls_index].reply = 1;

				PRINT_DEBUG("Attempting to wake: wedge_calls_index %i\n",wedge_calls_index);
				up(&wedge_calls[wedge_calls_index].wait_sem);

// I may have to do something here, there is extra data associated with recvfrom, like addr and addrlen

       				break;
			}
       			case WIFU_SENDTO:
       			//case WIFU_SEND:
			{	
				PRINT_DEBUG("case WIFU_SEND or SENDTO -- TODO");

				struct SendToResponseMessage* strm = (struct SendToResponseMessage*) buf;
				PRINT_DEBUG("strm->fd %u\n",strm->fd);
				PRINT_DEBUG("strm->return_value %i\n",strm->return_value);
				PRINT_DEBUG("strm->error %i\n",strm->error);

				int wedge_sockets_index = wedge_sockets_find(strm->fd);
				int wedge_calls_index = wedge_calls_find(strm->fd, WIFU_SENDTO);
				
				wedge_calls[wedge_calls_index].ret = ACK;
                   		wedge_calls[wedge_calls_index].msg = strm->return_value;
                    		wedge_calls[wedge_calls_index].buf = buf + sizeof(struct SendToResponseMessage);
                    		wedge_calls[wedge_calls_index].len = len - sizeof(struct SendToResponseMessage);
                    		wedge_calls[wedge_calls_index].reply = 1;

				PRINT_DEBUG("Attempting to wake: wedge_calls_index %i\n",wedge_calls_index);
				up(&wedge_calls[wedge_calls_index].wait_sem);

       				break;
			}
      			case WIFU_SOCKET:
       			{
				PRINT_DEBUG("case WIFU_SOCKET");
				struct SocketResponseMessag* srm = (struct SocketResponseMessag*) buf;
				PRINT_DEBUG("srm->fd %u\n",srm->fd);
				PRINT_DEBUG("srm->return_value %i\n",srm->return_value);
				PRINT_DEBUG("srm->error %i\n",srm->error);

				int wedge_sockets_index = wedge_sockets_find(srm->fd);
				int wedge_calls_index = wedge_calls_find(srm->fd, WIFU_SOCKET);
				
				wedge_calls[wedge_calls_index].ret = ACK;
                   		wedge_calls[wedge_calls_index].msg = srm->return_value;
                    		wedge_calls[wedge_calls_index].buf = buf + sizeof(struct SocketResponseMessag);
                    		wedge_calls[wedge_calls_index].len = len - sizeof(struct SocketResponseMessag);
                    		wedge_calls[wedge_calls_index].reply = 1;

				PRINT_DEBUG("Attempting to wake: wedge_calls_index %i\n",wedge_calls_index);
				up(&wedge_calls[wedge_calls_index].wait_sem);

            			break;
        		}

        		case WIFU_BIND: // UNTESTED
			{	
				PRINT_DEBUG("case WIFU_BIND-- TODO");

				struct BindResponseMessage* brm = (struct BindResponseMessage*) buf;
				PRINT_DEBUG("brm->fd %u\n",brm->fd);
				PRINT_DEBUG("brm->return_value %i\n",brm->return_value);
				PRINT_DEBUG("brm->error %i\n",brm->error);

				int wedge_sockets_index = wedge_sockets_find(brm->fd);
				int wedge_calls_index = wedge_calls_find(brm->fd, WIFU_BIND);
				
				wedge_calls[wedge_calls_index].ret = ACK;
                   		wedge_calls[wedge_calls_index].msg = brm->return_value;
                    		wedge_calls[wedge_calls_index].buf = buf + sizeof(struct BindResponseMessage);
                    		wedge_calls[wedge_calls_index].len = len - sizeof(struct BindResponseMessage);
                    		wedge_calls[wedge_calls_index].reply = 1;

				PRINT_DEBUG("Attempting to wake: wedge_calls_index %i\n",wedge_calls_index);
				up(&wedge_calls[wedge_calls_index].wait_sem);

       				break;
			}
        		case WIFU_LISTEN: // UNTESTED
			{	
				PRINT_DEBUG("case LISTEN -- TODO");

				struct ListenResponseMessage* lrm = (struct ListenResponseMessage*) buf;
				PRINT_DEBUG("lrm->fd %u\n",lrm->fd);
				PRINT_DEBUG("lrm->return_value %i\n",lrm->return_value);
				PRINT_DEBUG("lrm->error %i\n",lrm->error);

				int wedge_sockets_index = wedge_sockets_find(lrm->fd);
				int wedge_calls_index = wedge_calls_find(lrm->fd, WIFU_LISTEN);
				
				wedge_calls[wedge_calls_index].ret = ACK;
                   		wedge_calls[wedge_calls_index].msg = lrm->return_value;
                    		wedge_calls[wedge_calls_index].buf = buf + sizeof(struct ListenResponseMessage);
                    		wedge_calls[wedge_calls_index].len = len - sizeof(struct ListenResponseMessage);
                    		wedge_calls[wedge_calls_index].reply = 1;

				PRINT_DEBUG("Attempting to wake: wedge_calls_index %i\n",wedge_calls_index);
				up(&wedge_calls[wedge_calls_index].wait_sem);

       				break;
			}
        		case WIFU_ACCEPT: // UNTESTED
			{	
				PRINT_DEBUG("case WIFU_ACCEPT -- TODO");

				struct AcceptResponseMessage* arm = (struct AcceptResponseMessage*) buf;
				PRINT_DEBUG("arm->fd %u\n",arm->fd);
				PRINT_DEBUG("arm->return_value %i\n",arm->return_value);
				PRINT_DEBUG("arm->error %i\n",arm->error);

				int wedge_sockets_index = wedge_sockets_find(arm->fd);
				int wedge_calls_index = wedge_calls_find(arm->fd, WIFU_ACCEPT);
				
				wedge_calls[wedge_calls_index].ret = ACK;
                   		wedge_calls[wedge_calls_index].msg = arm->return_value;
                    		wedge_calls[wedge_calls_index].buf = buf + sizeof(struct AcceptResponseMessage);
                    		wedge_calls[wedge_calls_index].len = len - sizeof(struct AcceptResponseMessage);
                    		wedge_calls[wedge_calls_index].reply = 1;

				PRINT_DEBUG("Attempting to wake: wedge_calls_index %i\n",wedge_calls_index);
				up(&wedge_calls[wedge_calls_index].wait_sem);

// I may have to do something here, there is extra data associated with recvfrom, like addr and addrlen

       				break;
			}
        		case WIFU_CONNECT: // UNTESTED
			{	
				PRINT_DEBUG("case WIFU_CONNECT -- TODO");

				struct ConnectResponseMessage* crm = (struct ConnectResponseMessage*) buf;
				PRINT_DEBUG("crm->fd %u\n",crm->fd);
				PRINT_DEBUG("crm->return_value %i\n",crm->return_value);
				PRINT_DEBUG("crm->error %i\n",crm->error);

				int wedge_sockets_index = wedge_sockets_find(crm->fd);
				int wedge_calls_index = wedge_calls_find(crm->fd, WIFU_CONNECT);
				
				wedge_calls[wedge_calls_index].ret = ACK;
                   		wedge_calls[wedge_calls_index].msg = crm->return_value;
                    		wedge_calls[wedge_calls_index].buf = buf + sizeof(struct ConnectResponseMessage);
                    		wedge_calls[wedge_calls_index].len = len - sizeof(struct ConnectResponseMessage);
                    		wedge_calls[wedge_calls_index].reply = 1;

				PRINT_DEBUG("Attempting to wake: wedge_calls_index %i\n",wedge_calls_index);
				up(&wedge_calls[wedge_calls_index].wait_sem);

       				break;
			}
	       		case WIFU_GETSOCKOPT: // UNTESTED
			{	
				PRINT_DEBUG("case WIFU_GETSOCKOPT -- TODO");

				struct GetSockOptResponseMessage* gsorm = (struct GetSockOptResponseMessage*) buf;
				PRINT_DEBUG("gsorm->fd %u\n",gsorm->fd);
				PRINT_DEBUG("gsorm->return_value %i\n",gsorm->return_value);
				PRINT_DEBUG("gsorm->error %i\n",gsorm->error);

				int wedge_sockets_index = wedge_sockets_find(gsorm->fd);
				int wedge_calls_index = wedge_calls_find(gsorm->fd, WIFU_GETSOCKOPT);
				
				wedge_calls[wedge_calls_index].ret = ACK;
                   		wedge_calls[wedge_calls_index].msg = gsorm->return_value;
                    		wedge_calls[wedge_calls_index].buf = buf + sizeof(struct GetSockOptResponseMessage);
                    		wedge_calls[wedge_calls_index].len = len - sizeof(struct GetSockOptResponseMessage);
                    		wedge_calls[wedge_calls_index].reply = 1;

				PRINT_DEBUG("Attempting to wake: wedge_calls_index %i\n",wedge_calls_index);
				up(&wedge_calls[wedge_calls_index].wait_sem);

// I may have to do something here, there is extra data associated with this: sockoptval

       				break;
			}
        		case WIFU_SETSOCKOPT:
				PRINT_DEBUG("case WIFU_SETSOCKOPT -- TODO");
				struct SetSockOptResponseMessage* ssorm = (struct SetSockOptResponseMessage*) buf;
				PRINT_DEBUG("ssorm->fd %u\n",ssorm->fd);
				PRINT_DEBUG("ssorm->return_value %i\n",ssorm->return_value);
				PRINT_DEBUG("ssorm->error %i\n",ssorm->error);

				int wedge_sockets_index = wedge_sockets_find(ssorm->fd);
				int wedge_calls_index = wedge_calls_find(ssorm->fd, WIFU_SETSOCKOPT);
				
				wedge_calls[wedge_calls_index].ret = ACK;
                   		wedge_calls[wedge_calls_index].msg = ssorm->return_value;
                    		wedge_calls[wedge_calls_index].buf = buf + sizeof(struct SetSockOptResponseMessage);
                    		wedge_calls[wedge_calls_index].len = len - sizeof(struct SetSockOptResponseMessage);
                    		wedge_calls[wedge_calls_index].reply = 1;

				PRINT_DEBUG("Attempting to wake: wedge_calls_index %i\n",wedge_calls_index);
				up(&wedge_calls[wedge_calls_index].wait_sem);
            			break;

        		case WIFU_CLOSE:
			{
				PRINT_DEBUG("case WIFU_CLOSE -- TODO");
				struct CloseResponseMessage* crm = (struct CloseResponseMessage*) buf;
				PRINT_DEBUG("crm->fd %u\n",crm->fd);
				PRINT_DEBUG("crm->return_value %i\n",crm->return_value);
				PRINT_DEBUG("crm->error %i\n",crm->error);

				int wedge_sockets_index = wedge_sockets_find(crm->fd);
				int wedge_calls_index = wedge_calls_find(crm->fd, WIFU_CLOSE);
				
				wedge_calls[wedge_calls_index].ret = ACK;
                   		wedge_calls[wedge_calls_index].msg = crm->return_value;
                    		wedge_calls[wedge_calls_index].buf = buf + sizeof(struct CloseResponseMessage);
                    		wedge_calls[wedge_calls_index].len = len - sizeof(struct CloseResponseMessage);
                    		wedge_calls[wedge_calls_index].reply = 1;

				PRINT_DEBUG("Attempting to wake: wedge_calls_index %i\n",wedge_calls_index);
				up(&wedge_calls[wedge_calls_index].wait_sem);
            			break;
			}
        		default:
				PRINT_DEBUG("Unknown message type");
    		}
		up(&wedge_calls_sem);
	//Done with determining reply type and reacting to it.
	}

}


/*// Should I go back to using this>??

    // **** Remember the LKM must be up first, then the daemon,
    // but the daemon must make contact before any applications try to use socket()
    
    if (pid == -1) { // if the socket daemon hasn't made contact before
        // Print what we received
        PRINT_DEBUG("No msg pID, received='%p'", buf);
    } else {
        if (len >= sizeof(struct nl_daemon_to_wedge)) {
            hdr = (struct nl_daemon_to_wedge *) buf;
            len -= sizeof(struct nl_daemon_to_wedge);
            
            //
            //* extract common values and pass rest to shared buffer
            //* reply_call & sock_id, are to verify buf goes to the right sock & call
            //* This is preemptive as with multithreading we may have to add a shared queue
            //
            
            PRINT_DEBUG("Reply: call_type=%u, call_id=%u, call_index=%d, sock_id=%u, sock_index=%d, ret=%u, msg=%u, len=%d",
            hdr->call_type, hdr->call_id, hdr->call_index, hdr->sock_id, hdr->sock_index, hdr->ret, hdr->msg, len);
            
            if (hdr->call_type == 0) { //set to different calls
                if (hdr->sock_index == -1 || hdr->sock_index > MAX_SOCKETS) {
                    PRINT_ERROR("invalid sock_index: sock_index=%d", hdr->sock_index);
                    goto end;
                }
                if (down_interruptible(&wedge_sockets_sem)) {
                    PRINT_ERROR("sockets_sem acquire fail");
                    //TODO error
                }
                if (wedge_sockets[hdr->sock_index].sock_id != hdr->sock_id) {
                    up(&wedge_sockets_sem);
                    PRINT_ERROR("socket removed: sock_index=%d, sock_id=%u", hdr->sock_index, hdr->sock_id);
                    goto end;
                }
                
                PRINT_DEBUG("sock_index=%d, type=%u, threads=%d", hdr->sock_index, hdr->call_type, wedge_sockets[hdr->sock_index].threads[hdr->call_type]);
                if (wedge_sockets[hdr->sock_index].threads[hdr->call_type] < 1) { //TODO may be unnecessary, since have call_index/call_id
                    up(&wedge_sockets_sem);
                    PRINT_ERROR("Exiting: no waiting threads found: sock_index=%d, type=%u", hdr->sock_index, hdr->call_type);
                    goto end;
                }
                up(&wedge_sockets_sem);
                
                if (wedge_sockets[hdr->sock_index].release_flag && (hdr->call_type != release_call)) { //TODO: may be unnecessary & can be removed (flag, etc)
                    PRINT_DEBUG("socket released, dropping for sock_index=%d, sock_id=%u, type=%d", hdr->sock_index, hdr->sock_id, hdr->call_type);
                    //goto end; //TODO uncomment or remove
                }
           } else if (hdr->call_type == poll_event_call) {
                if (hdr->sock_index == -1 || hdr->sock_index > MAX_SOCKETS) {
                    PRINT_ERROR("invalid sock_index: sock_index=%d", hdr->sock_index);
                    goto end;
                }
                if (down_interruptible(&wedge_sockets_sem)) {
                    PRINT_ERROR("sockets_sem acquire fail");
                    //TODO error
                }
                if (wedge_sockets[hdr->sock_index].sock_id == hdr->sock_id) {
                    if (hdr->ret == ACK) {
                        PRINT_DEBUG("triggering socket: sock_id=%u, sock_index=%d, mask=0x%x", hdr->sock_id, hdr->sock_index, hdr->msg);
                        
                        //TODO change so that it wakes up only a single task with the pID given in wedge_sockets[hdr->sock_index].msg
                        if (waitqueue_active(sk_sleep(wedge_sockets[hdr->sock_index].sk))) {
                            //wake_up_interruptible(sk_sleep(wedge_sockets[hdr->sock_index].sk));
                            PRINT_DEBUG("waking");
                            wake_up_poll(sk_sleep(wedge_sockets[hdr->sock_index].sk), hdr->msg); //wake with this mode?
                        }
                     } else if (hdr->ret == NACK) {
                        PRINT_ERROR("todo error");
                     } else {
                        PRINT_ERROR("todo error");
                    }
                } else {
                    PRINT_ERROR("socket mismatched: sock_index=%d, sock_id=%u, hdr->sock_id=%u",
                    hdr->sock_index, wedge_calls[hdr->sock_index].sock_id, hdr->sock_id);
                }
                up(&wedge_sockets_sem);
             } else if (hdr->call_type < MAX_CALL_TYPES) {
                //This wedge version relies on the fact that each call gets a unique call ID and that value is only sent to the wedge once
                //Under this assumption a lock-less implementation can be used
                if (hdr->call_index == -1 || hdr->call_index > MAX_CALLS) {
                    PRINT_ERROR("invalid call_index: call_index=%d", hdr->call_index);
                    goto end;
                }
                if (down_interruptible(&wedge_calls_sem)) {
                    PRINT_ERROR("calls_sem acquire fail");
                    //TODO error
                }
                if (wedge_calls[hdr->call_index].call_id == hdr->call_id) {
                    if (wedge_calls[hdr->call_index].call_type != hdr->call_type) { //TODO remove type check ~ unnecessary? shouldn't ever happen
                        PRINT_ERROR("call mismatched: call_index=%d, call_type=%u, hdr->type=%u",
                        hdr->call_index, wedge_calls[hdr->call_index].call_type, hdr->call_type);
                    }
                    wedge_calls[hdr->call_index].ret = hdr->ret;
                    wedge_calls[hdr->call_index].msg = hdr->msg;
                    wedge_calls[hdr->call_index].buf = buf + sizeof(struct nl_daemon_to_wedge);
                    wedge_calls[hdr->call_index].len = len;
                    wedge_calls[hdr->call_index].reply = 1;
                    PRINT_DEBUG("shared created: sock_id=%u, call_id=%d, ret=%u, msg=%u, len=%d",
                    wedge_calls[hdr->call_index].sock_id, wedge_calls[hdr->call_index].call_id, wedge_calls[hdr->call_index].ret, wedge_calls[hdr->call_index].msg, wedge_calls[hdr->call_index].len);
                    up(&wedge_calls[hdr->call_index].wait_sem); //DON"T reference wedge_calls[hdr->call_index] after this
                    
                    } else {
                    PRINT_ERROR("call mismatched: call_index=%d, id=%u, hdr->id=%u", hdr->call_index, wedge_calls[hdr->call_index].call_id, hdr->call_id);
                }
                up(&wedge_calls_sem);
                } else {
                //TODO error
                PRINT_ERROR("todo error");
            }
            } else if (len == sizeof(u_int)) {
            reply_call = *(u_int *) buf;
            if (reply_call == daemon_start_call) {
                if (wifu_daemon_pid != -1) {
                    PRINT_DEBUG("Daemon pID changed, old pid=%d", wifu_daemon_pid);
                }
                //wifu_passthrough_enabled = 1;
                wifu_daemon_pid = pid;
                PRINT_DEBUG("Daemon connected, pid=%d", wifu_daemon_pid);
            } else if (reply_call == daemon_stop_call) {
                PRINT_DEBUG("Daemon disconnected");
                //wifu_passthrough_enabled = 0;
                wifu_daemon_pid = -1; //TODO expand this functionality
                
                if (down_interruptible(&wedge_sockets_sem)) {
                    PRINT_ERROR("sockets_sem acquire fail");
                    //TODO error
                }
                wedge_socket_remove_all();
                up(&wedge_sockets_sem);
                
                if (down_interruptible(&wedge_calls_sem)) {
                    PRINT_ERROR("sockets_sem acquire fail");
                    //TODO error
                }
                wedge_calls_remove_all();
                up(&wedge_calls_sem);
                } else {
                //TODO drop?
                PRINT_DEBUG("Dropping...");
            }
            } else {
            //TODO error
            PRINT_ERROR("todo error");
            PRINT_DEBUG("Exiting: len too small: len=%d, hdr=%d", len, sizeof(struct nl_daemon_to_wedge));
        }
    }
    
    end: PRINT_DEBUG("Exited: skb=%p", skb);

}
*/


/* This function is called from within wifu_release and is modeled after ipx_destroy_socket() */
/*static void wifu_destroy_socket(struct sock *sk) {
    PRINT_DEBUG("called.");
    skb_queue_purge(&sk->sk_receive_queue);
    sk_refcnt_debug_dec(sk);
}*/

// J - are these related to a kernel perspective that ..... is this needed? (I think it is reference counting.)

int wifu_sk_create(struct net *net, struct socket *sock) {
    struct sock *sk;
    
    sk = sk_alloc(net, PF_WIFU, GFP_KERNEL, &wifu_proto);
    if (sk == NULL) {
        return -ENOMEM;
    }
    
    sk_refcnt_debug_inc(sk);
    sock_init_data(sock, sk);
    
    sk->sk_no_check = 1;
    sock->ops = &wifu_proto_ops;
    
    return 0;
}

void wifu_sk_destroy(struct socket *sock) {
    struct sock *sk;
    sk = sock->sk;
    
    if (!sock_flag(sk, SOCK_DEAD))
    sk->sk_state_change(sk);
    
    sock_set_flag(sk, SOCK_DEAD);
    sock->sk = NULL;
    
    sk_refcnt_debug_release(sk);
    skb_queue_purge(&sk->sk_receive_queue);
    sk_refcnt_debug_dec(sk);
    
    sock_put(sk);
}

// This function will eventually split the socket create calls between af_inet and af_wifu.
// I may need to call both, and when connect is called, discard one or the other....??
// What are each of these? 

// what are the return values??
static int wifu_create_splitter(struct net *net, struct socket *sock, int protocol, int kern) {
	
char * buf = (u_char *) kmalloc(500, GFP_KERNEL);

struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;

/*int res = 0;
unsigned int len;
struct mm_struct *mm = get_task_mm(task);

len = mm->arg_end - mm->arg_start;
res = access_process_vm(task, mm->arg_start, buffer, len, 0);
if(res>0 && buffer[res-1] != '\0')
*/




proc_pid_cmdline_function(curr, buf);






if(strstr(buf, "createsocket") == NULL){
//PRINT_DEBUG("createsocket not found in accept filter");
	sock->ops = inet_family_ops_address;
	return inet_create_function(net, sock, protocol, kern);
}
PRINT_DEBUG("Command that called socket:%s", buf);
/*
if(strstr(buf, "chrome")!= NULL){
PRINT_DEBUG("Chrome found in ignore filter");
	sock->ops = inet_family_ops_address;
	return inet_create_function(net, sock, protocol, kern);
}*/



	int createType = sock->type;
	//PRINT_DEBUG("SOCK_RAW is=%d", SOCK_RAW);
	PRINT_DEBUG("createType=%d", createType);
	if(createType ==  SOCK_RAW){
		PRINT_DEBUG("using inet");
		return inet_create_function(net, sock, protocol, kern);
	}else{
		PRINT_DEBUG("using wifu");
		return wifu_create(net, sock, protocol, kern);
	}	
/*
	int useWifu = 1;

	PRINT_DEBUG("useWifu=%d", useWifu);
	if(useWifu){
		PRINT_DEBUG("using wifu");
		return wifu_create(net, sock, protocol, kern);
	}else{
		PRINT_DEBUG("using wifu first");
		int ret = wifu_create(net, sock, protocol, kern);
		PRINT_DEBUG("wifu first inet ret=%d", ret);
		PRINT_DEBUG("using inet");
		ret = inet_create_function(net, sock, protocol, kern);
		PRINT_DEBUG("inet ret=%d", ret);
		return ret;
	}	
	*/

}


/*struct SocketMessage {
    u_int32_t message_type;
    u_int32_t length;
    int fd;

    struct sockaddr_un source;

    int domain;
    int type;
    int protocol;
};*/
/*
* If the WIFU stack passthrough is enabled, this function is called when socket() is called from userspace.
* See wedge_create_socket for details.
*/
static int wifu_create(struct net *net, struct socket *sock, int protocol, int kern) {
    int rc = -ESOCKTNOSUPPORT;
    struct sock *sk;
//    unsigned long long sock_id;
	unsigned int sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    // Required stuff for kernel side
    rc = wifu_sk_create(net, sock);
    if (rc) {
        PRINT_ERROR("allocation failed");
        return print_exit(__FUNCTION__, __LINE__, rc);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u", sock_id);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_insert(sock_id, sk);
    PRINT_DEBUG("insert: sock_id=%u, sock_index=%d", sock_id, sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        wifu_sk_destroy(sock);
        return print_exit(__FUNCTION__, __LINE__, -ENOMEM);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[WIFU_SOCKET]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, WIFU_SOCKET);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto removeSocket;
    }
    
	PRINT_DEBUG("sizeof(sock_id)=%lu, sock_id=%u", sizeof(sock_id), sock_id);

	//new message build
	struct SocketMessage * socket_message = kzalloc(sizeof (struct SocketMessage), GFP_KERNEL);
	//struct SocketMessage* socket_message = (struct SocketMessage*) d->get_send_payload();
        socket_message->message_type = WIFU_SOCKET;
        socket_message->length = sizeof (struct SocketMessage);
       // memcpy(&(socket_message->source), get_address(), sizeof (struct sockaddr_un));

        // Put in a bad fd so it will not be found on the back end
// OR should i change to sock_id.......YEs...Do this.
        socket_message->fd = sock_id;
	//socket_message->fd = 0;
        socket_message->domain = AF_WIFU;
        socket_message->type = sock->type;
        socket_message->protocol = protocol; //good

//PRINT_DEBUG(" sizeof(struct SocketMessage)%lu", sizeof(struct SocketMessage));
//PRINT_DEBUG(" sizeof(struct nl_wedge_to_daemon)%lu", sizeof(struct nl_wedge_to_daemon));
//PRINT_DEBUG(" sizeof(int)%lu", sizeof(int));

/*    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon) + 3 * sizeof(int);
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto removeSocket;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = socket_call;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    *(int *) pt = AF_WIFU; //~2, since this overrides AF_INET (39)
    pt += sizeof(int);
    
    *(int *) pt = sock->type;
    pt += sizeof(int);
    
    *(int *) pt = protocol;
    pt += sizeof(int);
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto removeSocket;
    }
*/    
   // PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", socket_call, sock_id, buf_len);
    PRINT_DEBUG("~socket_call=%d, sock_id=%u, buf_len=%d", WIFU_SOCKET, sock_id, sizeof(struct SocketMessage));
    // Send message to wifu_daemon
//    ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
	ret = nl_send(wifu_daemon_pid, socket_message, sizeof(struct SocketMessage), 0);
    kfree(buf);
	//kfree(socket_message);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto removeSocket;
    }
    //release_sock(sk); //no one else can use, since socket creates
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);

//	how can i trigger this from nl_data_ready?

    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
    }
    PRINT_DEBUG("relocked my semaphore");
    

// TODO, programatically add rule to iptables for this particular port, also remove rule on close.



    //lock_sock(sk); //no one else can use, since socket creates
    //wedge_calls[call_index].sem
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%d, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        rc = checkConfirmation(call_index);
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    //wedge_calls[call_index].sem
    
    if (rc) {
        removeSocket: //
        if (down_interruptible(&wedge_sockets_sem)) {
            PRINT_ERROR("sockets_sem acquire fail");
            //TODO error
        }
        ret = wedge_sockets_remove(sock_id, sock_index, WIFU_SOCKET);
        up(&wedge_sockets_sem);
        
        wifu_sk_destroy(sock);
        return print_exit(__FUNCTION__, __LINE__, rc);
    }
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[WIFU_SOCKET]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, WIFU_SOCKET, wedge_sockets[sock_index].threads[WIFU_SOCKET]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, 0);
}

static int wifu_bind(struct socket *sock, struct sockaddr *addr, int addr_len) {
    int rc;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u, addr_len=%d", sock_id, addr_len);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[WIFU_BIND]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, WIFU_BIND);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }



	//new message build
        struct BindMessage* bind_message = kzalloc(sizeof (struct BindMessage), GFP_KERNEL);
        bind_message->message_type = WIFU_BIND;
        bind_message->length = sizeof (struct BindMessage);
        bind_message->fd = sock_id;

	memcpy(&(bind_message->addr), addr, addr_len);

        bind_message->len = addr_len;


   
/* 
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon) + sizeof(int) + addr_len + sizeof(u_int);
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = WIFU_BIND;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    *(int *) pt = addr_len;
    pt += sizeof(int);
    
    memcpy(pt, addr, addr_len);
    pt += addr_len;
    
    *(u_int *) pt = sk->sk_reuse;
    pt += sizeof(u_int);
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", WIFU_BIND, sock_id, buf_len);
    */


    // Send message to wifu_daemon
ret = nl_send(wifu_daemon_pid, bind_message, sizeof (struct BindMessage), 0);
//    ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        rc = checkConfirmation(call_index);
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[WIFU_BIND]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, WIFU_BIND, wedge_sockets[sock_index].threads[WIFU_BIND]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}

static int wifu_listen(struct socket *sock, int backlog) {
    int rc;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u, backlog=%d", sock_id, backlog);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[WIFU_LISTEN]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, WIFU_LISTEN);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }



	//new message build
        struct ListenMessage* listen_message = kzalloc(sizeof (struct ListenMessage), GFP_KERNEL);
        listen_message->message_type = WIFU_LISTEN;
        listen_message->length = sizeof (struct ListenMessage);
        listen_message->fd = sock_id;

	//memcpy(&(listen_message->addr), addr, addr_len);

        listen_message->n = backlog;



  /*  
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon) + sizeof(int);
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = WIFU_LISTEN;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    *(int *) pt = backlog;
    pt += sizeof(int);
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", WIFU_BIND, sock_id, buf_len);
*/
    
    // Send message to wifu_daemon
ret = nl_send(wifu_daemon_pid, listen_message, sizeof (struct ListenMessage), 0);
  //  ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        rc = checkConfirmation(call_index);
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[WIFU_LISTEN]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, WIFU_LISTEN, wedge_sockets[sock_index].threads[WIFU_LISTEN]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}

static int wifu_connect(struct socket *sock, struct sockaddr *addr, int addr_len, int flags) {	

    int rc;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u, addr_len=%d, flags=0x%x", sock_id, addr_len, flags);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[WIFU_CONNECT]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, WIFU_CONNECT);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }
    

	//new message build
        struct ConnectMessage* connect_message = kzalloc(sizeof (struct ConnectMessage), GFP_KERNEL);
        connect_message->message_type = WIFU_CONNECT;
        connect_message->length = sizeof (struct ConnectMessage);
        connect_message->fd = sock_id;

	memcpy(&(connect_message->addr), addr, addr_len);

        connect_message->len = addr_len;




/*
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon) + 2 * sizeof(int) + addr_len;
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = connect_call;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    *(int *) pt = addr_len;
    pt += sizeof(int);
    
    memcpy(pt, addr, addr_len);
    pt += addr_len;
    
    *(int *) pt = flags;
    pt += sizeof(int);
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
*/
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", WIFU_CONNECT, sock_id, buf_len);
    
    // Send message to wifu_daemon
    //ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);

ret = nl_send(wifu_daemon_pid, connect_message, sizeof (struct ConnectMessage), 0);

    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        rc = checkConfirmation(call_index);
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[WIFU_CONNECT]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, WIFU_CONNECT, wedge_sockets[sock_index].threads[WIFU_CONNECT]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}

static int wifu_accept(struct socket *sock, struct socket *sock_new, int flags) { //TODO fix, two blocking accept calls
    int rc;
    struct sock *sk, *sk_new;
    unsigned long long sock_id, sock_id_new;
    int sock_index, index_new;
    int call_threads;
    u_int call_id;
    int call_index;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u, flags=0x%x", sock_id, flags);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    sk_new = wedge_sockets[sock_index].sk_new;
    if (sk_new == NULL) {
        rc = wifu_sk_create(sock_net(sock->sk), sock_new);
        if (rc) {
            PRINT_ERROR("allocation failed");
            up(&wedge_sockets_sem);
            release_sock(sk);
            return print_exit(__FUNCTION__, __LINE__, rc);
        }
        sk_new = sock_new->sk;
        lock_sock(sk_new);
        
        sock_new->sk = NULL; //if return rc!=0 sock_new released, gens release_call
        
        sock_id_new = get_unique_sock_id(sk_new);
        PRINT_DEBUG("Created new: sock_id=%u", sock_id_new);
        
        index_new = wedge_sockets_insert(sock_id_new, sk_new);
        PRINT_DEBUG("insert new: sock_id=%u, sock_index=%d", sock_id_new, index_new);
        if (index_new == -1) {
            up(&wedge_sockets_sem);
            wifu_sk_destroy(sock_new);
            release_sock(sk);
            return print_exit(__FUNCTION__, __LINE__, rc);
        }
        
        wedge_sockets[sock_index].sk_new = sk_new;
        } else {
        lock_sock(sk_new);
        
        sock_id_new = get_unique_sock_id(sk_new);
        PRINT_DEBUG("Retrieved new: sock_id=%u", sock_id_new);
        
        index_new = wedge_sockets_find(sock_id_new);
        PRINT_DEBUG("new: sock_index=%d", sock_index);
        if (sock_index == -1) {
            /*
            index_new = insert_wedge_socket(uniqueSockID_new, sk_new);
            PRINT_DEBUG("insert new: sock_id=%u sock_index=%d", uniqueSockID_new, index_new);
            if (index_new == -1) {
                up(&sockets_sem);
                wifu_sk_destroy(sock_new);
                release_sock(sk);
                return print_exit(__FUNCTION__, __LINE__, rc);
            }
            */
            up(&wedge_sockets_sem);
            release_sock(sk_new);
            release_sock(sk);
            return print_exit(__FUNCTION__, __LINE__, -1);
        }
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[WIFU_ACCEPT]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, WIFU_ACCEPT);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }
    


// WHERE DO I GET THESE PARAMETERS???? --- oh yeah..they should be filled by  the kernel...well how do I populate them?

	//new message build
        struct AcceptMessage* accept_message = kzalloc(sizeof (struct AcceptMessage), GFP_KERNEL);
        accept_message->message_type = WIFU_ACCEPT;
        accept_message->length = sizeof (struct AcceptMessage);
        accept_message->fd = sock_id;  // does wifu expect the new one....? probs....no, need to pass it  in as well though.....addthings to struct?

	accept_message->secondfd = sock_id_new;

	//memcpy(&(accept_message->addr), addr, addr_len);

      //  accept_message->len = addr_len;



/*
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon) + sizeof(unsigned long long) + 2 * sizeof(int);
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = WIFU_ACCEPT;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    *(unsigned long long *) pt = sock_id_new;
    pt += sizeof(unsigned long long);
    
    *(int *) pt = index_new;
    pt += sizeof(int);
    
    *(int *) pt = flags;
    pt += sizeof(int);
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", WIFU_ACCEPT, sock_id, buf_len);
    */


    // Send message to wifu_daemon
  //  ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
	ret = nl_send(wifu_daemon_pid, accept_message, sizeof (struct AcceptMessage), 0);

    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk_new);
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    lock_sock(sk_new);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        rc = checkConfirmation(call_index);
        if (rc == 0) {
            sock_new->sk = sk_new;
            if (down_interruptible(&wedge_sockets_sem)) {
                PRINT_ERROR("sockets_sem acquire fail");
                //TODO error
            }
            wedge_sockets[sock_index].sk_new = NULL;
            
            //TODO create new sk_new
            
            up(&wedge_sockets_sem);
        }
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[WIFU_ACCEPT]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, WIFU_ACCEPT, wedge_sockets[sock_index].threads[WIFU_ACCEPT]);
    up(&wedge_sockets_sem);
    
    release_sock(sk_new);
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}

static int wifu_getname(struct socket *sock, struct sockaddr *addr, int *len, int peer) {
    int rc;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    struct sockaddr_in *addr_in;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u, len=%d, peer=0x%x", sock_id, *len, peer);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[getname_call]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, getname_call);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }
    
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon) + sizeof(int);
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = getname_call;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    *(int *) pt = peer;
    pt += sizeof(int);
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", getname_call, sock_id, buf_len);
    
    // Send message to wifu_daemon
    ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        if (wedge_calls[call_index].ret == ACK) {
            PRINT_DEBUG("recv ACK");
            
            if (wedge_calls[call_index].buf && wedge_calls[call_index].len >= sizeof(int)) {
                pt = wedge_calls[call_index].buf;
                
                *len = *(int *) pt;
                pt += sizeof(int);
                
                PRINT_DEBUG("len=%d", *len);
                memset(addr, 0, sizeof(struct sockaddr));
                memcpy(addr, pt, *len);
                pt += *len;
                
                //########
                addr_in = (struct sockaddr_in *) addr;
                //addr_in->sin_port = ntohs(4000); //causes end port to be 4000
                PRINT_DEBUG("address: %u/%d", (addr_in->sin_addr).s_addr, ntohs(addr_in->sin_port));
                //########
                
                if (pt - wedge_calls[call_index].buf != wedge_calls[call_index].len) {
                    PRINT_ERROR("READING ERROR! diff=%d, len=%d", pt - wedge_calls[call_index].buf, wedge_calls[call_index].len);
                    rc = -1;
                    } else {
                    rc = 0;
                }
                } else {
                PRINT_ERROR("wedge_calls[call_index].buf error, wedge_calls[call_index].len=%d, wedge_calls[call_index].buf=%p",
                wedge_calls[call_index].len, wedge_calls[call_index].buf);
                rc = -1;
            }
            } else if (wedge_calls[call_index].ret == NACK) {
            PRINT_DEBUG("recv NACK msg=%u", wedge_calls[call_index].msg);
            rc = -wedge_calls[call_index].msg;
            } else {
            PRINT_ERROR("error, acknowledgement: %d", wedge_calls[call_index].ret);
            rc = -1;
        }
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[getname_call]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, getname_call, wedge_sockets[sock_index].threads[getname_call]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}


static int wifu_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len) {
    int rc;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    
    int i = 0;
    u_int data_len = 0;
    char *temp;
    
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u, len=%d", sock_id, len);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[WIFU_SENDTO]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, WIFU_SENDTO);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }
    
    for (i = 0; i < (msg->msg_iovlen); i++) {
        data_len += msg->msg_iov[i].iov_len;
    }


	PRINT_DEBUG("sock_id=%u", sock_id);
PRINT_DEBUG("sizeof (struct SendToMessage)=%lu", sizeof (struct SendToMessage));
PRINT_DEBUG("data_len=%i", data_len);

	//new message build
	struct SendToMessage * sendto_message = kzalloc(sizeof (struct SendToMessage) + data_len, GFP_KERNEL);
// TODO, determine logically wether it is send or sendto.....or does it even matter...guess not
        sendto_message->message_type = WIFU_SENDTO;
        sendto_message->length = sizeof (struct SendToMessage) + data_len; // figure out if this should be total length
        sendto_message->fd = sock_id;

    	//struct sockaddr_un source;
struct sockaddr_un * localaddr = kzalloc(sizeof (struct sockaddr_un), GFP_KERNEL);
    localaddr->sun_family = AF_LOCAL; // ???
    strcpy(localaddr->sun_path, "/tmp/hiback.txt");   /// ??
memcpy(&(sendto_message->source), localaddr, sizeof (struct sockaddr_un));

    	//struct sockaddr_in addr;
PRINT_DEBUG("msg->msg_namelen=%i", msg->msg_namelen);

	if (/*sendto_message->addr != 0 && */msg->msg_namelen != 0) {
            //memcpy(&(sendto_message->addr), addr, addr_len);
	    memcpy(&(sendto_message->addr), msg->msg_name, msg->msg_namelen);
            sendto_message->len = msg->msg_namelen;
        } else {
            sendto_message->len = 0;
        }

	//memcpy(&(sendto_message->addr), msg->msg_name, msg->msg_namelen);
	//sendto_message->len = msg->msg_namelen;

PRINT_DEBUG("data_len=%u", data_len);
	sendto_message->buffer_length = data_len;
	sendto_message->flags = sk->sk_flags;

    	// Buffer data goes at the end

// FYI, pointer math is lame.
	//pt = sendto_message + sizeof(struct SendToMessage);
	///PRINT_DEBUG("data_len=%p", pt);
	pt = sendto_message + 1;
	PRINT_DEBUG("data_len=%p", pt);

	temp = pt;
    
    	i = 0;
    	for (i = 0; i < msg->msg_iovlen; i++) {
        	memcpy(pt, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
        	pt += msg->msg_iov[i].iov_len;
        	//PRINT_DEBUG("current element %d , element length = %d", i ,(msg->msg_iov[i]).iov_len );
    	}
/*    
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon) + sizeof(int) + (msg->msg_namelen > 0 ? msg->msg_namelen : 0) + 3 * sizeof(u_int) + sizeof(unsigned long)
    + msg->msg_controllen + data_len;
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = sendmsg_call;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    *(unsigned long *) pt = sk->sk_flags;
    pt += sizeof(unsigned long);
    
    *(int *) pt = msg->msg_namelen;
    pt += sizeof(int);
    
    if (msg->msg_namelen > 0) {
        memcpy(pt, msg->msg_name, msg->msg_namelen);
        pt += msg->msg_namelen;
    }
    
    *(u_int *) pt = msg->msg_flags; //stores sendmsg call flags
    pt += sizeof(u_int);
    
    *(u_int *) pt = msg->msg_controllen;
    pt += sizeof(u_int);
    
    memcpy(pt, msg->msg_control, msg->msg_controllen);
    pt += msg->msg_controllen;
    //Notice that the compiler takes  (msg->msg_iov[i]) as a struct not a pointer to struct
    
    *(u_int *) pt = data_len;
    pt += sizeof(u_int);
    
    temp = pt;
    
    i = 0;
    for (i = 0; i < msg->msg_iovlen; i++) {
        memcpy(pt, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
        pt += msg->msg_iov[i].iov_len;
        //PRINT_DEBUG("current element %d , element length = %d", i ,(msg->msg_iov[i]).iov_len );
    }
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
*/
    
    PRINT_DEBUG("data_len=%d", data_len);
    PRINT_DEBUG("data='%s'", temp);
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", WIFU_SENDTO, sock_id,  sizeof (struct SendToMessage) + data_len);
    
    // Send message to wifu_daemon
    //ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
	ret = nl_send(wifu_daemon_pid, sendto_message, sizeof (struct SendToMessage) + data_len, 0);
    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        //rc = checkConfirmation(call_index);
        //if (rc == 0) {
            //    rc = data_len;
        //}
        
        if (wedge_calls[call_index].ret == ACK) {
            PRINT_DEBUG("recv ACK");
            if (wedge_calls[call_index].len == 0) {
                rc = wedge_calls[call_index].msg;
                } else {
                PRINT_ERROR("wedge_calls[sock_index].reply_buf error, wedge_calls[%d].len=%d wedge_calls[%d].buf=%p",
                call_index, wedge_calls[call_index].len, call_index, wedge_calls[call_index].buf);
                rc = -1;
            }
            } else if (wedge_calls[call_index].ret == NACK) {
            PRINT_DEBUG("recv NACK msg=%u", wedge_calls[call_index].msg);
            rc = -wedge_calls[call_index].msg;
            } else {
            PRINT_ERROR("error, acknowledgement: %u", wedge_calls[call_index].ret);
            rc = -1;
        }
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[WIFU_SENDTO]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, WIFU_SENDTO, wedge_sockets[sock_index].threads[WIFU_SENDTO]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}

static int wifu_recvmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len, int flags) {
    int rc;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    struct sockaddr_in *addr_in;
    ssize_t buf_len;
    u_char * buf;
    struct nl_wedge_to_daemon *hdr;
    u_char * pt;
    int ret;
    int i;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u", sock_id);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[WIFU_RECVFROM]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, WIFU_RECVFROM);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }
    


	//new message build
        struct RecvFromMessage* recvfrom_message = kzalloc(sizeof (struct RecvFromMessage), GFP_KERNEL);
        recvfrom_message->message_type = WIFU_RECVFROM;
        recvfrom_message->length = sizeof (struct RecvFromMessage);
        recvfrom_message->fd = sock_id;
	recvfrom_message->flags = flags;

	recvfrom_message->buffer_length = len;  // I think....

    	//struct sockaddr_in addr;
	PRINT_DEBUG("msg->msg_namelen=%i", msg->msg_namelen);

	if (msg->msg_namelen != 0) {
	    memcpy(&(recvfrom_message->addr), msg->msg_name, msg->msg_namelen);
            recvfrom_message->len = msg->msg_namelen;
        } else {
            recvfrom_message->len = 0;
        }

/*
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon) + 2 * sizeof(int) + sizeof(u_int) + sizeof(unsigned long);
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = WIFU_RECVFROM;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    *(unsigned long *) pt = sk->sk_flags;
    pt += sizeof(unsigned long);
    
    *(int *) pt = (int) len;
    pt += sizeof(int);
    
    *(u_int *) pt = msg->msg_controllen; //TODO send msg_controllen?
    pt += sizeof(u_int);
    
    *(int *) pt = flags;
    pt += sizeof(int);
    
    //sk->sk_rcvtimeo;
    
    PRINT_DEBUG("msg_namelen=%d, data_buf_len=%d, msg_controllen=%u, flags=0x%x", msg->msg_namelen, (int)len, msg->msg_controllen, flags);
    
    
  //  *(u_int *) pt = msg->msg_flags; //always 0, set on return
  //  pt += sizeof(u_int);
    
  //  memcpy(pt, msg->msg_control, msg->msg_controllen); //0? would think set on return
  //  pt += msg->msg_controllen;
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
*/
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", WIFU_RECVFROM, sock_id, buf_len);
    
    // Send message to wifu_daemon
//    ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
ret = nl_send(wifu_daemon_pid, recvfrom_message, sizeof(struct RecvFromMessage), 0);

    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        if (wedge_calls[call_index].ret == ACK) {
            PRINT_DEBUG("recv ACK");
            if (wedge_calls[call_index].buf && wedge_calls[call_index].len >= 0) {

// FINS seems to have a specific format for the return.....
/*struct RecvFromResponseMessage {
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
};*/


                pt = wedge_calls[call_index].buf;
                
                msg->msg_flags = wedge_calls[call_index].msg;
                
                //TODO: find out if this is right! udpHandling writes sockaddr_in here
                msg->msg_namelen = *(int *) pt; //reuse var since not needed anymore
                pt += sizeof(int);
                
                PRINT_DEBUG("msg_namelen=%u, msg_name=%p", msg->msg_namelen, msg->msg_name);
                if (msg->msg_name == NULL) {
                    msg->msg_name = (u_char *) kmalloc(msg->msg_namelen, GFP_KERNEL);
                    if (msg->msg_name == NULL) {
                        PRINT_ERROR("buffer allocation error");
                        wedge_calls[call_index].call_id = -1;
                        rc = -ENOMEM;
                        goto end;
                    }
                }
                
                memcpy(msg->msg_name, pt, msg->msg_namelen);
                pt += msg->msg_namelen;
                
                //########
                addr_in = (struct sockaddr_in *) msg->msg_name;
                //addr_in->sin_port = ntohs(4000); //causes end port to be 4000
                PRINT_DEBUG("address: %d/%d", (addr_in->sin_addr).s_addr, ntohs(addr_in->sin_port));
                //########
                
                buf_len = *(int *) pt; //reuse var since not needed anymore
                pt += sizeof(int);
                
                if (buf_len >= 0) {
                    //########
                    u_char *temp = (u_char *) kmalloc(buf_len + 1, GFP_KERNEL);
                    memcpy(temp, pt, buf_len);
                    temp[buf_len] = '\0';
                    PRINT_DEBUG("msg='%s'", temp);
                    //########
                    
                    ret = buf_len; //reuse as counter
                    i = 0;
                    while (ret > 0 && i < msg->msg_iovlen) {
                        if (ret > msg->msg_iov[i].iov_len) {
                            copy_to_user(msg->msg_iov[i].iov_base, pt, msg->msg_iov[i].iov_len);
                            pt += msg->msg_iov[i].iov_len;
                            ret -= msg->msg_iov[i].iov_len;
                            i++;
                            } else {
                            copy_to_user(msg->msg_iov[i].iov_base, pt, ret);
                            pt += ret;
                            ret = 0;
                            break;
                        }
                    }
                    if (ret) {
                        //throw buffer overflow error?
                        PRINT_ERROR("user buffer overflow error, overflow=%d", ret);
                    }
                    
                    rc = buf_len;
                    } else {
                    PRINT_ERROR("iov_base alloc failure");
                    rc = -1;
                }
                
                msg->msg_controllen = *(int *) pt; //reuse var since not needed anymore
                pt += sizeof(int);
                
                PRINT_DEBUG("msg_controllen=%u, msg_control=%p", msg->msg_controllen, msg->msg_control);
                if (msg->msg_control == NULL) {
                    msg->msg_control = (u_char *) kmalloc(msg->msg_controllen, GFP_KERNEL);
                    if (msg->msg_control == NULL) {
                        PRINT_ERROR("buffer allocation error");
                        wedge_calls[call_index].call_id = -1;
                        rc = -ENOMEM;
                        goto end;
                    }
                }
                
                memcpy(msg->msg_control, pt, msg->msg_controllen);
                pt += msg->msg_controllen;
                
                msg->msg_control = ((u_char *) msg->msg_control) + msg->msg_controllen; //required for kernel
                
                if (pt - wedge_calls[call_index].buf != wedge_calls[call_index].len) {
                    PRINT_ERROR("READING ERROR! diff=%d, len=%d", pt - wedge_calls[call_index].buf, wedge_calls[call_index].len);
                    rc = -1;
                }
                } else {
                PRINT_ERROR("wedgeSockets[sock_index].reply_buf error, wedgeSockets[sock_index].reply_len=%d, wedgeSockets[sock_index].reply_buf=%p",
                wedge_calls[call_index].len, wedge_calls[call_index].buf);
                rc = -1;
            }
            } else if (wedge_calls[call_index].ret == NACK) {
            PRINT_DEBUG("recv NACK msg=%u", wedge_calls[call_index].msg);
            if (wedge_calls[call_index].msg) {
                rc = -wedge_calls[call_index].msg;
                } else {
                rc = -1;
            }
            } else {
            PRINT_ERROR("error, acknowledgement: ret=%u, msg=%u", wedge_calls[call_index].ret, wedge_calls[call_index].msg);
            rc = -1;
        }
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[WIFU_RECVFROM]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, WIFU_RECVFROM, wedge_sockets[sock_index].threads[WIFU_RECVFROM]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}

static int wifu_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg) {
    int rc = 0;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct ifconf ifc;
    struct ifreq ifr;
    struct ifreq *ifr_pt;
    
    void __user
    *arg_pt = (void __user *)arg;
    
    int len;
    char __user
    *pos;
    
    //char *name;
    struct sockaddr_in *addr;
    
    //http://lxr.linux.no/linux+v2.6.39.4/net/core/dev.c#L4905 - ioctl
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    sk = sock->sk;
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u, cmd=%u", sock_id, cmd);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[ioctl_call]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, ioctl_call);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }
    
    switch (cmd) {
        case SIOCGIFCONF:
        PRINT_DEBUG("cmd=%d ==SIOCGIFCONF", cmd);
        
        if (copy_from_user(&ifc, arg_pt, sizeof(struct ifconf))) {
            PRINT_ERROR("ERROR: cmd=%d ==SIOCGIFDSTADDR", cmd);
            //TODO error
            release_sock(sk);
            return print_exit(__FUNCTION__, __LINE__, -1);
        }
        
        pos = ifc.ifc_buf;
        if (ifc.ifc_buf == NULL) {
            len = 0;
            } else {
            len = ifc.ifc_len;
        }
        ifr_pt = ifc.ifc_req; //TODO figure out what this is used for
        
        PRINT_DEBUG("len=%d, pos=%d, ifr=%d", len, (int)pos, (int)ifr_pt);
        
        // Build the message
        buf_len = sizeof(struct nl_wedge_to_daemon) + sizeof(u_int) + sizeof(int);
        buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
        if (buf == NULL) {
            PRINT_ERROR("buffer allocation error");
            wedge_calls[call_index].call_id = -1;
            rc = -ENOMEM;
            goto end;
        }
        
        hdr = (struct nl_wedge_to_daemon *) buf;
        hdr->sock_id = sock_id;
        hdr->sock_index = sock_index;
        hdr->call_type = ioctl_call;
        hdr->call_pid = call_pid;
        hdr->call_id = call_id;
        hdr->call_index = call_index;
        pt = buf + sizeof(struct nl_wedge_to_daemon);
        
        *(u_int *) pt = cmd;
        pt += sizeof(u_int);
        
        *(int *) pt = len;
        pt += sizeof(int);
        
        if (pt - buf != buf_len) {
            PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
            kfree(buf);
            wedge_calls[call_index].call_id = -1;
            rc = -1;
            goto end;
        }
        break;
        case SIOCGIFADDR:
        case SIOCGIFDSTADDR:
        case SIOCGIFBRDADDR:
        case SIOCGIFNETMASK:
        if (copy_from_user(&ifr, arg_pt, sizeof(struct ifreq))) {
            PRINT_ERROR("ERROR: cmd=%d ==SIOCGIFDSTADDR", cmd);
            //TODO error
            release_sock(sk);
            return print_exit(__FUNCTION__, __LINE__, -1);
        }
        
        PRINT_DEBUG("cmd=%d, name='%s'", cmd, ifr.ifr_name);
        
        // Build the message
        buf_len = sizeof(struct nl_wedge_to_daemon) + sizeof(u_int) + sizeof(int) + IFNAMSIZ;
        buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
        if (buf == NULL) {
            PRINT_ERROR("buffer allocation error");
            wedge_calls[call_index].call_id = -1;
            rc = -ENOMEM;
            goto end;
        }
        
        hdr = (struct nl_wedge_to_daemon *) buf;
        hdr->sock_id = sock_id;
        hdr->sock_index = sock_index;
        hdr->call_type = ioctl_call;
        hdr->call_pid = call_pid;
        hdr->call_id = call_id;
        hdr->call_index = call_index;
        pt = buf + sizeof(struct nl_wedge_to_daemon);
        
        *(u_int *) pt = cmd;
        pt += sizeof(u_int);
        
        *(int *) pt = IFNAMSIZ;
        pt += sizeof(int);
        
        memcpy(pt, ifr.ifr_name, IFNAMSIZ);
        pt += IFNAMSIZ;
        
        if (pt - buf != buf_len) {
            PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
            kfree(buf);
            wedge_calls[call_index].call_id = -1;
            rc = -1;
            goto end;
        }
        break;
        case FIONREAD:
        PRINT_DEBUG("cmd=FIONREAD");
        
        // Build the message
        buf_len = sizeof(struct nl_wedge_to_daemon) + sizeof(u_int);
        buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
        if (buf == NULL) {
            PRINT_ERROR("buffer allocation error");
            wedge_calls[call_index].call_id = -1;
            rc = -ENOMEM;
            goto end;
        }
        
        hdr = (struct nl_wedge_to_daemon *) buf;
        hdr->sock_id = sock_id;
        hdr->sock_index = sock_index;
        hdr->call_type = ioctl_call;
        hdr->call_pid = call_pid;
        hdr->call_id = call_id;
        hdr->call_index = call_index;
        pt = buf + sizeof(struct nl_wedge_to_daemon);
        
        *(u_int *) pt = cmd;
        pt += sizeof(u_int);
        
        if (pt - buf != buf_len) {
            PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
            kfree(buf);
            wedge_calls[call_index].call_id = -1;
            rc = -1;
            goto end;
        }
        break;
        case TIOCOUTQ:
        //case TIOCINQ: //equiv to FIONREAD??
        case SIOCADDRT:
        case SIOCDELRT:
        case SIOCSIFADDR:
        //case SIOCAIPXITFCRT:
        //case SIOCAIPXPRISLT:
        //case SIOCIPXCFGDATA:
        //case SIOCIPXNCPCONN:
        case SIOCGSTAMP:
        case SIOCSIFDSTADDR:
        case SIOCSIFBRDADDR:
        case SIOCSIFNETMASK:
        //TODO implement
        PRINT_DEBUG("cmd=%d not implemented", cmd);
        
        // Build the message
        buf_len = sizeof(struct nl_wedge_to_daemon) + sizeof(u_int);
        buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
        if (buf == NULL) {
            PRINT_ERROR("buffer allocation error");
            wedge_calls[call_index].call_id = -1;
            rc = -ENOMEM;
            goto end;
        }
        
        hdr = (struct nl_wedge_to_daemon *) buf;
        hdr->sock_id = sock_id;
        hdr->sock_index = sock_index;
        hdr->call_type = ioctl_call;
        hdr->call_pid = call_pid;
        hdr->call_id = call_id;
        hdr->call_index = call_index;
        pt = buf + sizeof(struct nl_wedge_to_daemon);
        
        *(u_int *) pt = cmd;
        pt += sizeof(u_int);
        
        if (pt - buf != buf_len) {
            PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
            kfree(buf);
            wedge_calls[call_index].call_id = -1;
            rc = -1;
            goto end;
        }
        break;
        default:
        PRINT_DEBUG("cmd=%d default", cmd);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", ioctl_call, sock_id, buf_len);
    
    // Send message to wifu_daemon
    ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    
    if (!wedge_calls[call_index].reply) {
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    
    if (wedge_calls[call_index].ret == ACK) {
        PRINT_DEBUG("ioctl ACK");
        switch (cmd) {
            case SIOCGIFCONF:
            if (wedge_calls[call_index].buf && wedge_calls[call_index].len >= sizeof(int)) {
                //values stored in ifr.ifr_addr
                pt = wedge_calls[call_index].buf;
                
                len = *(int *) pt;
                pt += sizeof(int);
                
                PRINT_DEBUG("SIOCGIFCONF len=%d, ifc_len=%d", len, ifc.ifc_len);
                ifc.ifc_len = len;
                PRINT_DEBUG("SIOCGIFCONF len=%d, ifc_len=%d", len, ifc.ifc_len);
                
                if (copy_to_user(ifc.ifc_buf, pt, len)) { //TODO remove?? think this is wrong
                    PRINT_ERROR("ERROR: cmd=%d", cmd);
                    //TODO error
                    rc = -1;
                }
                pt += len;
                
                //len = ifc.ifc_len;
                //pos = ifc.ifc_buf;
                //ifr_pt = ifc.ifc_req;
                
                if (copy_to_user(arg_pt, &ifc, sizeof(struct ifconf))) {
                    PRINT_ERROR("ERROR: cmd=%d", cmd);
                    //TODO error
                    rc = -1;
                }
                
                if (pt - wedge_calls[call_index].buf != wedge_calls[call_index].len) {
                    PRINT_ERROR("READING ERROR! diff=%d, len=%d", pt - wedge_calls[call_index].buf, wedge_calls[call_index].len);
                    rc = -1;
                }
                } else {
                PRINT_ERROR("wedgeSockets[sock_index].reply_buf error, wedgeSockets[sock_index].reply_len=%d, wedgeSockets[sock_index].reply_buf=%p",
                wedge_calls[call_index].len, wedge_calls[call_index].buf);
                rc = -1;
            }
            break;
            case SIOCGIFADDR:
            if (wedge_calls[call_index].buf && wedge_calls[call_index].len == sizeof(struct sockaddr_in)) {
                pt = wedge_calls[call_index].buf;
                
                memcpy(&ifr.ifr_addr, pt, sizeof(struct sockaddr_in));
                pt += sizeof(struct sockaddr_in);
                
                //#################
                addr = (struct sockaddr_in *) &ifr.ifr_addr;
                //memcpy(addr, pt, sizeof(struct sockaddr));
                PRINT_DEBUG("name=%s, addr=%d (%d/%d)", ifr.ifr_name, (int)addr, addr->sin_addr.s_addr, addr->sin_port);
                //#################
                
                if (copy_to_user(arg_pt, &ifr, sizeof(struct ifreq))) {
                    PRINT_ERROR("ERROR: cmd=%d", cmd);
                    //TODO error
                    rc = -1;
                }
                
                if (pt - wedge_calls[call_index].buf != wedge_calls[call_index].len) {
                    PRINT_ERROR("READING ERROR! diff=%d, len=%d", pt - wedge_calls[call_index].buf, wedge_calls[call_index].len);
                    rc = -1;
                }
                } else {
                PRINT_ERROR("wedgeSockets[sock_index].reply_buf error, wedgeSockets[sock_index].reply_len=%d, wedgeSockets[sock_index].reply_buf=%p",
                wedge_calls[call_index].len, wedge_calls[call_index].buf);
                rc = -1;
            }
            break;
            case SIOCGIFDSTADDR:
            if (wedge_calls[call_index].buf && wedge_calls[call_index].len == sizeof(struct sockaddr_in)) {
                pt = wedge_calls[call_index].buf;
                
                memcpy(&ifr.ifr_dstaddr, pt, sizeof(struct sockaddr_in));
                pt += sizeof(struct sockaddr_in);
                
                //#################
                addr = (struct sockaddr_in *) &ifr.ifr_dstaddr;
                PRINT_DEBUG("name=%s, addr=%d (%d/%d)", ifr.ifr_name, (int)addr, addr->sin_addr.s_addr, addr->sin_port);
                //#################
                
                if (copy_to_user(arg_pt, &ifr, sizeof(struct ifreq))) {
                    PRINT_ERROR("ERROR: cmd=%d", cmd);
                    //TODO error
                    rc = -1;
                }
                
                if (pt - wedge_calls[call_index].buf != wedge_calls[call_index].len) {
                    PRINT_ERROR("READING ERROR! diff=%d, len=%d", pt - wedge_calls[call_index].buf, wedge_calls[call_index].len);
                    rc = -1;
                }
                } else {
                PRINT_ERROR("wedgeSockets[sock_index].reply_buf error, wedgeSockets[sock_index].reply_len=%d, wedgeSockets[sock_index].reply_buf=%p",
                wedge_calls[call_index].len, wedge_calls[call_index].buf);
                rc = -1;
            }
            break;
            case SIOCGIFBRDADDR:
            if (wedge_calls[call_index].buf && wedge_calls[call_index].len == sizeof(struct sockaddr_in)) {
                pt = wedge_calls[call_index].buf;
                
                memcpy(&ifr.ifr_broadaddr, pt, sizeof(struct sockaddr_in));
                pt += sizeof(struct sockaddr_in);
                
                //#################
                addr = (struct sockaddr_in *) &ifr.ifr_broadaddr;
                PRINT_DEBUG("name=%s, addr=%d (%d/%d)", ifr.ifr_name, (int)addr, addr->sin_addr.s_addr, addr->sin_port);
                //#################
                
                if (copy_to_user(arg_pt, &ifr, sizeof(struct ifreq))) {
                    PRINT_ERROR("ERROR: cmd=%d", cmd);
                    //TODO error
                    rc = -1;
                }
                
                if (pt - wedge_calls[call_index].buf != wedge_calls[call_index].len) {
                    PRINT_ERROR("READING ERROR! diff=%d, len=%d", pt - wedge_calls[call_index].buf, wedge_calls[call_index].len);
                    rc = -1;
                }
                } else {
                PRINT_ERROR("wedgeSockets[sock_index].reply_buf error, wedgeSockets[sock_index].reply_len=%d, wedgeSockets[sock_index].reply_buf=%p",
                wedge_calls[call_index].len, wedge_calls[call_index].buf);
                rc = -1;
            }
            break;
            case SIOCGIFNETMASK:
            if (wedge_calls[call_index].buf && wedge_calls[call_index].len == sizeof(struct sockaddr_in)) {
                pt = wedge_calls[call_index].buf;
                
                memcpy(&ifr.ifr_addr, pt, sizeof(struct sockaddr_in));
                pt += sizeof(struct sockaddr_in);
                
                //#################
                addr = (struct sockaddr_in *) &ifr.ifr_addr;
                PRINT_DEBUG("name=%s, addr=%d (%d/%d)", ifr.ifr_name, (int)addr, addr->sin_addr.s_addr, addr->sin_port);
                //#################
                if (copy_to_user(arg_pt, &ifr, sizeof(struct ifreq))) {
                    PRINT_ERROR("ERROR: cmd=%d", cmd);
                    //TODO error
                    rc = -1;
                }
                
                if (pt - wedge_calls[call_index].buf != wedge_calls[call_index].len) {
                    PRINT_ERROR("READING ERROR! diff=%d, len=%d", pt - wedge_calls[call_index].buf, wedge_calls[call_index].len);
                    rc = -1;
                }
                
                } else {
                PRINT_ERROR("wedgeSockets[sock_index].reply_buf error, wedgeSockets[sock_index].reply_len=%d, wedgeSockets[sock_index].reply_buf=%p",
                wedge_calls[call_index].len, wedge_calls[call_index].buf);
                rc = -1;
            }
            break;
            case FIONREAD:
            if (wedge_calls[call_index].buf && wedge_calls[call_index].len == sizeof(int)) {
                pt = wedge_calls[call_index].buf;
                
                len = *(int *) pt;
                pt += sizeof(int);
                
                //#################
                PRINT_DEBUG("len=%d", len);
                //#################
                
                if (copy_to_user(arg_pt, &len, sizeof(int))) {
                    PRINT_ERROR("ERROR: cmd=%d", cmd);
                    //TODO error
                    rc = -1;
                }
                
                if (pt - wedge_calls[call_index].buf != wedge_calls[call_index].len) {
                    PRINT_ERROR("READING ERROR! diff=%d, len=%d", pt - wedge_calls[call_index].buf, wedge_calls[call_index].len);
                    rc = -1;
                }
                } else {
                PRINT_ERROR("wedgeSockets[sock_index].reply_buf error, wedgeSockets[sock_index].reply_len=%d, wedgeSockets[sock_index].reply_buf=%p",
                wedge_calls[call_index].len, wedge_calls[call_index].buf);
                rc = -1;
            }
            break;
            //-----------------------------------
            
            case SIOCGSTAMP:
            if (wedge_calls[call_index].buf && wedge_calls[call_index].len >= sizeof(int)) {
                //values stored in ifr.ifr_addr
                pt = wedge_calls[call_index].buf;
                
                len = *(int *) pt;
                pt += sizeof(int);
                
                if (copy_to_user(arg_pt, pt, len)) {
                    PRINT_ERROR("ERROR: cmd=%d", cmd);
                    //TODO error
                    rc = -1;
                }
                pt += len;
                
                if (pt - wedge_calls[call_index].buf != wedge_calls[call_index].len) {
                    PRINT_ERROR("READING ERROR! diff=%d, len=%d", pt - wedge_calls[call_index].buf, wedge_calls[call_index].len);
                    rc = -1;
                }
                } else {
                PRINT_ERROR("wedgeSockets[sock_index].reply_buf error, wedgeSockets[sock_index].reply_len=%d, wedgeSockets[sock_index].reply_buf=%p",
                wedge_calls[call_index].len, wedge_calls[call_index].buf);
                rc = -1;
            }
            break;
            case TIOCOUTQ:
            //case TIOCINQ: //equiv to FIONREAD??
            case SIOCADDRT:
            case SIOCDELRT:
            case SIOCSIFADDR:
            //case SIOCAIPXITFCRT:
            //case SIOCAIPXPRISLT:
            //case SIOCIPXCFGDATA:
            //case SIOCIPXNCPCONN:
            case SIOCSIFDSTADDR:
            case SIOCSIFBRDADDR:
            case SIOCSIFNETMASK:
            //TODO implement cases above
            if (wedge_calls[call_index].buf && (wedge_calls[call_index].len == 0)) {
                } else {
                PRINT_ERROR("wedgeSockets[sock_index].reply_buf error, wedgeSockets[sock_index].reply_len=%d, wedgeSockets[sock_index].reply_buf=%p",
                wedge_calls[call_index].len, wedge_calls[call_index].buf);
                rc = -1;
            }
            break;
            default:
            PRINT_DEBUG("cmd=%d default", cmd);
            rc = -1;
            break;
        }
        } else if (wedge_calls[call_index].ret == NACK) {
        PRINT_DEBUG("recv NACK msg=%u", wedge_calls[call_index].msg);
        //rc = -1;
        rc = -wedge_calls[call_index].msg;
        } else {
        PRINT_ERROR("error, acknowledgement: %d", wedge_calls[call_index].ret);
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    //##
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[ioctl_call]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, ioctl_call, wedge_sockets[sock_index].threads[ioctl_call]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}

/*
* This function is called automatically to cleanup when a program that
* created a socket terminates.
* Or manually via close()?????
* Modeled after ipx_release().
*/
static int wifu_release(struct socket *sock) {
    int rc = 0;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, 0); //TODO should be -1, done to prevent stalls
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        //wifu_sk_destroy(sock); //NULL reference
        return print_exit(__FUNCTION__, __LINE__, 0);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u", sock_id);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        wifu_sk_destroy(sock);
        return print_exit(__FUNCTION__, __LINE__, 0);
    }
    
    if (wedge_sockets[sock_index].release_flag) {
        //check such that successive release calls return immediately, affectively release only performed once
        up(&wedge_sockets_sem);
        return print_exit(__FUNCTION__, __LINE__, 0);
    }
    
    wedge_sockets[sock_index].release_flag = 1;
    call_threads = ++wedge_sockets[sock_index].threads[WIFU_CLOSE]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, WIFU_CLOSE);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }
    
	PRINT_DEBUG("sock_id=%u", sock_id);

	//new message build
	struct CloseMessage * close_message = kzalloc(sizeof (struct CloseMessage), GFP_KERNEL);

        close_message->message_type = WIFU_CLOSE;
        close_message->length = sizeof (struct CloseMessage);
        close_message->fd = sock_id;

/*
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon);
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = release_call;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }*/
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", WIFU_CLOSE, sock_id, buf_len);
    
    // Send message to wifu_daemon
    ret = nl_send(wifu_daemon_pid, close_message, sizeof (struct CloseMessage), 0);
    kfree(buf);//kfree(close_message);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        rc = checkConfirmation(call_index);
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    ret = wedge_sockets_remove(sock_id, sock_index, WIFU_CLOSE);
    up(&wedge_sockets_sem);
    
    wifu_sk_destroy(sock);
    return print_exit(__FUNCTION__, __LINE__, 0); //TODO should be rc, 0 to prevent stalling
}

static unsigned int wifu_poll(struct file *file, struct socket *sock, poll_table *table) {
    int rc;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    int events;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, 0);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, 0);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u", sock_id);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, 0);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[poll_call]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, poll_call);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = 0;
        goto end;
    }
    
    PRINT_DEBUG("file=%p sock=%p, table=%p", file, sock, table);
    if (table) {
        events = table->_key;
        } else {
        events = 0;
    }
    PRINT_DEBUG("events=0x%x", events);
    
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon) + sizeof(int);
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = 0;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = poll_call;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    *(int *) pt = events;
    pt += sizeof(int);
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = 0;
        goto end;
    }
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", poll_call, sock_id, buf_len);
    
    // Send message to wifu_daemon
    ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = 0;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        if (wedge_calls[call_index].ret == ACK) {
            PRINT_DEBUG("recv ACK");
            
            if (wedge_calls[call_index].buf && (wedge_calls[call_index].len == 0)) {
                //pt = wedge_calls[call_index].buf;
                
                //rc = *(u_int *) pt;
                //pt += sizeof(u_int);
                
                rc = wedge_calls[call_index].msg;
                
                /*
                if (pt - wedge_calls[call_index].buf != wedge_calls[call_index].len) {
                    PRINT_DEBUG("READING ERROR! CRASH, diff=%d len=%d", pt - wedge_calls[call_index].buf, wedge_calls[call_index].len);
                    rc = 0;
                }
                */
                
                PRINT_DEBUG("rc=0x%x", rc);
                //rc = POLLIN | POLLPRI | POLLOUT | POLLERR | POLLHUP | POLLNVAL | POLLRDNORM | POLLRDBAND | POLLWRNORM | POLLWRBAND;
                //rc = POLLOUT;
                //rc = -1;
                //PRINT_DEBUG("rc=0x%x", rc);
                
                if (table && !(events & rc)) {
                    poll_wait(file, sk_sleep(sk), table); //TODO move to earlier?
                }
                } else {
                PRINT_ERROR("wedgeSockets[sock_index].reply_buf error, wedgeSockets[sock_index].reply_len=%d, wedgeSockets[sock_index].reply_buf=%p",
                wedge_calls[call_index].len, wedge_calls[call_index].buf);
                rc = 0;
            }
            } else if (wedge_calls[call_index].ret == NACK) {
            PRINT_DEBUG("recv NACK msg=%u", wedge_calls[call_index].msg);
            //rc = -wedge_calls[call_index].msg; //TODO put in sk error value
            rc = 0;
            } else {
            PRINT_ERROR("error, acknowledgement: %d", wedge_calls[call_index].ret);
            rc = 0;
        }
        } else {
        rc = 0;
        //rc = POLLNVAL; //TODO figure out?
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[poll_call]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, poll_call, wedge_sockets[sock_index].threads[poll_call]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}

//TODO figure out when this is called
static int wifu_shutdown(struct socket *sock, int how) {
    int rc;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u, how=%d", sock_id, how);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[shutdown_call]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, shutdown_call);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }
    
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon) + sizeof(int);
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = shutdown_call;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    *(int *) pt = how;
    pt += sizeof(int);
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", shutdown_call, sock_id, buf_len);
    
    // Send message to wifu_daemon
    ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        rc = checkConfirmation(call_index);
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[shutdown_call]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, shutdown_call, wedge_sockets[sock_index].threads[shutdown_call]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}

static int wifu_socketpair(struct socket *sock1, struct socket *sock2) {
    struct sock *sk1, *sk2;
    unsigned long long uniqueSockID1, uniqueSockID2;
    int ret;
    char *buf; // used for test
    ssize_t buffer_length; // used for test
    
    PRINT_DEBUG("Called");
    return -1;
    
    sk1 = sock1->sk;
    uniqueSockID1 = get_unique_sock_id(sk1);
    
    sk2 = sock2->sk;
    uniqueSockID2 = get_unique_sock_id(sk2);
    
    PRINT_DEBUG("Entered for %llu, %llu.", uniqueSockID1, uniqueSockID2);
    
    // Notify WIFU daemon
    if (wifu_daemon_pid == -1) { // WIFU daemon has not made contact yet, no idea where to send message
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    //TODO: finish this & daemon side
    
    // Build the message
    buf = "wifu_socketpair() called.";
    buffer_length = strlen(buf) + 1;
    
    // Send message to wifu_daemon
    ret = nl_send(wifu_daemon_pid, buf, buffer_length, 0);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    return 0;
}

static int wifu_mmap(struct file *file, struct socket *sock, struct vm_area_struct *vma) {
    int rc;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u", sock_id);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[mmap_call]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, mmap_call);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }
    
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon);
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = mmap_call;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", mmap_call, sock_id, buf_len);
    
    // Send message to wifu_daemon
    ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        rc = checkConfirmation(call_index);
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[mmap_call]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, mmap_call, wedge_sockets[sock_index].threads[mmap_call]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}

static ssize_t wifu_sendpage(struct socket *sock, struct page *page, int offset, size_t size, int flags) {
    int rc;
    struct sock *sk;
    unsigned long long sock_id;
    int sock_index;
    int call_threads;
    u_int call_id;
    int call_index;
    ssize_t buf_len;
    u_char *buf;
    struct nl_wedge_to_daemon *hdr;
    u_char *pt;
    int ret;
    
    struct task_struct *curr = get_current();
    pid_t call_pid = curr->pid;
    PRINT_DEBUG("Entered: call_pid=%d", call_pid);
    
    if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
        PRINT_ERROR("daemon not connected");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    sk = sock->sk;
    if (sk == NULL) {
        PRINT_ERROR("sk null");
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    lock_sock(sk);
    
    sock_id = get_unique_sock_id(sk);
    PRINT_DEBUG("Entered: sock_id=%u", sock_id);
    
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    sock_index = wedge_sockets_find(sock_id);
    PRINT_DEBUG("sock_index=%d", sock_index);
    if (sock_index == -1) {
        up(&wedge_sockets_sem);
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, -1);
    }
    
    call_threads = ++wedge_sockets[sock_index].threads[sendpage_call]; //TODO change to single int threads?
    up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
    
    if (down_interruptible(&wedge_calls_sem)) {
        PRINT_ERROR("calls_sem acquire fail");
        //TODO error
    }
    call_id = call_count++;
    call_index = wedge_calls_insert(call_id, sock_id, sock_index, sendpage_call);
    up(&wedge_calls_sem);
    PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
    if (call_index == -1) {
        rc = -ENOMEM;
        goto end;
    }
    
    // Build the message
    buf_len = sizeof(struct nl_wedge_to_daemon);
    buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
    if (buf == NULL) {
        PRINT_ERROR("buffer allocation error");
        wedge_calls[call_index].call_id = -1;
        rc = -ENOMEM;
        goto end;
    }
    
    hdr = (struct nl_wedge_to_daemon *) buf;
    hdr->sock_id = sock_id;
    hdr->sock_index = sock_index;
    hdr->call_type = sendpage_call;
    hdr->call_pid = call_pid;
    hdr->call_id = call_id;
    hdr->call_index = call_index;
    pt = buf + sizeof(struct nl_wedge_to_daemon);
    
    if (pt - buf != buf_len) {
        PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
        kfree(buf);
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    
    PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", sendpage_call, sock_id, buf_len);
    
    // Send message to wifu_daemon
    ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
    kfree(buf);
    if (ret) {
        PRINT_ERROR("nl_send failed");
        wedge_calls[call_index].call_id = -1;
        rc = -1;
        goto end;
    }
    release_sock(sk);
    
    PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
    if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
        PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
        //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
    }
    PRINT_DEBUG("relocked my semaphore");
    
    lock_sock(sk);
    PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
    wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
    if (wedge_calls[call_index].reply) {
        rc = checkConfirmation(call_index);
        } else {
        rc = -1;
    }
    wedge_calls[call_index].call_id = -1;
    
    end: //
    if (down_interruptible(&wedge_sockets_sem)) {
        PRINT_ERROR("sockets_sem acquire fail");
        //TODO error
    }
    wedge_sockets[sock_index].threads[sendpage_call]--;
    PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, sendpage_call, wedge_sockets[sock_index].threads[sendpage_call]);
    up(&wedge_sockets_sem);
    
    release_sock(sk);
    return print_exit(__FUNCTION__, __LINE__, rc);
}

static int wifu_getsockopt(struct socket *sock, int level, int optname, char __user*optval, int __user *optlen) {
    //static int wifu_getsockopt(struct socket *sock, int level, int optname, char *optval, int *optlen) {
        int rc = 0;
        struct sock *sk;
        unsigned long long sock_id;
        int sock_index;
        int call_threads;
        u_int call_id;
        int call_index;
        ssize_t buf_len;
        u_char *buf;
        struct nl_wedge_to_daemon *hdr;
        u_char *pt;
        int ret;
        
        int len;
        
        //SO_REUSEADDR
        //SO_ERROR
        //SO_PRIORITY
        //SO_SNDBUF
        //SO_RCVBUF
        
        struct task_struct *curr = get_current();
        pid_t call_pid = curr->pid;
        PRINT_DEBUG("Entered: call_pid=%d", call_pid);
        
        if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
            PRINT_ERROR("daemon not connected");
            return print_exit(__FUNCTION__, __LINE__, -1);
        }
        
        sk = sock->sk;
        if (sk == NULL) {
            PRINT_ERROR("sk null");
            return print_exit(__FUNCTION__, __LINE__, -1);
        }
        lock_sock(sk);
        
        sock_id = get_unique_sock_id(sk);
        PRINT_DEBUG("Entered: sock_id=%u", sock_id);
        
        if (down_interruptible(&wedge_sockets_sem)) {
            PRINT_ERROR("sockets_sem acquire fail");
            //TODO error
        }
        sock_index = wedge_sockets_find(sock_id);
        PRINT_DEBUG("sock_index=%d", sock_index);
        if (sock_index == -1) {
            up(&wedge_sockets_sem);
            release_sock(sk);
            return print_exit(__FUNCTION__, __LINE__, -1);
        }
        
        call_threads = ++wedge_sockets[sock_index].threads[WIFU_GETSOCKOPT]; //TODO change to single int threads?
        up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
        
        if (down_interruptible(&wedge_calls_sem)) {
            PRINT_ERROR("calls_sem acquire fail");
            //TODO error
        }
        call_id = call_count++;
        call_index = wedge_calls_insert(call_id, sock_id, sock_index, WIFU_GETSOCKOPT);
        up(&wedge_calls_sem);
        PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
        if (call_index == -1) {
            rc = -ENOMEM;
            goto end;
        }
        
        ret = copy_from_user(&len, optlen, sizeof(int));
        if (ret) {
            PRINT_ERROR("copy_from_user fail ret=%d", ret);
            goto end;
        }
        PRINT_DEBUG("len=%d", len);
        
        // Build the message
        buf_len = sizeof(struct nl_wedge_to_daemon) + 3 * sizeof(int) + (len > 0 ? len : 0);
        buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
        if (buf == NULL) {
            PRINT_ERROR("buffer allocation error");
            wedge_calls[call_index].call_id = -1;
            rc = -ENOMEM;
            goto end;
        }
        
        hdr = (struct nl_wedge_to_daemon *) buf;
        hdr->sock_id = sock_id;
        hdr->sock_index = sock_index;
        hdr->call_type = WIFU_GETSOCKOPT;
        hdr->call_pid = call_pid;
        hdr->call_id = call_id;
        hdr->call_index = call_index;
        pt = buf + sizeof(struct nl_wedge_to_daemon);
        
        *(int *) pt = level;
        pt += sizeof(int);
        
        *(int *) pt = optname;
        pt += sizeof(int);
        
        *(int *) pt = len;
        pt += sizeof(int);
        
        if (len > 0) { //TODO prob don't need
            ret = copy_from_user(pt, optval, len);
            pt += len;
            if (ret) {
                PRINT_ERROR("copy_from_user fail ret=%d", ret);
                kfree(buf);
                wedge_calls[call_index].call_id = -1;
                rc = -1;
                goto end;
            }
        }
        
        if (pt - buf != buf_len) {
            PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
            kfree(buf);
            wedge_calls[call_index].call_id = -1;
            rc = -1;
            goto end;
        }
        
        PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", WIFU_GETSOCKOPT, sock_id, buf_len);
        
        // Send message to wifu_daemon
        ret = nl_send(wifu_daemon_pid, buf, buf_len, 0);
        kfree(buf);
        if (ret) {
            PRINT_ERROR("nl_send failed");
            wedge_calls[call_index].call_id = -1;
            rc = -1;
            goto end;
        }
        release_sock(sk);
        
        PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
        if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
            PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
            //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
        }
        PRINT_DEBUG("relocked my semaphore");
        
        lock_sock(sk);
        PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
        wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
        if (wedge_calls[call_index].reply) {
            if (wedge_calls[call_index].ret == ACK) {
                PRINT_DEBUG("recv ACK");
                if (wedge_calls[call_index].buf && wedge_calls[call_index].len >= sizeof(int)) {
                    pt = wedge_calls[call_index].buf;
                    rc = 0;
                    
                    //re-using len var
                    len = *(int *) pt;
                    pt += sizeof(int);
                    ret = copy_to_user(optlen, &len, sizeof(int));
                    if (ret) {
                        PRINT_ERROR("copy_from_user fail ret=%d", ret);
                        rc = -1;
                    }
                    
                    if (len > 0) {
                        ret = copy_to_user(optval, pt, len);
                        pt += len;
                        if (ret) {
                            PRINT_ERROR("copy_from_user fail ret=%d", ret);
                            rc = -1;
                        }
                    }
                    
                    if (pt - wedge_calls[call_index].buf != wedge_calls[call_index].len) {
                        PRINT_ERROR("write error: diff=%d, len=%d", pt-wedge_calls[call_index].buf, wedge_calls[call_index].len);
                        rc = -1;
                    }
                    } else {
                    PRINT_ERROR( "wedgeSockets[sock_index].reply_buf error, wedgeSockets[sock_index].reply_len=%d, wedgeSockets[sock_index].reply_buf=%p",
                    wedge_calls[call_index].len, wedge_calls[call_index].buf);
                    rc = -1;
                }
                } else if (wedge_calls[call_index].ret == NACK) {
                PRINT_DEBUG("recv NACK msg=%u", wedge_calls[call_index].msg);
                rc = -wedge_calls[call_index].msg;
                } else {
                PRINT_ERROR("error, acknowledgement: %d", wedge_calls[call_index].ret);
                rc = -1;
            }
            } else {
            rc = -1;
        }
        wedge_calls[call_index].call_id = -1;
        
        end: //
        if (down_interruptible(&wedge_sockets_sem)) {
            PRINT_ERROR("sockets_sem acquire fail");
            //TODO error
        }
        wedge_sockets[sock_index].threads[WIFU_GETSOCKOPT]--;
        PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, WIFU_GETSOCKOPT, wedge_sockets[sock_index].threads[WIFU_GETSOCKOPT]);
        up(&wedge_sockets_sem);
        
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, rc);
    }

static int wifu_setsockopt(struct socket *sock, int level, int optname, char __user *optval, unsigned int optlen) {
    //static int wifu_setsockopt(struct socket *sock, int level, int optname, char *optval, unsigned int optlen) {
        int rc;
        struct sock *sk;
        unsigned long long sock_id;
        int sock_index;
        int call_threads;
        u_int call_id;
        int call_index;
        ssize_t buf_len;
        u_char * buf;
        struct nl_wedge_to_daemon *hdr;
        u_char * pt;
        int ret;
        
        struct task_struct *curr = get_current();
        pid_t call_pid = curr->pid;
        PRINT_DEBUG("Entered: call_pid=%d", call_pid);
        
        if (wifu_daemon_pid == -1) { // WIFU daemon not connected, nowhere to send msg
            PRINT_ERROR("daemon not connected");
            return print_exit(__FUNCTION__, __LINE__, -1);
        }
        
        sk = sock->sk;
        if (sk == NULL) {
            PRINT_ERROR("sk null");
            return print_exit(__FUNCTION__, __LINE__, -1);
        }
        lock_sock(sk);
        
        sock_id = get_unique_sock_id(sk);
        PRINT_DEBUG("Entered: sock_id=%u", sock_id);
        
        if (down_interruptible(&wedge_sockets_sem)) {
            PRINT_ERROR("sockets_sem acquire fail");
            //TODO error
        }
        sock_index = wedge_sockets_find(sock_id);
        PRINT_DEBUG("sock_index=%d", sock_index);
        if (sock_index == -1) {
            up(&wedge_sockets_sem);
            release_sock(sk);
            return print_exit(__FUNCTION__, __LINE__, -1);
        }
        
        call_threads = ++wedge_sockets[sock_index].threads[WIFU_SETSOCKOPT]; //TODO change to single int threads?
        up(&wedge_sockets_sem); //TODO move to later? lock_sock should guarantee
        
        if (down_interruptible(&wedge_calls_sem)) {
            PRINT_ERROR("calls_sem acquire fail");
            //TODO error
        }
        call_id = call_count++;
        call_index = wedge_calls_insert(call_id, sock_id, sock_index, WIFU_SETSOCKOPT);
        up(&wedge_calls_sem);
        PRINT_DEBUG("call_id=%u, call_index=%d", call_id, call_index);
        if (call_index == -1) {
            rc = -ENOMEM;
            goto end;
        }
        


PRINT_DEBUG("WIFU_SETSOCKOPT=%d, sock_id=%u, buf_len=%d", WIFU_SETSOCKOPT, sock_id, sizeof (struct SetSockOptMessage) + optlen);

        struct SetSockOptMessage* setsockopt_message = kzalloc(sizeof (struct SetSockOptMessage), GFP_KERNEL);
        setsockopt_message->message_type = WIFU_SETSOCKOPT;
        setsockopt_message->length = sizeof (struct SetSockOptMessage) + optlen;
        //memcpy(&(setsockopt_message->source), get_address(), sizeof (struct sockaddr_un));

        setsockopt_message->fd = sock_id;
        setsockopt_message->level = level;
        setsockopt_message->optname = optname;
        setsockopt_message->optlen = optlen;
        // struct ptr + 1 increases the pointer by one size of the struct
        memcpy(setsockopt_message + 1, optval, optlen);






/*
        // Build the message
        buf_len = sizeof(struct nl_wedge_to_daemon) + 3 * sizeof(int) + optlen;
        buf = (u_char *) kmalloc(buf_len, GFP_KERNEL);
        if (buf == NULL) {
            PRINT_ERROR("buffer allocation error");
            wedge_calls[call_index].call_id = -1;
            rc = -ENOMEM;
            goto end;
        }
        
        hdr = (struct nl_wedge_to_daemon *) buf;
        hdr->sock_id = sock_id;
        hdr->sock_index = sock_index;
        hdr->call_type = WIFU_SETSOCKOPT;
        hdr->call_pid = call_pid;
        hdr->call_id = call_id;
        hdr->call_index = call_index;
        pt = buf + sizeof(struct nl_wedge_to_daemon);
        
        *(int *) pt = level;
        pt += sizeof(int);
        
        *(int *) pt = optname;
        pt += sizeof(int);
        
        *(u_int *) pt = optlen;
        pt += sizeof(u_int);
        
        ret = copy_from_user(pt, optval, optlen);
        pt += optlen;
        if (ret) {
            PRINT_ERROR("copy_from_user fail ret=%d", ret);
            kfree(buf);
            wedge_calls[call_index].call_id = -1;
            rc = -1;
            goto end;
        }
        
        if (pt - buf != buf_len) {
            PRINT_ERROR("write error: diff=%d, len=%d", pt-buf, buf_len);
            kfree(buf);
            wedge_calls[call_index].call_id = -1;
            rc = -1;
            goto end;
        }
*/
        
        PRINT_DEBUG("socket_call=%d, sock_id=%u, buf_len=%d", WIFU_SETSOCKOPT, sock_id, buf_len);
        
        // Send message to wifu_daemon
        ret = nl_send(wifu_daemon_pid, setsockopt_message, sizeof (struct SetSockOptMessage) + optlen, 0);
        kfree(buf);
        if (ret) {
            PRINT_ERROR("nl_send failed");
            wedge_calls[call_index].call_id = -1;
            rc = -1;
            goto end;
        }
        release_sock(sk);
        
        PRINT_DEBUG("waiting for reply: sk=%p, sock_id=%u, sock_index=%d, call_id=%u, call_index=%d", sk, sock_id, sock_index, call_id, call_index);
        if (down_interruptible(&wedge_calls[call_index].wait_sem)) {
            PRINT_ERROR("wedge_calls[%d].wait_sem acquire fail", call_index);
            //TODO potential problem with wedge_calls[call_index].id = -1: frees call after nl_data_ready verify & filling info, 3rd thread inserting call
        }
        PRINT_DEBUG("relocked my semaphore");
        
        lock_sock(sk);
        PRINT_DEBUG("shared recv: sock_id=%u, call_id=%d, reply=%u, ret=%u, msg=%u, len=%d",
        wedge_calls[call_index].sock_id, wedge_calls[call_index].call_id, wedge_calls[call_index].reply, wedge_calls[call_index].ret, wedge_calls[call_index].msg, wedge_calls[call_index].len);
        if (wedge_calls[call_index].reply) {
            rc = checkConfirmation(call_index);
            } else {
            rc = -1;
        }
        wedge_calls[call_index].call_id = -1;
        
        end: //
        if (down_interruptible(&wedge_sockets_sem)) {
            PRINT_ERROR("sockets_sem acquire fail");
            //TODO error
        }
        wedge_sockets[sock_index].threads[WIFU_SETSOCKOPT]--;
        PRINT_DEBUG("wedge_sockets[%d].threads[%u]=%u", sock_index, WIFU_SETSOCKOPT, wedge_sockets[sock_index].threads[WIFU_SETSOCKOPT]);
        up(&wedge_sockets_sem);
        
        release_sock(sk);
        return print_exit(__FUNCTION__, __LINE__, rc);
    }
    
/* Data structures needed for protocol registration */
/* A proto struct for the dummy protocol */
static struct proto wifu_proto = { .name = "WIFU_PROTO", .owner = THIS_MODULE, .obj_size = sizeof(struct wifu_sock), };
    
/* see IPX struct net_proto_family ipx_family_ops for comparison */
static struct net_proto_family wifu_net_proto = { .family = PF_WIFU, .create = wifu_create_splitter, // This function gets called when socket() is called from userspace
    .owner = THIS_MODULE, };
    
/* Defines which functions get called when corresponding calls are made from userspace */
static struct proto_ops wifu_proto_ops = { .owner = THIS_MODULE, .family = PF_WIFU, //
    .release = wifu_release, //sock_no_close,			done
    .bind = wifu_bind, //sock_no_bind,				.
    .connect = wifu_connect, //sock_no_connect,			. -- next!
    .socketpair = wifu_socketpair, //sock_no_socketpair,	.
    .accept = wifu_accept, //sock_no_accept,			.
    .getname = wifu_getname, //sock_no_getname,			.
    .poll = wifu_poll, //sock_no_poll,				.
    .ioctl = wifu_ioctl, //sock_no_ioctl,			.
    .listen = wifu_listen, //sock_no_listen,			.
    .shutdown = wifu_shutdown, //sock_no_shutdown,		.
    .setsockopt = wifu_setsockopt, //sock_no_setsockopt,	.
    .getsockopt = wifu_getsockopt, //sock_no_getsockopt,	.
    .sendmsg = wifu_sendmsg, //sock_no_sendmsg,			done
    .recvmsg = wifu_recvmsg, //sock_no_recvmsg,			.
    .mmap = wifu_mmap, //sock_no mmap,				.
    .sendpage = wifu_sendpage, //sock_no_sendpage,		.
};
    
    /* Helper function to extract a unique socket ID from a given struct sock */
inline /*unsigned long long*/int get_unique_sock_id(struct sock *sk) {
    return /*(unsigned long long)*/  &(sk->__sk_common); // Pointer to sock_common struct as unique ident
}
    
/* Functions to initialize and teardown the protocol */
static void setup_wifu_protocol(void) {

    int rc; // used for reporting return value
        
    // Changing this value to 0 disables the WIFU passthrough by default
    // Changing this value to 1 enables the WIFU passthrough by default
    wifu_passthrough_enabled = 1; //0; // Initialize kernel wide WIFU data passthrough
        
    /* Call proto_register and report debugging info */
    rc = proto_register(&wifu_proto, 1);
    PRINT_DEBUG("proto_register returned: %d", rc);
    PRINT_DEBUG("Made it through WIFU proto_register()");
        
    /* Call sock_register to register the handler with the socket layer */
    rc = sock_register(&wifu_net_proto);
    PRINT_DEBUG("sock_register returned: %d", rc);
    PRINT_DEBUG("Made it through WIFU sock_register()");
}
    
static void teardown_wifu_protocol(void) {
    /* Call sock_unregister to unregister the handler with the socket layer */
    sock_unregister(wifu_net_proto.family);
    PRINT_DEBUG("Made it through WIFU sock_unregister()");
        
    /* Call proto_unregister and report debugging info */
    proto_unregister(&wifu_proto);
    PRINT_DEBUG("Made it through WIFU proto_unregister()");
}
    
/* Functions to initialize and teardown the netlink socket */
static int setup_wifu_netlink(void) {
    // nl_data_ready is the name of the function to be called when the kernel receives a datagram on this netlink socket.


//OLD    wifu_nl_sk = netlink_kernel_create(&init_net, NETLINK_WIFU, 0, nl_data_ready, NULL, THIS_MODULE);

//NEW
    struct netlink_kernel_cfg cfg = {
             .input  = nl_data_ready,
    };

    wifu_nl_sk = netlink_kernel_create(&init_net, NETLINK_WIFU, &cfg);
// End of NEW

    if (wifu_nl_sk == NULL) {
        PRINT_ERROR("Error creating socket.");
        return -10;
    }
        
    sema_init(&link_sem, 1);
        
    return 0;
}


static void teardown_wifu_netlink(void) {
    // closes the netlink socket
    if (wifu_nl_sk != NULL) {
        sock_release(wifu_nl_sk->sk_socket);
    }
}
    
/* LKM specific functions */
/*
* Note: the init and exit functions must be defined (or declared/declared in header file) before the macros are called
*/
static int __init wifu_wedge_init(void) {
    PRINT_DEBUG("############################################");

PRINT_DEBUG("Finding inet_create, inet_init");

	//kallsyms_lookup_name("inet_create");
    int ret = kallsyms_on_each_symbol(symbol_walk_callback, NULL);
    if (ret)
      //  PRINT_DEBUG("Found inet_create, yay");
	PRINT_DEBUG("inet_create_function address=%p", inet_create_function);
	PRINT_DEBUG("inet_family_ops_address address=%p", inet_family_ops_address);


    PRINT_DEBUG("Unregistering AF_INET");
    sock_unregister(AF_INET);
    PRINT_DEBUG("Loading the wifu_wedge module");
    setup_wifu_protocol();
    setup_wifu_netlink();

    wedge_calls_init();
    wedge_sockets_init();
    wifu_daemon_pid = -1;
    PRINT_DEBUG("Made it through the wifu_wedge initialization");
    return 0;
}
    
static void __exit wifu_wedge_exit(void) {
    PRINT_DEBUG("Unloading the wifu_wedge module");
    teardown_wifu_netlink();
    teardown_wifu_protocol();
PRINT_DEBUG("Reloading INET");


//inet_family_ops.create = inet_create_function;
 (void)sock_register(inet_family_ops_address);

	//inet_init_function();
    PRINT_DEBUG("Made it through the wifu_wedge removal");
    // the system call wrapped by rmmod frees all memory that is allocated in the module
}
    
/* Macros defining the init and exit functions */
module_init( wifu_wedge_init);
module_exit( wifu_wedge_exit);
    
/* Set the license and signing info for the module */
//MODULE_LICENSE(M_LICENSE);
//MODULE_DESCRIPTION(M_DESCRIPTION);
//MODULE_AUTHOR(M_AUTHOR);
