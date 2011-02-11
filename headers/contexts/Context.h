/* 
 * File:   Context.h
 * Author: rbuck
 *
 * Created on December 30, 2010, 10:36 AM
 */

#ifndef CONTEXT_H
#define	CONTEXT_H

#include <string>
#include <iostream>
#include "IContext.h"

#include "AddressPort.h"
#include "Socket.h"
#include "events/ConnectEvent.h"
#include "events/AcceptEvent.h"
#include "events/TimerFiredEvent.h"

using namespace std;

class State;

class Context : public IContext {
public:
    Context();
    virtual ~Context();
    virtual void set_state(State* s);
    State* get_state();

private:
    class State* current_;
};

class State {
public:

    State() {

    }

    virtual ~State() {
    }

    virtual void enter(Context* c) {
    }

    virtual void exit(Context* c) {
    }

    virtual void socket(Context* c, Socket* s) {

    }

    virtual void bind(Context* c, Socket* s, AddressPort* ap) {

    }

    virtual void listen(Context* c, Socket* s, int back_log) {
    }

    virtual void accept(Context* c, AcceptEvent* e) {
    }

    virtual void new_connection_established(Context* c, Socket* s) {
    }

    virtual void receive_packet(Context* c, Socket* s, WiFuPacket* p) {
    }

    virtual void timer_fired(Context* c, TimerFiredEvent* e) {
    }

    virtual void connect(Context* c, ConnectEvent* e) {
    }

    virtual void close(Context* c) {
    }

    virtual void send_packet(Context* c, Socket* s, WiFuPacket* p) {
    }

    void enter_state(string state) {
        //cout << "Entering " << state << " State" << endl;
    }

    void leave_state(string state) {
        //cout << "Leaving " << state << " State" << endl;
    }
};


#endif	/* CONTEXT_H */

