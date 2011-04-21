#include "states/FinWait1.h"

FinWait1::FinWait1() : State() {

}

FinWait1::~FinWait1() {

}

void FinWait1::enter(Context* c) {
    cout << "FinWait1::enter()" << endl;
}

void FinWait1::exit(Context* c) {
    cout << "FinWait1::exit()" << endl;
}

void FinWait1::close(Context* c) {
    ConnectionManagerContext* cmc = (ConnectionManagerContext*) c;
}

void FinWait1::receive_packet(Context* c, NetworkReceivePacketEvent* e) {
    cout << "FinWait1::receive_packet()" << endl;
    ConnectionManagerContext* cmc = (ConnectionManagerContext*) c;
    TCPPacket* p = (TCPPacket*) e->get_packet();
    Socket* s = e->get_socket();

    if (p->is_tcp_fin()) {
        cout << "FinWait1::receive_packet(), FIN" << endl;
        unsigned char* data = (unsigned char*) "";
        AddressPort* destination = s->get_remote_address_port();
        AddressPort* source = s->get_local_address_port();

        TCPPacket* response = new TCPPacket();
        response->set_ip_protocol(SIMPLE_TCP);
        response->set_ip_destination_address_s(destination->get_address());
        response->set_ip_source_address_s(source->get_address());

        response->set_destination_port(destination->get_port());
        response->set_source_port(source->get_port());

        response->set_data(data, 0);

        SendPacketEvent* event = new SendPacketEvent(s, response);
        Dispatcher::instance().enqueue(event);

        cmc->set_state(new Closing());
        
    } else if (p->is_tcp_ack()) {
        cout << "FinWait1::receive_packet(), ACK" << endl;
        cmc->set_state(new FinWait2());
    }
}

bool FinWait1::state_can_receive(Context* c, Socket* s) {
    return true;
}
