#include "states/SlowStart.h"

SlowStart::SlowStart() : State() {
}

SlowStart::~SlowStart() {

}

void SlowStart::send_packet(Context* c, SendPacketEvent* e) {
    CongestionControlContext* ccc = (CongestionControlContext*) c;
    TCPPacket* p = (TCPPacket*) e->get_packet();

    ccc->set_last_sent_sequence_number(p->get_tcp_sequence_number());

    if(p->is_tcp_syn() || p->is_tcp_fin()) {
        ccc->set_num_outstanding(ccc->get_num_outstanding() + 1);
    }
}

void SlowStart::receive_packet(Context* c, NetworkReceivePacketEvent* e) {
//    cout << "SlowStart::receive_packet()" << endl;
    CongestionControlContext* ccc = (CongestionControlContext*) c;
    Socket* s = e->get_socket();
    TCPPacket* p = (TCPPacket*) e->get_packet();

    if(ccc->get_num_outstanding() > 0 && p->is_tcp_ack() && p->get_tcp_ack_number() - 1 == ccc->get_last_sent_sequence_number()) {
        ccc->set_num_outstanding(ccc->get_num_outstanding() - 1);
    }


    if (p->get_data_length_bytes() > 0) {
        // Send ACK
        TCPPacket* p = new TCPPacket();
        p->set_ip_protocol(SIMPLE_TCP);

        AddressPort* destination = s->get_remote_address_port();
        AddressPort* source = s->get_local_address_port();

        p->set_ip_destination_address_s(destination->get_address());
        p->set_destination_port(destination->get_port());
        
        p->set_ip_source_address_s(source->get_address());
        p->set_source_port(source->get_port());

        unsigned char* data = (unsigned char*) "";
        p->set_data(data, 0);

        Event* spe = new SendPacketEvent(s, p);
        
//        cout << "SlowStart::receive_packet() A: Packet: " << p << endl;

        Dispatcher::instance().enqueue(spe);
    }
    else if(!ccc->get_num_outstanding() && s->get_send_buffer().size() > 0) {
        assert(p->is_tcp_ack());
        // receive an ACK
        // send data
        TCPPacket* p = new TCPPacket();

        string data = s->get_send_buffer().substr(0, p->max_data_length());
        s->get_send_buffer().erase(0, data.size());

        AddressPort* destination = s->get_remote_address_port();
        AddressPort* source = s->get_local_address_port();

        p->set_ip_protocol(SIMPLE_TCP);

        p->set_ip_destination_address_s(destination->get_address());
        p->set_destination_port(destination->get_port());

        p->set_ip_source_address_s(source->get_address());
        p->set_source_port(source->get_port());

        p->set_data((unsigned char*) data.c_str(), data.size());

        Event* spe = new SendPacketEvent(s, p);

        ccc->set_num_outstanding(ccc->get_num_outstanding() + 1);
//        cout << "SlowStart::receive_packet() B: Packet: " << p << endl;

        Dispatcher::instance().enqueue(spe);
        Dispatcher::instance().enqueue(new SendBufferNotFullEvent(s));
    }

//    cout << "SlowStart::receive_packet(), #outstanding: " << ccc->get_num_outstanding() << endl;
}

void SlowStart::state_send_buffer_not_empty(Context* c, SendBufferNotEmptyEvent* e) {
//    cout << "SlowStart::state_send_buffer_not_empty()" << endl;
    CongestionControlContext* ccc = (CongestionControlContext*) c;
    Socket* s = e->get_socket();

    if (!ccc->get_num_outstanding() && s->get_send_buffer().size() > 0) {

        TCPPacket* p = new TCPPacket();

        string data = s->get_send_buffer().substr(0, p->max_data_length());
        s->get_send_buffer().erase(0, data.size());

        AddressPort* destination = s->get_remote_address_port();
        AddressPort* source = s->get_local_address_port();

        p->set_ip_protocol(SIMPLE_TCP);

        p->set_ip_destination_address_s(destination->get_address());
        p->set_destination_port(destination->get_port());

        p->set_ip_source_address_s(source->get_address());
        p->set_source_port(source->get_port());

        p->set_data((unsigned char*) data.c_str(), data.size());


        Event* spe = new SendPacketEvent(s, p);
        Event* sbnf = new SendBufferNotFullEvent(s);

//        cout << "SlowStart::state_send_buffer_not_empty(): Packet: " << p << endl;

        ccc->set_num_outstanding(ccc->get_num_outstanding() + 1);
        Dispatcher::instance().enqueue(spe);
        Dispatcher::instance().enqueue(sbnf);
    }
}

void SlowStart::enter(Context* c) {
    enter_state("SlowStart");
}

void SlowStart::exit(Context* c) {
    leave_state("SlowStart");
}