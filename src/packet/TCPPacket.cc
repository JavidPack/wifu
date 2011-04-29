#include "packet/TCPPacket.h"
#include "exceptions/IllegalStateException.h"
#include "visitors/GetTCPHeaderOptionsDataVisitor.h"

TCPPacket::TCPPacket() : WiFuPacket() {
    init();
}

//TCPPacket::TCPPacket(IPPacket& p) : WiFuPacket(p) {
//    init();
//}
//
//TCPPacket::TCPPacket(unsigned char* buffer, int length) : WiFuPacket(buffer, length) {
//    init();
//}

TCPPacket::~TCPPacket() {

}

unsigned char* TCPPacket::get_data() {
    return get_next_header() + get_tcp_header_length_bytes();
}

int TCPPacket::get_data_length_bytes() {
    return get_ip_tot_length() - get_ip_header_length_bytes() - get_tcp_header_length_bytes();
}

void TCPPacket::set_data(unsigned char* data, int length) {
    if(get_data_length_bytes() > 0) {
        // Can not set the data twice
        throw IllegalStateException();
    }

    GetTCPHeaderOptionsDataVisitor visitor(get_data());
    options_.accept(&visitor);
    visitor.append_padding();

    set_tcp_data_offset(get_tcp_data_offset() + visitor.get_padded_length());

    memcpy(get_data(), data, length);
    set_ip_tot_length(get_ip_header_length_bytes() + get_tcp_header_length_bytes() + length);
}

u_int32_t TCPPacket::get_tcp_sequence_number() {
    return ntohl(tcp_->seq);
}

void TCPPacket::set_tcp_sequence_number(u_int32_t seq_num) {
    tcp_->seq = htonl(seq_num);
}

u_int32_t TCPPacket::get_tcp_ack_number() {
    return ntohl(tcp_->ack_seq);
}

void TCPPacket::set_tcp_ack_number(u_int32_t ack_num) {
    tcp_->ack_seq = htonl(ack_num);
}

int TCPPacket::get_tcp_header_length_bytes() {
    return get_tcp_data_offset() * 4;
}

u_int16_t TCPPacket::get_tcp_data_offset() {
    return tcp_->doff;
}

void TCPPacket::set_tcp_data_offset(u_int16_t length) {
    tcp_->doff = length;
}

bool TCPPacket::is_tcp_urg() {
    return tcp_->urg;
}

void TCPPacket::set_tcp_urg(bool urg) {
    tcp_->urg = urg;
}

bool TCPPacket::is_tcp_ack() {
    return tcp_->ack;
}

void TCPPacket::set_tcp_ack(bool ack) {
    tcp_->ack = ack;
}

bool TCPPacket::is_tcp_psh() {
    return tcp_->psh;
}

void TCPPacket::set_tcp_psh(bool psh) {
    tcp_->psh = psh;
}

bool TCPPacket::is_tcp_rst() {
    return tcp_->rst;
}

void TCPPacket::set_tcp_rst(bool rst) {
    tcp_->rst = rst;
}

bool TCPPacket::is_tcp_syn() {
    return tcp_->syn;
}

void TCPPacket::set_tcp_syn(bool syn) {
    tcp_->syn = syn;
}

bool TCPPacket::is_tcp_fin() {
    return tcp_->fin;
}

void TCPPacket::set_tcp_fin(bool fin) {
    tcp_->fin = fin;
}

u_int16_t TCPPacket::get_tcp_receive_window_size() {
    return ntohs(tcp_->window);
}

void TCPPacket::set_tcp_receive_window_size(u_int16_t window) {
    tcp_->window = htons(window);
}

u_int16_t TCPPacket::get_tcp_checksum() {
    return tcp_->check;
}

void TCPPacket::set_tcp_checksum(u_int16_t checksum) {
    tcp_->check = checksum;
}

u_int16_t TCPPacket::get_tcp_urgent_pointer() {
    return ntohs(tcp_->urg_ptr);
}

void TCPPacket::set_tcp_urgent_pointer(u_int16_t urg_ptr) {
    tcp_->urg_ptr = htons(urg_ptr);
}

void TCPPacket::init() {
    tcp_ = (struct tcphdr*) get_next_header();
    set_tcp_data_offset(sizeof (struct tcphdr) / 4);
    set_ip_tot_length(get_ip_header_length_bytes() + get_tcp_header_length_bytes());
}

bool TCPPacket::is_naked_ack() {
    if (is_tcp_ack() &&
            get_data_length_bytes() == 0 &&
            !is_tcp_fin() &&
            !is_tcp_psh() &&
            !is_tcp_rst() &&
            !is_tcp_syn() &&
            !is_tcp_urg()) {
        return true;
    }
    return false;
}

int TCPPacket::max_data_length() {
    return IPPacket::max_data_length() - get_tcp_header_length_bytes();
}

string TCPPacket::to_s() {
    stringstream s;
    s << IPPacket::to_s() << endl
            << "tcp "
            << get_source_port() << " "
            << get_destination_port() << " "
            << get_tcp_sequence_number() << " "
            << get_tcp_ack_number() << " "
            << get_tcp_header_length_bytes() << " "
            << is_tcp_urg() << " "
            << is_tcp_ack() << " "
            << is_tcp_psh() << " "
            << is_tcp_rst() << " "
            << is_tcp_syn() << " "
            << is_tcp_fin();
    return s.str();
}

string TCPPacket::to_s_format() {
    stringstream s;
    s << IPPacket::to_s_format() << endl
            << "# tcp sport dport seq_num ack_num header_length URG ACK PSH RST SYN FIN";
    return s.str();
}

void TCPPacket::insert_tcp_header_option(TCPHeaderOption* option) {
    if(get_data_length_bytes() > 0) {
        // Can not add options after the data has been set
        throw IllegalStateException();
    }
    
    // TODO: should we remove the (same) option if it exists before inserting it?
    options_.insert(option);
}

TCPHeaderOption* TCPPacket::remove_tcp_header_option(u_int8_t kind) {
    return options_.remove(kind);
}

TCPHeaderOption* TCPPacket::get_option(u_int8_t kind) {
     //TODO: parse options from payload if doff != sizeof(tcphdr) / 4 && options is empty


    if(options_.empty() && get_tcp_data_offset() > sizeof(tcphdr) / 4) {
        // unparsed options
        u_int8_t length = (get_tcp_data_offset() - (sizeof(tcphdr) / 4)) * 4;
        options_.parse(get_payload() + sizeof(struct iphdr) + sizeof(struct tcphdr), length);

    }

    FindTCPHeaderOptionVisitor finder(kind);
    options_.accept(&finder);
    return finder.get_option();
}