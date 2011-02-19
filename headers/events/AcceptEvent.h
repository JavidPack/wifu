/* 
 * File:   AcceptEvent.h
 * Author: rbuck
 *
 * Created on January 14, 2011, 10:49 AM
 */

#ifndef _ACCEPTEVENT_H
#define	_ACCEPTEVENT_H

#include "LibraryEvent.h"

class AcceptEvent : public LibraryEvent {
public:
    AcceptEvent(string& message, string& file, Socket* s);

    virtual ~AcceptEvent();

    void execute(IModule* m);

};

#endif	/* _ACCEPTEVENT_H */
