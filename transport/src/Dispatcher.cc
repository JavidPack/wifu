#include "Dispatcher.h"


Dispatcher& Dispatcher::instance() {
    static Dispatcher instance_;
    return instance_;
}

Dispatcher::~Dispatcher() {
    reset();
}

void Dispatcher::map_event(event_name name, EventQPPointer q) {
//printf("Dispatcher::map_event\n");
    mutex_.wait();
    if (map_.find(name) == map_.end()) {
        map_[name] = new vector<EventQPPointer, gc_allocator<EventQPPointer> >;
    }

    vector<EventQPPointer, gc_allocator<EventQPPointer> >::iterator itr = find(map_[name]->begin(), map_[name]->end(), q);
    if (itr == map_[name]->end()) {
        map_[name]->push_back(q);
    }
    mutex_.post();
}

void Dispatcher::reset() {
    mutex_.wait();
    itr_ = map_.begin();
    while (itr_ != map_.end()) {
        QPVectorPointer v = itr_->second;
        delete v;
        ++itr_;
    }
    map_.clear();
    mutex_.post();
}

void Dispatcher::process(Event* e) {
//printf("Dispatcher::process\n");
    mutex_.wait();
    assert(e);
    itr_ = map_.find(type_name(*e));

    if (itr_ != map_.end()) {

        QPVectorPointer queue_processors = itr_->second;

        for (int i = 0; i < queue_processors->size(); i++) {
            queue_processors->at(i)->enqueue(e);
        }
    }
    mutex_.post();
}


Dispatcher::Dispatcher() : QueueProcessor<Event*>() {
    mutex_.init(1);
}
