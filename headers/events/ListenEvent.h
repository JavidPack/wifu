/* 
 * File:   ListenEvent.h
 * Author: rbuck
 *
 * Created on December 28, 2010, 10:35 AM
 */

#ifndef _LISTENEVENT_H
#define	_LISTENEVENT_H

#include "LibraryEvent.h"

class ListenEvent : public LibraryEvent {
public:
    ListenEvent(string& message, string& file, Socket* s);

    virtual ~ListenEvent();

    void execute(IModule* m);

};

#endif	/* _LISTENEVENT_H */
