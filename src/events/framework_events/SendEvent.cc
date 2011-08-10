#include "events/framework_events/SendEvent.h"

SendEvent::SendEvent(gcstring_map& m, gcstring& file, Socket* s) : LibraryEvent(m, file, s), data_(0), data_length_(-1) {

}

SendEvent::SendEvent() : LibraryEvent() {
    
}

void SendEvent::execute(IModule* m) {
    m->imodule_library_send(this);
}

unsigned char * SendEvent::get_data() {
    if (data_ == 0) {
        data_ = (unsigned char*) get_map().at(BUFFER_STRING).data();
    }
    return data_;
}

ssize_t SendEvent::data_length() {
    if(data_length_ < 0) {
        data_length_ = atoi(get_map().at(N_STRING).c_str());
    }

    return data_length_;
}