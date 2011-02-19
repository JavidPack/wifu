/* 
 * File:   NetworkSendPacketEvent.h
 * Author: rbuck
 *
 * Created on January 28, 2011, 11:56 AM
 */

#ifndef _NETWORKSENDPACKETEVENT_H
#define	_NETWORKSENDPACKETEVENT_H

#include "PacketEvent.h"
#include "Socket.h"

class NetworkSendPacketEvent : public PacketEvent {
public:
    NetworkSendPacketEvent(Socket*, WiFuPacket*);

    virtual ~NetworkSendPacketEvent();

    void execute(IModule*);

};

#endif	/* _NETWORKSENDPACKETEVENT_H */
