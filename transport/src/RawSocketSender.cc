#include "RawSocketSender.h"

RawSocketSender::RawSocketSender() {
    create_socket();
    set_header_include();
}

size_t showbytez(const void *object, size_t size)
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

ssize_t RawSocketSender::send(WiFuPacket* p) {
	printf("=========================RawSocketSender::send called\n");


printf("socket_ %i\n", socket_);
showbytez(p->get_payload(), p->get_ip_tot_length());
printf("p->get_ip_tot_length() %i\n", p->get_ip_tot_length());

    int ret = sendto(socket_,
            p->get_payload(),
            p->get_ip_tot_length(),
            0,
            (struct sockaddr*) p->get_dest_address_port()->get_network_struct_ptr(),
            (sizeof (struct sockaddr_in)));

    if (ret < 0) {
        perror("RawSocketSender: Error Sending Packet");
        // TODO: What should we do on a fail?
    }
    return ret;
}

void RawSocketSender::create_socket() {
	printf("RawSocketSender::create_socket() called");
    socket_ = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (socket_ < 0) {
        perror("NetworkInterface cannot create raw socket");
        exit(EXIT_FAILURE);
    }
}

void RawSocketSender::set_header_include() {
    int one = 1;
    const int *val = &one;
    if (setsockopt(socket_, IPPROTO_IP, IP_HDRINCL, val, sizeof (one)) < 0) {
        perror("NetworkInterface: cannot set HDRINCL");
        exit(EXIT_FAILURE);
    }
}
