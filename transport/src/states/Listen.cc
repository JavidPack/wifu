#include "states/Listen.h"

Listen::Listen() {

}

Listen::~Listen() {

}

void Listen::state_enter(Context* c) {
   
}

void Listen::state_exit(Context* c) {
    
}

void Listen::state_receive_packet(Context* c, QueueProcessor<Event*>* q, NetworkReceivePacketEvent* e) {
    ConnectionManagerContext* cmc = (ConnectionManagerContext*) c;
    TCPPacket* packet = (TCPPacket*) e->get_packet();

    if (packet->is_tcp_syn()) {
        process_syn(c, q, e);
    }
}

void Listen::state_close(Context* c, QueueProcessor<Event*>* q, CloseEvent* e) {
    ConnectionManagerContext* cmc = (ConnectionManagerContext*) c;
    q->enqueue(new DeleteSocketEvent(e->get_socket()));
    cmc->set_state(new Closed());
}

void Listen::process_syn(Context* c, QueueProcessor<Event*>* q, NetworkReceivePacketEvent* e) {
    ConnectionManagerContext* cmc = (ConnectionManagerContext*) c;

    Socket* listening_socket = e->get_socket();
    TCPPacket* packet = (TCPPacket*) e->get_packet();

AddressPort* localNew = new AddressPort(*listening_socket->get_local_address_port());
AddressPort* remoteNew = new AddressPort(*packet->get_source_address_port());

    // TODO: we are reusing the same local address/port (not calling new)
    // This may be okay...but I don't want to think about it now
    Socket* new_socket = new Socket(listening_socket->get_second_socket_id(),
		listening_socket->get_domain(),
            listening_socket->get_type(),
            listening_socket->get_protocol(),
		localNew, remoteNew);
       //     listening_socket->get_local_address_port(),
       //     packet->get_source_address_port());


printf(".............in listen2 %u\n", listening_socket->get_second_socket_id());
printf(".............in listen1 %u\n", listening_socket->get_socket_id());

printf("&&&& - I am fd:%u\n",listening_socket->get_socket_id());

    cmc->set_accept_socket(listening_socket);
    Event* new_connection = new ConnectionInitiatedEvent(listening_socket, new_socket, cmc->get_listen_event());
    SocketCollection::instance().push(new_socket);
    q->enqueue(new_connection);


    unsigned char* data = (unsigned char*) "";
    AddressPort* source = packet->get_dest_address_port();
    AddressPort* destination = packet->get_source_address_port();

    TCPPacket* response = new TCPPacket();
    response->insert_tcp_header_option(new TCPTimestampOption());
    response->set_ip_destination_address_s(destination->get_address());
    response->set_ip_source_address_s(source->get_address());

    response->set_destination_port(destination->get_port());
    response->set_source_port(source->get_port());

    response->set_tcp_syn(true);
    response->set_tcp_ack(true);
    response->set_data(data, 0);

    SendPacketEvent* event = new SendPacketEvent(new_socket, response);
    q->enqueue(event);

    cmc->set_state(new SynReceived());
}
