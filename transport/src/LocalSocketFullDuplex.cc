#include "LocalSocketFullDuplex.h"
#include "MessageStructDefinitions.h"

LocalSocketFullDuplex::LocalSocketFullDuplex(gcstring& file) : LocalSocketSender(), LocalSocketReceiver(file, this), LocalSocketReceiverCallback() {
}

LocalSocketFullDuplex::LocalSocketFullDuplex(const char* file) : LocalSocketSender(), LocalSocketReceiver(file, this), LocalSocketReceiverCallback() {

}
/*
LocalSocketFullDuplex::LocalSocketFullDuplex(gcstring& file) : LocalSocketReceiverCallback(), LocalSocketSender(), LocalSocketReceiver(file, this) {
}

LocalSocketFullDuplex::LocalSocketFullDuplex(const char* file) : LocalSocketReceiverCallback(), LocalSocketSender(), LocalSocketReceiver(file, this) {

}*/

LocalSocketFullDuplex::~LocalSocketFullDuplex() {
	perror("destructing LocalSocketFullDuplex");

}
