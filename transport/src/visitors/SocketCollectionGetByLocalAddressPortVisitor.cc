#include "visitors/SocketCollectionGetByLocalAddressPortVisitor.h"

SocketCollectionGetByLocalAddressPortVisitor::SocketCollectionGetByLocalAddressPortVisitor(AddressPort* local) : local_(local) {
}

SocketCollectionGetByLocalAddressPortVisitor::~SocketCollectionGetByLocalAddressPortVisitor() {
}

void SocketCollectionGetByLocalAddressPortVisitor::visit(Socket* s) {
//printf("visit%u\n", local_->get_port());
//printf("visit%u\n", s->get_local_address_port()->get_port());
//printf("visit%s\n", local_->get_address().c_str());
//printf("visit%s\n", s->get_local_address_port()->get_address().c_str());

    if (local_->get_port() == s->get_local_address_port()->get_port() ) {
	//printf("A\n");
    	if (strcmp (local_->get_address().c_str() , s->get_local_address_port()->get_address().c_str()) == 0 || strcmp( s->get_local_address_port()->get_address().c_str() ,"0.0.0.0" ) == 0) {


		//printf("set_socket(s);\n");
        	set_socket(s);
	}
    }

 //   if ((*s->get_local_address_port()) == (*local_)/* && (*s->get_remote_address_port()) == (*AddressPort::default_address_port())*/) {
//    if (local_->get_port() == s->get_local_address_port()->get_port() && ((local_->get_address().c_str() == s->get_local_address_port()->get_address().c_str())||(s->get_local_address_port()->get_address().c_str() == "0.0.0.0")) ) {
//	printf("set_socket(s);\n");
 ///       set_socket(s);

 //   }
}
