#include "contexts/ConnectionManagerContext.h"

ConnectionManagerContext::ConnectionManagerContext() : Context() {
    set_state(new Closed());
}

// TODO: move this method (and all others that we can) into Context

void ConnectionManagerContext::listen(Socket* s, int back_log) {
    get_state()->listen(this, s, back_log);
}

void ConnectionManagerContext::connect(ConnectEvent* e) {
    get_state()->connect(this, e);
}

void ConnectionManagerContext::accept(Socket* s) {
    get_state()->accept(this, s);
}

void ConnectionManagerContext::close() {
    get_state()->close(this);
}

void ConnectionManagerContext::receive(Socket* s, Packet* p) {
    get_state()->receive(this, s, p);
}

void ConnectionManagerContext::send(Socket* s, Packet* p) {
    get_state()->send(this, s, p);
}

bool ConnectionManagerContext::is_open() {
    return get_state()->is_open();
}

// Non-state methods

int ConnectionManagerContext::get_back_log() {
    return back_log_;
}

void ConnectionManagerContext::set_back_log(int back_log) {
    back_log_ = back_log;
}

ConnectionType ConnectionManagerContext::get_connection_type() {
    return type_;
}

void ConnectionManagerContext::set_connection_type(ConnectionType type) {
    type_ = type;
}

ConnectEvent* ConnectionManagerContext::get_connect_event() {
    return c_event_;
}

void ConnectionManagerContext::set_connect_event(ConnectEvent* e) {
    c_event_ = e;
}