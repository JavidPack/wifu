/* 
 * File:   Closed.h
 * Author: rbuck
 *
 * Created on December 30, 2010, 1:05 PM
 */

#ifndef CLOSED_H
#define	CLOSED_H

#include <iostream>

#include "../contexts/Context.h"
#include "contexts/ConnectionManagerContext.h"
#include "states/Listen.h"
#include "states/SynSent.h"

using namespace std;

class Closed : public State {
public:
    Closed();
    virtual ~Closed();
    virtual void enter(Context* c);
    virtual void exit(Context* c);
    virtual void connect(Context* c, string& remote);
    virtual void listen(Context* c, Socket* s, int back_log);
    
};

#endif	/* CLOSED_H */
