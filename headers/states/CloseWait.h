/* 
 * File:   CloseWait.h
 * Author: rbuck
 *
 * Created on April 18, 2011, 4:41 PM
 */

#ifndef CLOSEWAIT_H
#define	CLOSEWAIT_H

#include "State.h"
#include "contexts/Context.h"
#include "contexts/ConnectionManagerContext.h"

#include "states/LastAck.h"

#include "events/ResponseEvent.h"
#include "events/CloseEvent.h"

#include "packet/TCPPacket.h"

class CloseWait : public State {
public:
    CloseWait();
    virtual ~CloseWait();
    virtual void enter(Context* c);
    virtual void exit(Context* c);

    virtual void state_close(Context* c, CloseEvent* e);
};

#endif	/* CLOSEWAIT_H */

