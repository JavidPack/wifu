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
#include "states/State.h"

using namespace std;

class State;

class Context : public IContext {
public:
    Context();

    virtual ~Context();

    virtual void set_state(State* s);

    virtual State* get_state();

    // From IContext
    virtual void socket(SocketEvent* e);

    virtual void bind(BindEvent* e);

    virtual void listen(ListenEvent* e);

    virtual void connect(ConnectEvent* e);

    virtual void accept(AcceptEvent* e);

    virtual void new_connection_established(ConnectionEstablishedEvent* e);

    virtual void new_conneciton_initiated(ConnectionInitiatedEvent* e);

    virtual void close();

    virtual void receive_packet(NetworkReceivePacketEvent* e);

    virtual void send_packet(SendPacketEvent* e);

    virtual void timer_fired_event(TimerFiredEvent* e);

    virtual void resend_packet(ResendPacketEvent* e);

    virtual ssize_t send_to(SendEvent* e);

    virtual bool is_connected(Socket*);

    virtual void receive_from(ReceiveEvent* e);

private:
    State* current_;

};

#endif	/* CONTEXT_H */
