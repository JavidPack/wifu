/* 
 * File:   PacketFactory.h
 * Author: rbuck
 *
 * Created on January 28, 2011, 1:09 PM
 */

#ifndef _PACKETFACTORY_H
#define	_PACKETFACTORY_H

#include "packet/WiFuPacket.h"
#include "GarbageCollector.h"

class PacketFactory : public gc {
public:
    virtual WiFuPacket* create() = 0;
};

#endif	/* _PACKETFACTORY_H */

