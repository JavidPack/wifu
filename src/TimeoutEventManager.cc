/*
 * File:   TimeoutEventManager.h
 * Author: rbuck
 *
 * Created on October 21, 2010, 12:41 PM
 */

#include "TimeoutEventManager.h"

TimeoutEventManager::TimeoutEventManager() : Module(&queue_) {

    //make sure singleton is created
    TimeoutManagerSemaphore;

    signal(SIG_ENQUEUE_EVENT, signal_manager);
    signal(SIG_CANCEL_EVENT, signal_manager);
}

TimeoutEventManager::~TimeoutEventManager() {

}

void TimeoutEventManager::cancel(Event * event) {
    CanceledEvents::instance().add(event);
    raise(SIG_CANCEL_EVENT);
}

void TimeoutEventManager::timeout(Event* e) {
    TimeoutEvent* event = (TimeoutEvent*) e;

    if (CanceledEvents::instance().is_canceled(event)) {
        CanceledEvents::instance().remove(event);
        delete event;
        return;
    }

    bool timedout = TimeoutManagerSemaphore.timed_wait(&event->get_timeout_time());

    if (timedout) {
        //event->execute(this);
        cout << "Timed out" << endl;
        cout << event->get_timeout_time().tv_sec << endl;
        cout << event->get_timeout_time().tv_nsec << endl;

        return;
    }

    // Semaphore was posted on
    enqueue(event, false);
}

void TimeoutEventManager::enqueue(Event* e, bool signal) {
    TimeoutEvent* event = (TimeoutEvent*) e;
    this->QueueProcessor<Event*>::enqueue(event, signal);
}

void signal_manager(int signal) {
    switch (signal) {
        case SIG_ENQUEUE_EVENT:
        case SIG_CANCEL_EVENT:
            TimeoutManagerSemaphore.post();
            break;
    }
}