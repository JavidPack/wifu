#include "WifuEndBackEndLibrary.h"
#include "MessageStructDefinitions.h"

WifuEndBackEndLibrary& WifuEndBackEndLibrary::instance() {
    static WifuEndBackEndLibrary instance_;
    return instance_;
}

WifuEndBackEndLibrary::~WifuEndBackEndLibrary() {

    log_INFORMATIONAL("recv_events_size: ", pantheios::i(receive_events_.size()), " recv_response_events_size: ", pantheios::i(recv_response_events_.size()), " recv_response_sizes_size: ", pantheios::i(recv_response_sizes_.size()));
    log_INFORMATIONAL("send_events_size: ", pantheios::i(send_events_.size()), " send_response_events_size: ", pantheios::i(send_response_events_.size()), " send_response_sizes_size: ", pantheios::i(send_response_sizes_.size()));

    while (!recv_response_events_.empty()) {

        int size = 1000;
        char start[size];
        char end[size];
        memset(start, 0, size);
        memset(end, 0, size);

        sprintf(start, "%llu", receive_events_.front());
        sprintf(end, "%llu", recv_response_events_.front());

        basic_string<PAN_CHAR_T> start_s(start);
        basic_string<PAN_CHAR_T> end_s(end);
        basic_string<PAN_CHAR_T> size_s(recv_response_sizes_.front().c_str());

        log_INFORMATIONAL("recv_backend ", start_s, " ", end_s, " ", size_s);

        receive_events_.pop_front();
        recv_response_events_.pop_front();
        recv_response_sizes_.pop_front();
    }

    while (!send_response_events_.empty()) {

        int size = 1000;
        char start[size];
        char end[size];
        memset(start, 0, size);
        memset(end, 0, size);

        sprintf(start, "%llu", send_events_.front());
        sprintf(end, "%llu", send_response_events_.front());

        basic_string<PAN_CHAR_T> start_s(start);
        basic_string<PAN_CHAR_T> end_s(end);
        basic_string<PAN_CHAR_T> size_s(send_response_sizes_.front().c_str());

        log_INFORMATIONAL("send_backend ", start_s, " ", end_s, " ", size_s);

        send_events_.pop_front();
        send_response_events_.pop_front();
        send_response_sizes_.pop_front();
    }


}

void WifuEndBackEndLibrary::receive(gcstring& message, u_int64_t& receive_time) {
    // TODO: this method is way too long (and will likely get bigger)
    // TODO: refactor this method to use objects as much as possible

    //                cout << "WifuEndBackEndLibrary::receive(), message: " << message << endl;

    gcstring_map m;
    try {
        QueryStringParser::parse(message, m);
    } catch (WiFuException e) {
        return;
    }

    gcstring& name = m[NAME_STRING];
    gcstring& s = m[SOCKET_STRING];
    int socket_int = atoi(s.c_str());
    //assert(sockInt != 0);
    Socket* socket = SocketCollection::instance().get_by_id(socket_int);

    if (!name.compare(WIFU_RECVFROM_NAME)) {
        //        cout << Utils::get_current_time_microseconds_32() << " WifuEndBackEndLibrary::receive(), ReceiveEvent to be dispatched" << endl;
        //log_INFORMATIONAL("recv_event ");
        receive_events_.push_back(receive_time);
        dispatch(new ReceiveEvent(m, get_file(), socket));
        return;

    } else if (!name.compare(WIFU_SENDTO_NAME)) {
        send_events_.push_back(receive_time);
        dispatch(new SendEvent(m, get_file(), socket));
        return;

    } else if (!name.compare(WIFU_BIND_NAME)) {
        dispatch(new BindEvent(m, get_file(), socket));
        return;

    } else if (!name.compare(WIFU_LISTEN_NAME)) {
        dispatch(new ListenEvent(m, get_file(), socket));
        return;

    } else if (!name.compare(WIFU_ACCEPT_NAME)) {
        dispatch(new AcceptEvent(m, get_file(), socket));
        return;

    } else if (!name.compare(WIFU_CONNECT_NAME)) {
        dispatch(new ConnectEvent(m, get_file(), socket));
        return;

    } else if (!name.compare(WIFU_GETSOCKOPT_NAME)) {
        dispatch(new GetSocketOptionEvent(m, get_file(), socket));
        return;

    } else if (!name.compare(WIFU_SETSOCKOPT_NAME)) {
        dispatch(new SetSocketOptionEvent(m, get_file(), socket));
        return;

    } else if (!name.compare(WIFU_CLOSE_NAME)) {
        dispatch(new CloseEvent(m, get_file(), socket));
        return;

    } else if (!name.compare(WIFU_SOCKET_NAME)) {


        int domain = atoi(m[DOMAIN_STRING].c_str());
        int type = atoi(m[TYPE_STRING].c_str());
        int protocol = atoi(m[PROTOCOL_STRING].c_str());

        if (ProtocolManager::instance().is_supported(domain, type, protocol)) {
            Socket* socket = new Socket(domain, type, protocol);
            SocketCollection::instance().push(socket);

            dispatch(new SocketEvent(m, get_file(), socket));
            return;

        } else {
            gcstring_map response;
            response[SOCKET_STRING] = s;
            response[FILE_STRING] = get_file();
            response[SOCKET_STRING] = Utils::itoa(-1);
            response[ERRNO] = Utils::itoa(EPROTONOSUPPORT);
            // TODO: May not always want to respond immediately
            // TODO: We may need to wait for a response from the internal system
            gcstring response_message;
            QueryStringParser::create(name, response, response_message);
            u_int64_t time;
            send_to(m[FILE_STRING], response_message, &time);
        }
    }
}

void WifuEndBackEndLibrary::receive(unsigned char* message, int length, u_int64_t& receive_time) {
    struct GenericMessage* gm = reinterpret_cast<struct GenericMessage*> (message);

    switch (gm->message_type) {
        case WIFU_SOCKET:
        {
            cout << "WIFU_SOCKET: " << endl;
            LibraryEvent* event = new LibraryEvent();
            SocketEvent* se = static_cast<SocketEvent*> (event);
            SocketEvent* se1 = (SocketEvent*) event;
            cout << type_name(event) << endl;
            cout << type_name(se) << endl;
            cout << type_name(se1) << endl;
            cout << type_name(*se) << endl;
        }
            break;
        case WIFU_BIND:
            cout << "WIFU_BIND" << endl;
            break;
        case WIFU_LISTEN:
            cout << "WIFU_LISTEN" << endl;
            break;
        case WIFU_ACCEPT:
            cout << "WIFU_ACCEPT" << endl;
            break;
        case WIFU_SENDTO:
        case WIFU_SEND:
            cout << "WIFU_SEND(TO)" << endl;
            break;
        case WIFU_RECVFROM:
        case WIFU_RECV:
            cout << "WIFU_RECV(FROM)" << endl;
            break;
        case WIFU_CONNECT:
            cout << "WIFU_CONNECT" << endl;
            break;
        case WIFU_GETSOCKOPT:
            cout << "WIFU_GETSOCKOPT" << endl;
            break;
        case WIFU_SETSOCKOPT:
            cout << "WIFU_SETSOCKOPT" << endl;
            break;
        case WIFU_CLOSE:
            cout << "WIFU_CLOSE" << endl;
            break;
        case WIFU_PRECLOSE:
            cout << "WIFU_PRECLOSE" << endl;
            break;
        default:
            return;
            //throw WiFuException("Unknown message type");
    }
}

void WifuEndBackEndLibrary::imodule_library_response(Event* e) {
    ResponseEvent* event = (ResponseEvent*) e;
    event->put(FILE_STRING, get_file());
    gcstring response;
    event->get_response(response);
    //        cout << "Response: " << response << endl;
    u_int64_t time;
    send_to(event->get_write_file(), response, &time);
    if (!event->get_name().compare(WIFU_RECVFROM_NAME)) {
        //cout << Utils::get_current_time_microseconds_32() << " WifuEndBackEndLibrary::imodule_library_response()" << endl;
        //log_INFORMATIONAL("recv_response_event ", (pan_uint64_t) Utils::get_current_time_microseconds_64());
        recv_response_events_.push_back(time);
        recv_response_sizes_.push_back(*(event->get(RETURN_VALUE_STRING)));
    } else if (!event->get_name().compare(WIFU_SENDTO_NAME)) {
        //cout << Utils::get_current_time_microseconds_32() << " WifuEndBackEndLibrary::imodule_library_response()" << endl;
        //log_INFORMATIONAL("recv_response_event ", (pan_uint64_t) Utils::get_current_time_microseconds_64());
        send_response_events_.push_back(time);
        send_response_sizes_.push_back(*(event->get(RETURN_VALUE_STRING)));
    }

}

WifuEndBackEndLibrary::WifuEndBackEndLibrary() : LocalSocketFullDuplex("/tmp/WS"), Module() {
    log_INFORMATIONAL("WiFuBackEndLibrary Created");
}
