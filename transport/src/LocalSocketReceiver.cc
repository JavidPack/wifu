#include "LocalSocketReceiver.h"
#include "Utils.h"
#include "MessageStructDefinitions.h"


//#define MAX_PAYLOAD 1024
//#define NETLINK_MESSAGE "This message originated from the kernel!"

#define NETLINK_WIFU	29  // 29 wifu, 31 is test app

// Should these be higher up?
struct nlmsghdr *nlh = NULL;  // Free after each use.
struct iovec iov;             // iov.base and length to be updated each time.
struct msghdr msg;            // should be ok just one initialize.

struct nlmsghdr *nlh2 = NULL;  // Free after each use.
struct iovec iov2;             // iov.base and length to be updated each time.
struct msghdr msg2;            // should be ok just one initialize.

LocalSocketReceiver::LocalSocketReceiver(gcstring& file, LocalSocketReceiverCallback* callback) : file_(file), callback_(callback) {
    init();
}

LocalSocketReceiver::LocalSocketReceiver(const char* file, LocalSocketReceiverCallback* callback) : file_(file), callback_(callback) {
    init();
}

LocalSocketReceiver::~LocalSocketReceiver() {
perror("destructing LocalSocketReceiver");
    close(socket_);
    unlink(file_.c_str());
    pthread_cancel(thread_);
}

int LocalSocketReceiver::get_socket() {
    return socket_;
}

size_t showbytes(const void *object, size_t size)
{
	size_t oldsize = size;
   const unsigned char *byte;
   for ( byte = (const unsigned char *)object; size--; ++byte )
   {
      printf("%02X", *byte);
   }
   putchar('\n');
	return oldsize;
}

int LocalSocketReceiver::prime_wedge() {
	// Wedge expects one int sized message for control messages...for now we can emulate this for simplicities sake.
    printf("prime_wedge() called\n");

	int daemoncode = 19;//daemon_start_call;
	int dataLength =  sizeof(int);

	nlh2 = (struct nlmsghdr *)malloc(NLMSG_SPACE(dataLength));
	memset(nlh2, 0, NLMSG_SPACE(dataLength));
// Javid-- Changed from Space to Length...should be correct.
	nlh2->nlmsg_len = NLMSG_LENGTH(dataLength);  
	nlh2->nlmsg_pid = getpid();
	nlh2->nlmsg_flags = 0;

	memcpy(NLMSG_DATA(nlh2),(uint8_t *) &daemoncode, dataLength);

	iov2.iov_base = (void *)nlh2;
	iov2.iov_len = nlh2->nlmsg_len;

	msg2.msg_name = (void *)&kernel_sockaddress;
	msg2.msg_namelen = sizeof(kernel_sockaddress);
	msg2.msg_iov = &iov2;
	msg2.msg_iovlen = 1;

printf("nlh2->nlmsg_len %i\n",nlh2->nlmsg_len);

	printf("Sending Priming message to kernel\n");
	int ret = sendmsg(socket_,&msg2,0);
	if(ret == -1){
		perror("Error LocalSocketSender::send_to");
	}

	free(nlh2); // Free buffer.

    return 0;
}

gcstring& LocalSocketReceiver::get_file() {
    return file_;
}

LocalSocketReceiverCallback* LocalSocketReceiver::get_callback() const {
    return callback_;
}

struct sockaddr_un* LocalSocketReceiver::get_address() {
    return &server_;
}

ssize_t LocalSocketReceiver::send_to_wedge(void* buffer, size_t length) {
	printf("LocalSocketReceiver::send_to_wedge\n");
	int ret;
showbytes(buffer , length);
    struct GenericMessage* gm = (struct GenericMessage*) buffer;
printf("@gm->message_type %i\n",gm->message_type);
printf("@gm->length %i\n",gm->length);
printf("@unsigned gm->fd %u\n",gm->fd);
if(gm->message_type == 1){
 	struct SocketResponseMessage* srm = (struct SocketResponseMessage*) buffer;
	printf("srm->return_value %i\n",srm->return_value);
	printf("srm->error %i\n",srm->error);
}




	int dataLength =  length;

	nlh2 = (struct nlmsghdr *)malloc(NLMSG_SPACE(dataLength));
	memset(nlh2, 0, NLMSG_SPACE(dataLength));
	nlh2->nlmsg_len = NLMSG_LENGTH(dataLength);  
	nlh2->nlmsg_pid = getpid();
	nlh2->nlmsg_flags = 0;

	memcpy(NLMSG_DATA(nlh2),(uint8_t *) buffer, dataLength);

	iov2.iov_base = (void *)nlh2;
	iov2.iov_len = nlh2->nlmsg_len;

	msg2.msg_name = (void *)&kernel_sockaddress;
	msg2.msg_namelen = sizeof(kernel_sockaddress);
	msg2.msg_iov = &iov2;
	msg2.msg_iovlen = 1;

printf("nlh2->nlmsg_len %i\n",nlh2->nlmsg_len);

	ret = sendmsg(socket_,&msg2,0);
	if(ret == -1){
		perror("Error LocalSocketSender::send_to");
	}

	free(nlh2); // Free buffer.

	return length;


/*
        ret = sendto(socket_, buffer, length, 0, (struct sockaddr*) &kernel_sockaddress, sizeof(struct sockaddr));//SUN_LEN(kernel_sockaddress));
	if(ret == -1){
		perror("Error LocalSocketSender::send_to");
	}
	return ret;*/
}

void LocalSocketReceiver::init(void) {

    sem_.init(1);

    
/*
    // setup socket address structure
    memset(&server_, 0, sizeof (server_));
    server_.sun_family = AF_LOCAL;
    strcpy(server_.sun_path, file_.c_str());
*/


    // create socket
    socket_ = socket(AF_NETLINK, SOCK_RAW, NETLINK_WIFU);//socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (socket_ == -1) {
        perror("Error creating SOCK_RAW socket");
        exit(-1);
    }
    unlink(file_.c_str());

// Are these necessary for netlink??
   /* int optval = 1;
    int value = setsockopt(get_socket(), SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
    if (value) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    optval = UNIX_SOCKET_MAX_BUFFER_SIZE;
    value = setsockopt(get_socket(), SOL_SOCKET, SO_RCVBUF, &optval, sizeof (optval));
    if (value) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }*/


	// Populate local_sockaddress
	memset(&local_sockaddress, 0, sizeof(struct sockaddr_nl));
	local_sockaddress.nl_family = AF_NETLINK;
	local_sockaddress.nl_pad = 0;
	local_sockaddress.nl_pid = getpid(); //pthread_self() << 16 | getpid(),	// use second option for multi-threaded process
	local_sockaddress.nl_groups = 0; // unicast

	// Bind the local netlink socket
	if (bind(get_socket(), (struct sockaddr*) &local_sockaddress, sizeof(struct sockaddr_nl) ) < 0) {
        	perror("BindReceiver");
		printf("%i",getpid());
		printf("%i",get_socket());
        	exit(-1);
    	}

printf("init hehegetpid() is %i\n",getpid());







	memset(&kernel_sockaddress, 0, sizeof(kernel_sockaddress));
	kernel_sockaddress.nl_family = AF_NETLINK;
	kernel_sockaddress.nl_pid = 0; // For Linux Kernel 
	kernel_sockaddress.nl_groups = 0; // unicast 

	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_LENGTH(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	//unsigned char * bob = "nobo";

	/*struct GenericMessage * message = (GenericMessage *)malloc(sizeof(GenericMessage));
	message->message_type=1;
	message->length=16;
	message->fd=32;

	memcpy(NLMSG_DATA(nlh),message, sizeof(GenericMessage));*/

	//strcpy((char *)NLMSG_DATA(nlh), "HELLO HELLO HELLO");
	//strcpy(((void*)(((char*)nlh) + NLMSG_LENGTH(0))), bob);
	//std::string s5 ("Another character sequence");
	//memcpy(NLMSG_DATA(nlh), my_list, sizeof my_list).

	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&kernel_sockaddress;
	msg.msg_namelen = sizeof(kernel_sockaddress);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	//printf("Sending message to kernel\n");
	//sendmsg(socket_,&msg,0);
	//printf("Waiting for message from kernel\n");

//// TEST





/*
    if (bind(get_socket(), (const struct sockaddr *) & server_, SUN_LEN(&server_)) < 0) {
        perror("Bind");
        exit(-1);
    }*/

    struct local_socket_receiver_obj obj;
    obj.sock = get_socket();
    obj.receiver = this;
    obj.sem.init(0);

    if (pthread_create(&thread_, NULL, &unix_receive_handler, &obj) != 0) {
        perror("Error creating new thread");
        exit(EXIT_FAILURE);
    }
printf(" obj.sem.wait start\n");
    obj.sem.wait();
printf(" obj.sem.wait stop\n");
}



void* unix_receive_handler(void* arg) {
    struct local_socket_receiver_obj* obj = (struct local_socket_receiver_obj*) arg;

    LocalSocketReceiverCallback* receiver = obj->receiver->get_callback();
    int socket = obj->sock;

    obj->sem.post();

   // unsigned char buf[UNIX_SOCKET_MAX_BUFFER_SIZE];

    int nread;
    while (1) {
	printf("while 1 start:\n");
	nread = recvmsg(socket, &msg, 0);
       // nread = recv(socket, buf, UNIX_SOCKET_MAX_BUFFER_SIZE, 0);
        if (nread < 0) {
            if (errno == EINTR)
                continue;
            else
                break;
        } else if (nread == 0) {
            // the socket is closed
            break;
        }

	//int msg_size=strlen((char *)NLMSG_DATA(nlh));
	printf("nlh %p\n",nlh);
	nlh = (struct nlmsghdr *)msg.msg_iov->iov_base;
	printf("nlh %p\n",nlh);	

printf("nlh->nlmsg_len %lu\n",msg.msg_iov->iov_len);	
printf("nlh->nlssssmsg_len %i\n",nlh->nlmsg_len);
//showbytes(NLMSG_DATA(nlh) , nlh->nlmsg_len - 16);

printf("msg.msg_iov->iov_base %p\n",msg.msg_iov->iov_base);	

	//memcpy(buf, NLMSG_DATA(nlh), nlh->nlmsg_len);	

//nlh->nlmsg_len = nread; // why do I have to do this? is this set upwrong?

	printf("Received message payload: %s\n", (char *)NLMSG_DATA(nlh));

	showbytes((unsigned char*)NLMSG_DATA(nlh) , NLMSG_PAYLOAD(nlh , 0));
	

	printf("nread %i\n",nread);
	//printf("msg.msg_iov->iov_len %lu\n",msg.msg_iov->iov_len);
	

printf("NLMSG_PAYLOAD %i\n",NLMSG_PAYLOAD(nlh , 0));

	//getchar();
	//printf("msg_size %i\n",msg_size);
	//printf("%s",buf);
printf("abouttocallreceive\n");
        receiver->receive((unsigned char*)NLMSG_DATA(nlh), NLMSG_PAYLOAD(nlh , 0));
	//receiver->receive((unsigned char*)"hello hello hello", msg_size);
	printf("aftercallreceive\n");
    }
}


/////  nlmsghdr 16 | struct | mesg | padding
/////       16        160      5  181     184   
