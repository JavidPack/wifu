/* 
 * File:   ContextContainer.h
 * Author: rbuck
 *
 * Created on December 31, 2010, 9:52 AM
 */

#ifndef CONTEXTCONTAINER_H
#define	CONTEXTCONTAINER_H

#include "IContext.h"
#include "GarbageCollector.h"
#include "ReliabilityContext.h"
#include "CongestionControlContext.h"
#include "ConnectionManagerContext.h"

#include "events/ReceiveEvent.h"
#include "events/SendEvent.h"
#include "events/CloseEvent.h"

using namespace std;

class IContextContainer : public gc {
public:
    IContextContainer();

    IContextContainer(IContextContainer const& other);

    IContext* get_congestion_control();

    IContext* get_reliability();

    IContext* get_connection_manager();

    SendEvent* get_saved_send_event();

    void set_saved_send_event(SendEvent* e);

    ReceiveEvent* get_saved_receive_event();

    void set_saved_receive_event(ReceiveEvent* e);

    CloseEvent* get_saved_close_event();

    void set_saved_close_event(CloseEvent* e);

private:
    IContext* reliability_;
    IContext* cc_;
    IContext* cm_;

    SendEvent* saved_send_event_;
    ReceiveEvent* saved_receive_event_;
    CloseEvent* close_event_;
};

#endif	/* CONTEXTCONTAINER_H */
