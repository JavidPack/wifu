#include "states/FinWait1.h"

FinWait1::FinWait1() : State() {

}

FinWait1::~FinWait1() {

}

void FinWait1::state_enter(Context* c) {
}

void FinWait1::state_exit(Context* c) {
}

void FinWait1::close(Context* c) {
    ConnectionManagerContext* cmc = (ConnectionManagerContext*) c;
}

void FinWait1::state_receive_packet(Context* c, QueueProcessor<Event*>* q, NetworkReceivePacketEvent* e) {
    ConnectionManagerContext* cmc = (ConnectionManagerContext*) c;
    TCPPacket* p = (TCPPacket*) e->get_packet();
    Socket* s = e->get_socket();

    if (p->is_tcp_fin() && p->is_tcp_ack()) {
        unsigned char* data = (unsigned char*) "";
        AddressPort* destination = s->get_remote_address_port();
        AddressPort* source = s->get_local_address_port();

        TCPPacket* response = new TCPPacket();
        response->insert_tcp_header_option(new TCPTimestampOption());
        response->set_ip_destination_address_s(destination->get_address());
        response->set_ip_source_address_s(source->get_address());

        response->set_destination_port(destination->get_port());
        response->set_source_port(source->get_port());

        response->set_data(data, 0);

        SendPacketEvent* event = new SendPacketEvent(s, response);
        q->enqueue(event);

        cmc->set_state(new TimeWait());
        
    } else if (p->is_tcp_ack()) {
        cmc->set_state(new FinWait2());
    }
}

bool FinWait1::state_can_receive(Context* c, Socket* s) {
    return true;
}
