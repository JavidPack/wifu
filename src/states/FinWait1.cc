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

    if(p->is_tcp_ack()) {
        cout << "FinWait1::receive_packet(), ACK" << endl;
        cmc->set_state(new FinWait2());
    }
}

bool FinWait1::state_can_receive(Context* c, Socket* s) {
    return true;
}
