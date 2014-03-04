#include "Module.h"

Module::Module() : IModule(), QueueProcessor<Event*>() {
    start_processing();
}

Module::Module(IQueue<Event*>* queue) : IModule(), QueueProcessor<Event*>(queue) {
    start_processing();
}

Module::~Module() {

}

void Module::process(Event* e) {
//printf("Module::process\n");
    e->execute(this);
}

void Module::dispatch(Event* e) {
//printf("Module::dispatch\n");
    Dispatcher::instance().enqueue(e);
}
