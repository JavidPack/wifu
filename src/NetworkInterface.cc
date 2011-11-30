#include "NetworkInterface.h"

NetworkInterface& NetworkInterface::instance() {
    static NetworkInterface instance_;
    return instance_;
}

NetworkInterface::~NetworkInterface() {
}

void NetworkInterface::start() {
    listener_.start(this);
}

void NetworkInterface::register_protocol(int protocol, PacketFactory* pf) {
    listener_.register_protocol(protocol, pf);
}

void NetworkInterface::imodule_network_receive(WiFuPacket* p) {

    usleep(19000);

    PacketLogger::instance().log(p);
    //    cout << "Network receive packet: \n" << p->to_s_format() << endl << p->to_s() << endl;

    
    // TODO: figure out if the kernel does this check for us
    if (!p->is_valid_ip_checksum()) {
        return;
    }


    AddressPort* remote = p->get_source_address_port();
    AddressPort* local = p->get_dest_address_port();


    Socket* s = SocketCollection::instance().get_by_local_and_remote_ap(local, remote);

    if (!s) {
        //cout << "Trying to find socket...\n";
        s = SocketCollection::instance().get_by_local_ap(local);
    }

    if (!s) {
        // No bound local socket
        //TODO: should it really just return like this or should it throw an exception? -Scott
        //cout << "Dropping packet, we didn't find a valid socket...\n";
        return;
    }

    Event* e = new NetworkReceivePacketEvent(s, p);
    //cout << "NetworkInterface::imodule_network_receive: dispatching NetworkReceivePacketEvent.\n";
    Dispatcher::instance().enqueue(e);
}

void NetworkInterface::imodule_network_send(Event* e) {
    NetworkSendPacketEvent* event = (NetworkSendPacketEvent*) e;

    usleep(20000);

    // TODO: Check return value (bytes sent)?
    WiFuPacket* p = event->get_packet();
    p->calculate_and_set_ip_checksum();
    //    cout << "Network sending packet: \n" << p->to_s_format() << endl << p->to_s() << endl;
    sender_.send(p);
    PacketLogger::instance().log(p);
    
}

NetworkInterface::NetworkInterface() : INetworkInterface() {

}
