/* 
 * File:   main.cc
 * Author: rbuck
 *
 * Created on October 18, 2010, 1:47 PM
 */

#include <stdlib.h>
#include <iostream>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdexcept>
#include <string.h>
#include <assert.h>
#include <signal.h>

#include "Queue.h"
#include "PriorityQueue.h"
#include "events/TimeoutEvent.h"
#include "TimeoutEventManager.h"
#include "Semaphore.h"
#include "events/CanceledEvents.h"
#include "Dispatcher.h"
#include "events/ConnectEvent.h"
#include "Socket.h"
#include "UDPInterface.h"
#include "defines.h"
#include "events/PacketReceivedEvent.h"
#include "WifuEndBackEndLibrary.h"
#include "MainSemaphore.h"
#include "ProtocolManager.h"
#include "SimpleTCP.h"
#include "../headers/events/SendPacketEvent.h"

#include "events/SendPacketEvent.h"
#include "events/UDPSendPacketEvent.h"
#include "events/UDPReceivePacketEvent.h"
#include "events/TimeoutEvent.h"
#include "events/TimerFiredEvent.h"
#include "events/AcceptEvent.h"

using namespace std;

#define dispatcher Dispatcher::instance()

void main_signal_manager(int signal) {
    switch(signal) {
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
            MainSemaphore::instance().post();
    }
}

void register_signals(){
    signal(SIGINT, main_signal_manager);
    signal(SIGQUIT, main_signal_manager);
    signal(SIGTERM, main_signal_manager);
}

void register_protocols() {
    // TODO: figure out a better way to register protocols via a config file
    ProtocolManager::instance().support(SIMPLE_TCP);
}

int main(int argc, char** argv) {
	GC_INIT();

    //TODO: Change second argument to 0 once we have a logger in place
    daemon(1,1);

    MainSemaphore::instance().init(0);

    register_signals();
    register_protocols();

    // TODO: change this to be either an option passed in or INADDR_ANY
    // In the latter case we will need ensure that we have compatability throughout.
    // (It is likely that the SocketCollection will break.)
    // It is also likely that we will need to figure out how to query the kernel for
    // the local address as well.
    // INADDR_ANY == 0.0.0.0
    string address("127.0.0.1");
    int port = WIFU_PORT;

    AddressPort ap(address, port);

    // Start Dispatcher
    Dispatcher::instance().start_processing();

    // Start Back end
    WifuEndBackEndLibrary::instance();

    // Load Modules
    UDPInterface::instance().start(ap);

    dispatcher.map_event(type_name(UDPSendPacketEvent), &UDPInterface::instance());
    dispatcher.map_event(type_name(TimeoutEvent), &TimeoutEventManager::instance());
    dispatcher.map_event(type_name(CancelTimerEvent), &TimeoutEventManager::instance());

    dispatcher.map_event(type_name(SocketEvent), &SimpleTCP::instance());
    dispatcher.map_event(type_name(BindEvent), &SimpleTCP::instance());
    dispatcher.map_event(type_name(ListenEvent), &SimpleTCP::instance());
    dispatcher.map_event(type_name(ConnectEvent), &SimpleTCP::instance());
    dispatcher.map_event(type_name(AcceptEvent), &SimpleTCP::instance());
    dispatcher.map_event(type_name(SendPacketEvent), &SimpleTCP::instance());
    dispatcher.map_event(type_name(UDPReceivePacketEvent), &SimpleTCP::instance());
    dispatcher.map_event(type_name(TimerFiredEvent), &SimpleTCP::instance());
    
    dispatcher.map_event(type_name(ResponseEvent), &WifuEndBackEndLibrary::instance());
    
    // Wait indefinitely
    MainSemaphore::instance().wait();

    return (EXIT_SUCCESS);
}

