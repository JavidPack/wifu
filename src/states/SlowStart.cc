#include "states/SlowStart.h"


SlowStart::SlowStart() {

}

SlowStart::~SlowStart() {

}

void SlowStart::send_packet(Context* c, SendPacketEvent* e) {

}

void SlowStart::receive_packet(Context* c, NetworkReceivePacketEvent* e) {

}

ssize_t SlowStart::send_to(Context* c, SendEvent* e) {
    cout << "SlowStart::send_to()" << endl;
}

void SlowStart::enter(Context* c) {
    enter_state("SlowStart");
}

void SlowStart::exit(Context* c) {
    leave_state("SlowStart");
}


