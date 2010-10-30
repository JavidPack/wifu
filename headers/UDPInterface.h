/* 
 * File:   UDPInterface.h
 * Author: rbuck
 *
 * Created on October 30, 2010, 9:37 AM
 */

#ifndef _UDPINTERFACE_H
#define	_UDPINTERFACE_H

#include "Module.h"
#include "AddressPort.h"


class UDPInterface : public Module {
public:
    UDPInterface() : Module() {

    }

    virtual ~UDPInterface() {

    }

    void udp_send(Event* e) {
        SendSynEvent* event = (SendSynEvent*)e;
        cout << "UDPInterface udp_send: " << event->get_address() << " " << event->get_port() << endl;
        
    }    
};

#endif	/* _UDPINTERFACE_H */

