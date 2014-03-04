#include "events/framework_events/SendEvent.h"

SendEvent::SendEvent() : DataEvent() {
    
}

void SendEvent::execute(IModule* m) {
printf("SendEvent::execute(IModule* m)\n");
    m->imodule_library_send(this);
}

unsigned char * SendEvent::get_data() {
printf("get data called in send event\n");
    return get_buffer() + sizeof(struct SendToMessage);
}
