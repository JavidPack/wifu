#include "AddressPort.h"

AddressPort::AddressPort(string& address, uint16_t port) : address_(address), port_(port) {
	init(address.c_str(), port);
}

AddressPort::AddressPort(const char* address, uint16_t port) : address_(string(address)), port_(port) {
	init(address, port);
}

AddressPort::AddressPort(struct sockaddr_in* obj) {
	assert(obj);

	char ip_address[INET_ADDRSTRLEN];
	u_int32_t saddr = obj->sin_addr.s_addr;
	inet_ntop(AF_INET, &saddr, ip_address, INET_ADDRSTRLEN);

	address_ = string(ip_address);
	port_ = ntohs(obj->sin_port);

	memcpy(&data_, obj, sizeof (struct sockaddr_in));
}

AddressPort::AddressPort(const AddressPort& original) : address_(original.address_), port_(original.port_) {
	memcpy(&data_, &original.data_, sizeof (struct sockaddr_in));
}

AddressPort& AddressPort::operator=(const AddressPort& original) {
	if (this != &original) {
		address_ = original.address_;
		port_ = original.port_;
		memcpy(&data_, &original.data_, sizeof (struct sockaddr_in));
	}
	return *this;
}

bool AddressPort::operator==(const AddressPort& other) const {
	return address_ == other.address_ && port_ == other.port_;
}

bool AddressPort::operator!=(const AddressPort& other) const {
	return !operator ==(other);
}

AddressPort::~AddressPort() {}

string& AddressPort::get_address() {
	return address_;
}

uint16_t AddressPort::get_port() {
	return port_;
}

struct sockaddr_in& AddressPort::get_network_struct() {
	return data_;
}

struct sockaddr_in* AddressPort::get_network_struct_ptr() {
	return &data_;
}

string AddressPort::to_s() {
	stringstream s;
	s << "Address: " << get_address() << " ";
	s << "Port: " << get_port();
	return s.str();
}

void AddressPort::init(const char* address, uint16_t port) {
	data_.sin_family = AF_INET;
	data_.sin_port = htons(port);

	if (!inet_aton(address, &data_.sin_addr))
		throw InvalidAddressException();
}

bool AddressPortComparator::operator()(AddressPort* const& ap1, AddressPort* const& ap2) {
	return ap1->to_s() < ap2->to_s();
}