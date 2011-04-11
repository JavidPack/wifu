/* 
 * File:   ReceiveBufferNotEmpty.h
 * Author: rbuck
 *
 * Created on April 6, 2011, 10:54 AM
 */

#ifndef RECEIVEBUFFERNOTEMPTYEVENT_H
#define	RECEIVEBUFFERNOTEMPTYEVENT_H

#include "Event.h"

class ReceiveBufferNotEmptyEvent : public Event {
public:
    ReceiveBufferNotEmptyEvent(Socket* s);

    void execute(IModule* m);
};

#endif	/* RECEIVEBUFFERNOTEMPTYEVENT_H */
