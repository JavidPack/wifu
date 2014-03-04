#include "WiFuTransportBackEndTranslator.h"
#include "MessageStructDefinitions.h"

#define NETLINK_WIFU	29

WiFuTransportBackEndTranslator& WiFuTransportBackEndTranslator::instance() {
    static WiFuTransportBackEndTranslator instance_;
    return instance_;
}

WiFuTransportBackEndTranslator::~WiFuTransportBackEndTranslator() {

}

void WiFuTransportBackEndTranslator::receive(unsigned char* message, int length) {
	perror("Here");
	log_INFORMATIONAL("WiFuTransportBackEndTranslator receive called~!");
	printf("WiFuTransportBackEndTranslator receive called~!\n");
	//std::string sbuffer=reinterpret_cast<const char*>(message);

	//log_INFORMATIONAL("hey", sbuffer);
	printf("hey\n");
printf("lenght %i\n",length);

    struct GenericMessage* gm = (struct GenericMessage*) message;
printf("gm->message_type %i\n",gm->message_type);
printf("gm->fd %i\n",gm->fd);
printf("unsigned gm->fd %u\n",gm->fd);
    LibraryEvent* e = NULL;
    Socket* socket = SocketCollection::instance().get_by_id(gm->fd);

    switch (gm->message_type) {
        case WIFU_RECVFROM:
        case WIFU_RECV:
            e = ObjectPool<ReceiveEvent>::instance().get();
            break;

        case WIFU_SENDTO:
        case WIFU_SEND:
	{
		perror("case WIFU_SENDTO");

            	struct SendToMessage* stm = (struct SendToMessage*) message;

   // struct sockaddr_un source;

   // struct sockaddr_in addr;

		printf("stm->addr->sin_family %i\n",stm->addr.sin_family);
		printf("stm->addr->sin_port %02x\n",stm->addr.sin_port);
		printf("stm->addr->sin_addr %02x\n",stm->addr.sin_addr.s_addr);

		printf("stm->len %i\n",stm->len);
		printf("stm->buffer_length %zu\n",stm->buffer_length);
		printf("stm->flags %i\n",stm->flags);

            	e = ObjectPool<SendEvent>::instance().get();
            	break;
	}
        case WIFU_SOCKET:
        {
		perror("case WIFU_SOCKET");
            struct SocketMessage* sm = (struct SocketMessage*) message;
printf("sm->domain %i\n",sm->domain);
printf("sm->type %i\n",sm->type);
printf("sm->protocol %i\n",sm->protocol);

// Eventually I need to figure out what to do about 0 in protocol feild. what is the default.
	    if(sm->type == SOCK_DGRAM){ // FOR UDP
		sm->protocol = UDP; // from defines.h aka, 217
  	    }
	    else if(sm->type == SOCK_STREAM){ // FOR TCP
	        sm->protocol = TCP_TAHOE; // TCP_TAHOE 207
	    }

printf("sm->protocol %i\n",sm->protocol);
            if (ProtocolManager::instance().is_supported(sm->domain, sm->type, sm->protocol)) {
printf("Sopported protocol case WIFU_SOCKET\n");
                e = ObjectPool<SocketEvent>::instance().get();
                socket = new Socket(sm->fd, sm->domain, sm->type, sm->protocol);
                SocketCollection::instance().push(socket);
            } else {
printf("else case WIFU_SOCKET\n");
                ResponseEvent* response = ObjectPool<ResponseEvent>::instance().get();
                response->set_message_type(sm->message_type);
                response->set_fd(sm->fd);
                response->set_return_value(-1);
                response->set_errno(EPROTONOSUPPORT);
                response->set_default_length();
                response->set_destination(&(sm->source));
                send_to_wedge(/*response->get_destination(),*/ response->get_buffer(), response->get_length());
                ObjectPool<ResponseEvent>::instance().release(response);
                return;
            }
            break;
        }

        case WIFU_BIND:
		perror("case WIFU_BIND");
            e = ObjectPool<BindEvent>::instance().get();
            break;

        case WIFU_LISTEN:
perror("case WIFU_LISTEN");
            e = ObjectPool<ListenEvent>::instance().get();
            break;

        case WIFU_ACCEPT:
	{
	perror("case WIFU_ACCEPT");
            e = ObjectPool<AcceptEvent>::instance().get();

// Set here and use in Listen.cc
	    struct AcceptMessage* am = (struct AcceptMessage*) message;
	    socket->set_second_socket_id(am->secondfd);


            break;
	}
        case WIFU_CONNECT:
perror("case WIFU_CONNECT");
            e = ObjectPool<ConnectEvent>::instance().get();
            break;

        case WIFU_GETSOCKOPT:
            e = ObjectPool<GetSocketOptionEvent>::instance().get();
            break;

        case WIFU_SETSOCKOPT:
            e = ObjectPool<SetSocketOptionEvent>::instance().get();
            break;

        case WIFU_CLOSE:
            e = ObjectPool<CloseEvent>::instance().get();
            break;

        default:
		perror("Unknown message type");
            throw WiFuException("Unknown message type");
    }

    assert(socket);

    if (e) {
printf(" if (e) { reached\n");

//printf("event_map_[socket->get_socket_id()] %p\n", &event_map_[socket->get_socket_id()]);

        event_map_[socket->get_socket_id()] = e;
//printf("event_map_[socket->get_socket_id()] %p\n", &event_map_[socket->get_socket_id()]);


//printf(" event_map_[socket->get_socket_id()] = e; passed\n");
        e->set_socket(socket);
//printf(" e->set_socket(socket); passed\n");
        e->save_buffer(message, length);
printf("   e->save_buffer(message, length); passed\n");
        dispatch(e);
printf(" dispatch(e); passed\n");
    }

}

void WiFuTransportBackEndTranslator::imodule_library_response(Event* e) {

    ResponseEvent* event = (ResponseEvent*) e;
printf(" event->get_socket()->get_socket_id() %u \n",event->get_socket()->get_socket_id());
    event_map_iterator_ = event_map_.find(event->get_socket()->get_socket_id());
    assert(event_map_iterator_ != event_map_.end());

    LibraryEvent* original_event = event_map_iterator_->second;
//printf(" sent %zu \n",sent);
	ssize_t sent = send_to_wedge(event->get_buffer(), event->get_length());
printf(" sent %zu \n",sent);
    //ssize_t sent = send_to(event->get_destination(), event->get_buffer(), event->get_length());
    assert(sent == event->get_length());
printf("switch (original_event->get_message_type %i \n",original_event->get_message_type());
    switch (original_event->get_message_type()) {
        case WIFU_RECVFROM:
        case WIFU_RECV:
            ObjectPool<ReceiveEvent>::instance().release((ReceiveEvent*) original_event);
            break;
        case WIFU_SENDTO:
        case WIFU_SEND:
printf(" imodule lib responce  sendto \n");
            ObjectPool<SendEvent>::instance().release((SendEvent*) original_event);
            break;
        case WIFU_SOCKET:
		printf(" imodule lib responce socket \n");
            ObjectPool<SocketEvent>::instance().release((SocketEvent*) original_event);
            break;
        case WIFU_BIND:
printf(" imodule lib responce WIFU_BIND \n");
            ObjectPool<BindEvent>::instance().release((BindEvent*) original_event);
            break;
        case WIFU_LISTEN:
            ObjectPool<ListenEvent>::instance().release((ListenEvent*) original_event);
            break;
        case WIFU_ACCEPT:
            ObjectPool<AcceptEvent>::instance().release((AcceptEvent*) original_event);
            break;
        case WIFU_CONNECT:
perror("imodule lib responce WIFU_CONNECT");
            ObjectPool<ConnectEvent>::instance().release((ConnectEvent*) original_event);
            break;
        case WIFU_GETSOCKOPT:
            ObjectPool<GetSocketOptionEvent>::instance().release((GetSocketOptionEvent*) original_event);
            break;
        case WIFU_SETSOCKOPT:
	printf(" imodule lib responce  setsockop \n");
            ObjectPool<SetSocketOptionEvent>::instance().release((SetSocketOptionEvent*) original_event);
            break;
        case WIFU_CLOSE:
            ObjectPool<CloseEvent>::instance().release((CloseEvent*) original_event);
            break;
        default:
            throw WiFuException("Unknown message type");
    }
printf("instance().release(event); \n");
    ObjectPool<ResponseEvent>::instance().release(event);
}

WiFuTransportBackEndTranslator::WiFuTransportBackEndTranslator() : LocalSocketFullDuplex("/tmp/WS"), Module() {
    ObjectPool<SocketEvent>::instance();
    ObjectPool<BindEvent>::instance();
    ObjectPool<ListenEvent>::instance();
    ObjectPool<AcceptEvent>::instance();
    ObjectPool<SendEvent>::instance();
    ObjectPool<ReceiveEvent>::instance();
    ObjectPool<ConnectEvent>::instance();
    ObjectPool<GetSocketOptionEvent>::instance();
    ObjectPool<SetSocketOptionEvent>::instance();
    ObjectPool<CloseEvent>::instance();

    ObjectPool<ResponseEvent>::instance();

    log_INFORMATIONAL("WiFuTransportBackEndTranslator Netlink setup");
	printf("Is this working?\n");

    log_INFORMATIONAL("WiFuTransportBackEndTranslator Created");

    log_INFORMATIONAL("attempting priming!");

    prime_wedge();

//send_to(response->get_destination(), response->get_buffer(), response->get_length());


}
