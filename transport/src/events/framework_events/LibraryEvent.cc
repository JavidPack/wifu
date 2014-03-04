#include "events/framework_events/LibraryEvent.h"

LibraryEvent::~LibraryEvent() {

}

LibraryEvent::LibraryEvent() : BufferEvent() {

}

size_t showbites(const void *object, size_t size)
{
	size_t oldsize = size;
   const unsigned char *byte;
   for ( byte = (const unsigned char *)object; size--; ++byte )
   {
      printf("%02X", *byte);
   }
   putchar('\n');
	return oldsize;
}

void LibraryEvent::save_buffer(unsigned char* buffer, u_int32_t length) {
printf("save buffer called of length: %i\n",length);
printf("save buffer called on: %s\n",buffer);
printf("save buffer called on adjuster: %s\n",buffer + sizeof(struct SendToMessage));
showbites(buffer, length);
    memcpy(get_buffer(), buffer, length);
    message_ = reinterpret_cast<FrontEndMessage*>(get_buffer());
    buffer_length_ = length;
}

struct sockaddr_un* LibraryEvent::get_source() const {
printf("get_source in libeven");
    return &(message_->source);
}

u_int32_t LibraryEvent::get_message_type() const {
printf("get_messagetype in libeven");
    return message_->message_type;
}

u_int32_t LibraryEvent::get_buffer_length() const {
printf("get_buffer_length in libeven");
    return buffer_length_;
}

int LibraryEvent::get_fd() const {
printf("get_fd in libeven");
    return message_->fd;
}
