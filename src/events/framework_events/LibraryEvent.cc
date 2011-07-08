#include "events/framework_events/LibraryEvent.h"

LibraryEvent::LibraryEvent(gcstring& message, gcstring& file, Socket* socket) : FrameworkEvent(socket), file_(file) {
    QueryStringParser::parse(message, m_);
    name_ = m_[NAME_STRING];

    m_[SOCKET_STRING] = Utils::itoa(socket->get_socket_id());
}

LibraryEvent::~LibraryEvent() {
    
}

gcstring& LibraryEvent::get_file() {
    return file_;
}

gcstring& LibraryEvent::get_name() {
    return name_;
}

gcstring_map& LibraryEvent::get_map() {
    return m_;
}
