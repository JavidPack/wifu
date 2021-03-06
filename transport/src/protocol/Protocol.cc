#include "protocol/Protocol.h"
#include "events/framework_events/AddressResponseEvent.h"
#include "events/framework_events/GetSockOptResponseEvent.h"

Protocol::Protocol(int protocol) : Module(new PriorityQueue<Event*, PriorityEventComparator>()), protocol_(protocol) {

}

Protocol::~Protocol() {

}

void Protocol::imodule_library_socket(Event* e) {
    SocketEvent* event = (SocketEvent*) e;

    Socket* s = event->get_socket();
    // This is to filter out bind events which do not correspond to this Protocol
    // TODO: this will not (likely) scale well
    if (s->get_protocol() != protocol_) {
        return;
    }

    sockets_.insert(s);
    icontext_socket(this, event);

    ResponseEvent* response_event = ObjectPool<ResponseEvent>::instance().get();
    response_event->set_socket(s);
    response_event->set_message_type(event->get_message_type());
    response_event->set_fd(event->get_fd());
    response_event->set_return_value(s->get_socket_id());
    response_event->set_errno(0);
    response_event->set_default_length();
    response_event->set_destination(event->get_source());
    dispatch(response_event);
}

void Protocol::imodule_library_bind(Event* e) {
	printf("Protocol::imodule_library_bind(Event* e)\n");
    BindEvent* event = (BindEvent*) e;

    int error = 0;
    int return_val = -1;
    Socket* socket = event->get_socket();

    if (!sockets_.contains(socket)) {
        return;
    }
printf("sockets_.contains(socket)\n");

    // TODO: may not need this check, it is done on the front end.
    // TODO: I'm leaving it for now, just to be safe.
    if (socket != NULL) {
        AddressPort* local = new AddressPort(event->get_addr());

        // TODO: Check possible errors
        AlreadyBoundToAddressPortVisitor v(local);
        SocketCollection::instance().accept(&v);

        if (!v.is_bound()) {
            socket->set_local_address_port(local);
printf(" socket->set_local_address_port(local); %u\n", local->get_port());
            icontext_bind(this, event);
            return_val = 0;
        } else {
            error = EINVAL;
        }

    } else {
        error = EBADF;
    }

    ResponseEvent* response_event = ObjectPool<ResponseEvent>::instance().get();
    response_event->set_socket(socket);
    response_event->set_message_type(event->get_message_type());
    response_event->set_fd(event->get_fd());
    response_event->set_return_value(return_val);
    response_event->set_errno(error);
    response_event->set_default_length();
    response_event->set_destination(event->get_source());
    dispatch(response_event);
}

void Protocol::imodule_library_listen(Event* e) {
    ListenEvent* event = (ListenEvent*) e;

    Socket* socket = event->get_socket();
    if (!sockets_.contains(socket)) {
        return;
    }

    // TODO: Do something with this back log
    int back_log = event->get_back_log();

    int error = 0;
    int return_val = 0;

    // TODO: do we need to check the address and port or just the port?
    AlreadyListeningOnSamePortVisitor v(socket->get_local_address_port()->get_port());
    SocketCollection::instance().accept(&v);

    // TODO: check EOPNOTSUPP:
    // The socket is not of a type that supports  the  listen()  operation.
    // see man 2 listen

    if (v.is_listening()) {
        error = EADDRINUSE;
        return_val = -1;
    } else {
        icontext_listen(this, event);
    }

    ResponseEvent* response_event = ObjectPool<ResponseEvent>::instance().get();
    response_event->set_socket(socket);
    response_event->set_message_type(event->get_message_type());
    response_event->set_fd(event->get_fd());
    response_event->set_return_value(return_val);
    response_event->set_errno(error);
    response_event->set_default_length();
    response_event->set_destination(event->get_source());

    // TODO: we may not need this if we are pushing things into the FSMs
    socket->make_passive();
    dispatch(response_event);
}

void Protocol::imodule_library_connect(Event* e) {
printf("Protocol::imodule_library_connect");
    ConnectEvent* event = (ConnectEvent*) e;

    Socket* socket = event->get_socket();
    if (!sockets_.contains(socket)) {
printf("!sockets_.contains(socket)");
        return;
    }

    // TODO: Error check

    //    cout << "In library connect" << endl;
    icontext_connect(this, event);
}

void Protocol::imodule_library_accept(Event* e) {
    AcceptEvent* event = (AcceptEvent*) e;

    Socket* socket = event->get_socket();
    if (!sockets_.contains(socket)) {
        return;
    }

    // TODO: Error check
    icontext_accept(this, event);
}

void Protocol::imodule_library_receive(Event* e) {
    ReceiveEvent* event = (ReceiveEvent*) e;

    Socket* s = event->get_socket();

    if (!sockets_.contains(s)) {
        return;
    }

    if (!icontext_can_receive(s)) {
        // TODO: respond with error
        return;
    }


    icontext_receive(this, event);
}

void Protocol::imodule_library_send(Event* e) {
printf("Protocol::imodule_library_send(Event* e) called\n");
    SendEvent* event = (SendEvent*) e;
    Socket* socket = event->get_socket();

    if (!sockets_.contains(socket)) {
printf("!sockets_.contains(socket)\n");
        return;
    }

    if (!icontext_can_send(socket)) {
printf("!icontext_can_send(socket)\n");
        // TODO: respond with error ?
        return;
    }

    // call contexts
    icontext_send(this, event);
}

void Protocol::imodule_library_close(Event* e) {
    CloseEvent* event = (CloseEvent*) e;
    if (!sockets_.contains(event->get_socket())) {
        // TODO: return an error?
        return;
    }
    icontext_close(this, event);
}

void Protocol::imodule_send(Event* e) {
    SendPacketEvent* event = (SendPacketEvent*) e;

    Socket* socket = event->get_socket();
    if (!sockets_.contains(socket)) {
        return;
    }

    event->get_packet()->set_ip_protocol(protocol_);

    // TODO: Error check
    icontext_send_packet(this, event);
}

void Protocol::imodule_network_receive(Event* e) {
    NetworkReceivePacketEvent* event = (NetworkReceivePacketEvent*) e;

    Socket* socket = event->get_socket();

    if (!sockets_.contains(socket)) {
        return;
    }

    // TODO: Error check
    // We will potentially save data before we are connected; however, we won't pass data to the application until we are connected.
    icontext_receive_packet(this, event);
}

void Protocol::imodule_connection_established(Event* e) {
    ConnectionEstablishedEvent* event = (ConnectionEstablishedEvent*) e;

    Socket* socket = event->get_socket();

    // This is to filter out bind events which do not correspond to this Protocol
    // TODO: this will not (likely) scale well
    if (socket->get_protocol() != protocol_) {
        return;
    }
    icontext_new_connection_established(this, event);
}

void Protocol::imodule_connection_initiated(Event* e) {
	printf("Protocol::imodule_connection_initiated\n");
    ConnectionInitiatedEvent* event = (ConnectionInitiatedEvent*) e;
    Socket* listening_socket = event->get_socket();
    Socket* new_socket = event->get_new_socket();
printf("event->get_new_socket(); called\n");
    if (new_socket->get_protocol() != protocol_) {
        return;
    }

    sockets_.insert(new_socket);

    // TODO: Error Check: socket(s)

    icontext_new_connection_initiated(this, event);
}

void Protocol::imodule_timer_fired(Event* e) {
    TimerFiredEvent* event = (TimerFiredEvent*) e;
    Socket* socket = event->get_socket();


    if (!sockets_.contains(socket)) {
        return;
    }

    icontext_timer_fired_event(this, event);
}

void Protocol::imodule_resend(Event* e) {
    ResendPacketEvent* event = (ResendPacketEvent*) e;
    Socket* socket = event->get_socket();

    if (!sockets_.contains(socket)) {
        return;
    }

    icontext_resend_packet(this, event);
}

void Protocol::imodule_send_buffer_not_empty(Event* e) {
    SendBufferNotEmptyEvent* event = (SendBufferNotEmptyEvent*) e;
    Socket* socket = event->get_socket();

    if (!sockets_.contains(socket)) {
        return;
    }

    icontext_send_buffer_not_empty(this, event);
}

void Protocol::imodule_send_buffer_not_full(Event* e) {
    SendBufferNotFullEvent* event = (SendBufferNotFullEvent*) e;
    Socket* socket = event->get_socket();

    if (!sockets_.contains(socket)) {
        return;
    }

    icontext_send_buffer_not_full(this, event);
}

void Protocol::imodule_receive_buffer_not_empty(Event* e) {
    ReceiveBufferNotEmptyEvent* event = (ReceiveBufferNotEmptyEvent*) e;
    Socket* socket = event->get_socket();

    if (!sockets_.contains(socket)) {
        return;
    }

    icontext_receive_buffer_not_empty(this, event);
}

void Protocol::imodule_receive_buffer_not_full(Event* e) {
    ReceiveBufferNotFullEvent* event = (ReceiveBufferNotFullEvent*) e;
    Socket* socket = event->get_socket();

    if (!sockets_.contains(socket)) {
        return;
    }

    icontext_receive_buffer_not_full(this, event);
}

void Protocol::imodule_delete_socket(Event* e) {
    DeleteSocketEvent* event = (DeleteSocketEvent*) e;
    Socket* socket = event->get_socket();

    if (!sockets_.contains(socket)) {

        return;
    }
    icontext_delete_socket(this, event);

    sockets_.remove(socket);
    assert(!sockets_.contains(socket));

    SocketCollectionGetByIdVisitor visitor1(socket->get_socket_id());
    SocketCollection::instance().accept(&visitor1);
    assert(visitor1.get_socket() == socket);

    SocketCollection::instance().remove(socket);

    SocketCollectionGetByIdVisitor visitor(socket->get_socket_id());
    SocketCollection::instance().accept(&visitor);
    assert(visitor.get_socket() == NULL);

}

void Protocol::imodule_library_set_socket_option(Event* e) {
    SetSocketOptionEvent* event = (SetSocketOptionEvent*) e;
    Socket* socket = event->get_socket();

    if (!sockets_.contains(socket)) {
        return;
    }

    // TODO: error check
    socket->get_socket_options().insert(event->get_level_name_pair(), event->get_value_length_pair());
    icontext_set_socket_option(this, event);

    ResponseEvent* response_event = ObjectPool<ResponseEvent>::instance().get();
    response_event->set_socket(socket);
    response_event->set_message_type(event->get_message_type());
    response_event->set_fd(event->get_fd());
    response_event->set_return_value(0);
    response_event->set_errno(0);
    response_event->set_default_length();
    response_event->set_destination(event->get_source());
    dispatch(response_event);
}

void Protocol::imodule_library_get_socket_option(Event* e) {
    GetSocketOptionEvent* event = (GetSocketOptionEvent*) e;
    Socket* socket = event->get_socket();

    if (!sockets_.contains(socket)) {
        return;
    }

    pair<gcstring, socklen_t> value = socket->get_socket_options().get(event->get_level_name_pair());

    if (value.first.empty()) {
        // Indicates no option found
        // TODO: error?
    }

    GetSockOptResponseEvent* response_event = (GetSockOptResponseEvent*) ObjectPool<ResponseEvent>::instance().get();
    response_event->set_socket(socket);
    response_event->set_message_type(event->get_message_type());
    response_event->set_fd(event->get_fd());
    response_event->set_return_value(0);
    response_event->set_errno(0);
    response_event->set_length(sizeof(struct GetSockOptResponseMessage));
    response_event->set_destination(event->get_source());
    response_event->set_optval((void*) value.first.data(), value.second);
    dispatch(response_event);

    icontext_get_socket_option(this, event);
}

void Protocol::send_network_packet(Socket* s, WiFuPacket* p) {
    NetworkSendPacketEvent* e = new NetworkSendPacketEvent(s, p);
    Dispatcher::instance().enqueue(e);
}

bool Protocol::should_enqueue(Event* event) {
    return event->get_socket()->get_protocol() == protocol_;
}
