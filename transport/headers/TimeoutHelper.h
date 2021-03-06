/* 
 * File:   TimeoutHelper.h
 * Author: rbuck
 *
 * Created on November 3, 2010, 2:04 PM
 */

#ifndef _TIMEOUTHELPER_H
#define	_TIMEOUTHELPER_H

#include "Set.h"
#include "events/framework_events/TimeoutEvent.h"
#include "Dispatcher.h"
#include "events/framework_events/TimerFiredEvent.h"
#include "events/framework_events/CancelTimerEvent.h"

/**
 * Class which aides a Module in filtering TimerFiredEvent objects.
 *
 * Due to the fact that every Module will receive all TimerFiredEvent objects,
 * this class helps filter and pass along only those which a corresponding TimeoutEvent was created by this class.
 */
class TimeoutHelper : public Set<Event*> {
public:

    /**
     * Creates a TimeoutHelper object.
     */
    TimeoutHelper();

    /**
     * Cleans up this TimeoutHelper object.
     */
    virtual ~TimeoutHelper();

    /**
     * Keeps track of event as coming from this object and then calls Dispacther::enqueue()
     *
     * @param event The Event to keep track of.
     */
    void dispatch_timeout(Event* event);

    /**
     * Determines if the TimeoutEvent internal to event was sent via this object's dispatch_timeout() method.
     * Calling this method twice in a row on the same TimerFiredEvent will always result in the second call returning false.
     *
     * @param event The TimerFiredEvent in question as to whether this object generated its TimeoutEvent.
     * @return True if the TimeoutEvent internal to event was sent via this object's dispatch_timeout() method, false otherwise.
     */
    bool is_my_timeout(TimerFiredEvent* event);

    /**
     * Generates a CancelTimerEvent and sends it to the Dispatcher.
     *
     * @param event The TimeoutEvent to cancel.
     */
    void cancel_timeout(TimeoutEvent* event);
};

#endif	/* _TIMEOUTHELPER_H */

