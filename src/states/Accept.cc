#include "states/Accept.h"

Accept::Accept() {

}

Accept::~Accept() {

}

void Accept::enter(Context* c) {

}

void Accept::exit(Context* c) {

}

void Accept::receive_packet(Context* c, Socket* s, WiFuPacket* p) {
    cout << "Accept::receive_packet()" << endl;
    ConnectionManagerContext* cmc = (ConnectionManagerContext*) c;
    TCPPacket* packet = (TCPPacket*) p;

    assert(packet->is_tcp_syn());

    if(packet->is_tcp_syn()) {
        cout << "Accept::receive_packet(): Packet is a SYN" << endl;
        unsigned char* data = (unsigned char*) "";
        AddressPort* source = packet->get_dest_address_port();
        AddressPort* destination = packet->get_source_address_port();

        TCPPacket* response = new TCPPacket();
        response->set_ip_protocol(SIMPLE_TCP);
        response->set_ip_destination_address_s(destination->get_address());
        response->set_ip_source_address_s(source->get_address());

        response->set_destination_port(destination->get_port());
        response->set_source_port(source->get_port());

        response->set_tcp_syn(true);
        response->set_tcp_ack(true);
        response->set_data(data, 0);

        SendPacketEvent* e = new SendPacketEvent(s, response);
        Dispatcher::instance().enqueue(e);

        cmc->set_state(new SynReceived());
        return;
    }

    // Should never receive a different type of packet in the Listent State
    assert(false);


}
