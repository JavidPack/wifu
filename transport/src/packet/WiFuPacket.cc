#include "packet/WiFuPacket.h"

WiFuPacket::WiFuPacket() : IPPacket() {
    init();
}

WiFuPacket::~WiFuPacket() {

}

unsigned char* WiFuPacket::get_data() {
    return get_next_header() + sizeof (struct wifu_packet_header);
}

int WiFuPacket::get_data_length_bytes() {
    return get_ip_tot_length() - get_ip_header_length_bytes() - sizeof (struct wifu_packet_header);
}

void WiFuPacket::set_data(unsigned char* data, int length) {
    memcpy(get_data(), data, length);
    set_ip_tot_length(get_ip_header_length_bytes() + sizeof (struct wifu_packet_header) + length);
}

void WiFuPacket::pack() {

}

u_int16_t WiFuPacket::get_source_port() const {
    return ntohs(ports_->sport);
}

void WiFuPacket::set_source_port(u_int16_t port) {
    ports_->sport = htons(port);
}

u_int16_t WiFuPacket::get_destination_port() const {
    return ntohs(ports_->dport);
}

void WiFuPacket::set_destination_port(u_int16_t port) {
    ports_->dport = htons(port);
}

AddressPort* WiFuPacket::get_source_address_port() {
    if (!source_) {
        source_ = new AddressPort(get_ip_source_address_s().c_str(), get_source_port());
    }
    return source_;
}

AddressPort* WiFuPacket::get_dest_address_port() {
    if (!dest_) {
        dest_ = new AddressPort(get_ip_destination_address_s().c_str(), get_destination_port());
    }
    return dest_;
}

void WiFuPacket::init() {
    ports_ = (struct wifu_packet_header*) get_next_header();
    source_ = 0;
    dest_ = 0;
    set_ip_tot_length(get_ip_header_length_bytes() + sizeof (struct wifu_packet_header));
}

int WiFuPacket::max_data_length() {
    return IPPacket::max_data_length() - sizeof (struct wifu_packet_header);
}

gcstring WiFuPacket::to_s() const {
    char buffer[200];
    sprintf(buffer, "%s\nwifu %hu %hu", IPPacket::to_s().c_str(), get_source_port(), get_destination_port());
    return buffer;
}

gcstring WiFuPacket::to_s_format() const {
    char buffer[200];
    sprintf(buffer, "%s\n# wifu source_port destination_port", IPPacket::to_s_format().c_str());
    return buffer;
}

bool WiFuPacket::operator ==(const IPPacket& other) const {
    if (!this->IPPacket::operator ==(other)) {
        return false;
    }

    WiFuPacket const* other_ptr = dynamic_cast<WiFuPacket const*> (&other);

    return other_ptr != NULL &&
            ports_->dport == other_ptr->ports_->dport &&
            ports_->sport == other_ptr->ports_->sport;
}

bool WiFuPacket::operator !=(const IPPacket& other) const {
    return !(*this == other);
}
