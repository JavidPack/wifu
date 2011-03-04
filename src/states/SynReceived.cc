#include "states/SynReceived.h"

SynReceived::SynReceived() {

}

SynReceived::~SynReceived() {

}

void SynReceived::enter(Context* c) {
    cout << "Entering SynReceived State" << endl;
}

void SynReceived::exit(Context* c) {

}

void SynReceived::receive_packet(Context* c, NetworkReceivePacketEvent* e) {
    cout << "SynReceived::receive_packet()" << endl;
    ConnectionManagerContext* cmc = (ConnectionManagerContext*) c;
    TCPPacket* packet = (TCPPacket*) e->get_packet();
    Socket* s = e->get_socket();

    if (packet->is_tcp_ack()) {
        cout << "SynReceived::receive_packet(): Packet is ACK" << endl;

        Socket* new_socket = new Socket(s->get_domain(),
                                            s->get_type(),
                                            s->get_protocol(),
                                            s->get_local_address_port(),
                                            packet->get_source_address_port());

        ConnectionEstablishedEvent* event = new ConnectionEstablishedEvent(cmc->get_accept_event(), new_socket);
        Dispatcher::instance().enqueue(event);
        cmc->set_state(new Listen());

        return;
    }
}
