/* 
 * File:   ConnectionInitiatedEvent.h
 * Author: rbuck
 *
 * Created on March 4, 2011, 2:17 PM
 */

#ifndef _CONNECTIONINITIATEDEVENT_H
#define	_CONNECTIONINITIATEDEVENT_H

#include "ProtocolEvent.h"
#include "Socket.h"
#include "events/framework_events/ListenEvent.h"

using namespace std;

/**
 * Event that aides in switching contexts between a socket that is accpeting new connections and one that is connected.
 *
 * @see Event
 * @see ProtocolEvent
 */
class ConnectionInitiatedEvent : public ProtocolEvent {
public:

    /**
     * Constructs a new ConnectionInitiatedEvent
     * @param listening_socket The socket that is listening for and accepting new connections.
     * @param new_socket A newely created socket because a remote host sent a SYN packet.
     * @param listen_event The original listen event used to make this a passive connection
     */
    ConnectionInitiatedEvent(Socket* listening_socket, Socket* new_socket, ListenEvent* listen_event);

    /**
     * Destructor.
     */
    virtual ~ConnectionInitiatedEvent();

    /**
     * Calls IModule::imodule_connection_initiated() and passes this ConnectionInitiatedEvent in as the argument.
     *
     * @param m The module which to call IModule::imodule_connection_initiated() on.
     * @see Event::execute()
     * @see IModule
     * @see IModule::imodule_connection_initiated()
     */
    void execute(IModule* m);

    /**
     * @return The newely created socket that needs to finish connecting.
     */
    Socket* get_new_socket();

    ListenEvent* get_listen_event();

private:

    /**
     * Pointer to a Socket object that is in the process of connecting to a remote host.
     */
    Socket* new_socket_;

    /**
     * Original ListenEvent (so we know who to respond to when we close())
     */
    ListenEvent* listen_event_;

};

#endif	/* _CONNECTIONINITIATEDEVENT_H */

