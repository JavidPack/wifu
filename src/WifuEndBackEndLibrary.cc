#include "WifuEndBackEndLibrary.h"

WifuEndBackEndLibrary& WifuEndBackEndLibrary::instance() {
	static WifuEndBackEndLibrary instance_;
	return instance_;
}

WifuEndBackEndLibrary::~WifuEndBackEndLibrary() {}

void WifuEndBackEndLibrary::receive(string& message) {
	// TODO: this method is way too long (and will likely get bigger)
	// TODO: refactor this method to use objects as much as possible

	map<string, string> m;
	QueryStringParser::parse(message, m);

	string name = m[NAME_STRING];
	string s = m[SOCKET_STRING];
	Socket* socket = SocketCollection::instance().get_by_id(atoi(s.c_str()));

	if (!name.compare(WIFU_SOCKET_NAME)) {

		int domain = atoi(m[DOMAIN_STRING].c_str());
		int type = atoi(m[TYPE_STRING].c_str());
		int protocol = atoi(m[PROTOCOL_STRING].c_str());

		if (ProtocolManager::instance().is_supported(domain, type, protocol)) {
			Socket* socket = new Socket(domain, type, protocol);
			SocketCollection::instance().push(socket);

			dispatch(new SocketEvent(message, getFile(), socket));
			return;

		} else {
			map<string, string> response;
			response[SOCKET_STRING] = s;
			response[FILE_STRING] = getFile();
			response[SOCKET_STRING] = Utils::itoa(-1);
			response[ERRNO] = Utils::itoa(EPROTONOSUPPORT);
			// TODO: May not always want to respond immediately
			// TODO: We may need to wait for a response from the internal system
			string response_message = QueryStringParser::create(name, response);
			send_to(m[FILE_STRING], response_message);
		}


	} else if (!name.compare(WIFU_BIND_NAME)) {

		dispatch(new BindEvent(message, getFile(), socket));
		return;

	} else if (!name.compare(WIFU_LISTEN_NAME)) {

		dispatch(new ListenEvent(message, getFile(), socket));
		return;

	} else if (!name.compare(WIFU_ACCEPT_NAME)) {
		dispatch(new AcceptEvent(message, getFile(), socket));
		return;

	} else if (!name.compare(WIFU_SENDTO_NAME)) {
		int return_val = 1;
		//            response[RETURN_VALUE_STRING] = Utils::itoa(return_val);
	} else if (!name.compare(WIFU_RECVFROM_NAME)) {


	} else if (!name.compare(WIFU_CONNECT_NAME)) {
		dispatch(new ConnectEvent(message, getFile(), socket));
		return;

	} else if (!name.compare(WIFU_GETSOCKOPT_NAME)) {
		int return_val = SO_BINDTODEVICE;
		//            response[RETURN_VALUE_STRING] = Utils::itoa(return_val);
	} else if (!name.compare(WIFU_SETSOCKOPT_NAME)) {
		int return_val = 0;
		//            response[RETURN_VALUE_STRING] = Utils::itoa(return_val);
	}
}

void WifuEndBackEndLibrary::library_response(Event* e) {
	ResponseEvent* event = (ResponseEvent*) e;
	event->put(FILE_STRING, getFile());
	string file = event->get_write_file();
	string response = event->get_response();
//        cout << "Response: " << response << endl;
	send_to(file, response);
}

WifuEndBackEndLibrary::WifuEndBackEndLibrary() : LocalSocketFullDuplex("/tmp/WS"), Module() {}