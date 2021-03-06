/* 
 * File:   Queue.h
 * Author: rbuck
 *
 * Created on October 21, 2010, 6:41 PM
 */

#ifndef _PRIORITYQUEUE_H
#define	_PRIORITYQUEUE_H


#include <list>
#include <algorithm>
#include <typeinfo>
#include <signal.h>

#include "defines.h"
#include "Semaphore.h"
#include "IQueue.h"
#include "Utils.h"

using namespace std;

/**
 * Queue which gives priority to its elements based on Comparator.
 * This object is thread safe.
 * @see IQueue
 * @see Queue
 */
template<class T, class Comparator>
class PriorityQueue : public IQueue<T> {
public:

    /**
     * Constructs a PriorityQueue object.
     */
    PriorityQueue() : IQueue<T>() {
        sem_.init(1);
        counter_.init(0);
    }

    /**
     * Cleans up this PriorityQueue object.
     */
    virtual ~PriorityQueue() {

    }

    /**
     * Dequeues and returns the get element in the queue.
     * This is a blocking method and will wait indefinitely if there is nothing to dequeue.
     *
     * @return The get element in the queue.
     */
    T dequeue() {
        counter_.wait();
        sem_.wait();
        T value = q_.front();
        q_.pop_front();
        sem_.post();
        return value;
    }

    /**
     * Enqueues obj into this object.  If signal is true, raises a SIG_ENQUEUE_EVENT signal.
     *
     * @param obj The element to put into this PriorityQueue.
     * @param signal If true, raises a SIG_ENQUEUE_EVENT signal.
     */
    void enqueue(T obj, bool signal = false) {
        sem_.wait();
        q_.insert(upper_bound(q_.begin(), q_.end(), obj, comparator_), obj);

        if (signal) {
            raise(SIG_ENQUEUE_EVENT);
        }
        sem_.post();
        counter_.post();
    }

    /**
     * @see IQueue::top()
     */
    const T& top() {
        sem_.wait();
        const T& val = q_.front();
        sem_.post();
        return val;
    }

    /**
     * @return The number of elements in this PriorityQueue.
     */
    int size() {
        sem_.wait();
        int value = q_.size();
        sem_.post();
        return value;
    }

    /**
     * @return True if there are no elements in this PriorityQueue, false otherwise.
     */
    bool is_empty() {
        // protection is done in size()
        return size() == 0;
    }

    /**
     * Empties this PriorityQueue
     */
    void clear() {
        sem_.wait();
        q_.clear();
        sem_.post();
    }

private:
    /**
     * Protective Semaphore; this semaphore makes this class thread safe.
     */
    Semaphore sem_;

    /**
     * Semaphore which enables a producer-consumer relationship.
     * Namely, we can block on wait().
     */
    Semaphore counter_;

    list<T, gc_allocator<T> > q_;
    Comparator comparator_;
};


#endif	/* _PRIORITYQUEUE_H */

