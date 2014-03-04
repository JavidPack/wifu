#include "events/framework_events/BufferEvent.h"

BufferEvent::BufferEvent() : FrameworkEvent() {

}

BufferEvent::~BufferEvent() {

}

unsigned char* BufferEvent::get_buffer() {
printf("getbuffercalled in bufferevent\n");
    return buffer_;
}

int BufferEvent::buffer_capacity() {
    return UNIX_SOCKET_MAX_BUFFER_SIZE;
}
