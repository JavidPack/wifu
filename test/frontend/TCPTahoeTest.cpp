#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>

#include "gtest/gtest.h"
#include "../applib/wifu_socket.h"
#include "../headers/defines.h"
#include "../headers/AddressPort.h"
#include "../headers/GarbageCollector.h"
#include "../headers/Semaphore.h"
#include "Timer.h"
#include "../headers/RandomStringGenerator.h"

#include "../headers/BackEndTest.h"

#include "../headers/PacketLogReader.h"
#include "../headers/packet/TCPPacket.h"
#include "../headers/PacketTraceHelper.h"

void* tcp_tahoe_test_thread(void* args) {

    struct var* v = (struct var*) args;
    AddressPort* to_bind = v->to_bind_;
    Semaphore* sem = v->sem_;

    // Create server
    int server = wifu_socket(AF_INET, SOCK_STREAM, SIMPLE_TCP);
    int result = wifu_bind(server, (const struct sockaddr *) to_bind->get_network_struct_ptr(), sizeof (struct sockaddr_in));
    EXPECT_EQ(0, result);
    result = wifu_listen(server, 5);
    EXPECT_EQ(0, result);
    sem->post();

    struct sockaddr_in addr;
    socklen_t length = sizeof (addr);
    int connection;
    if ((connection = wifu_accept(server, (struct sockaddr *) & addr, &length)) < 0) {
        // TODO: we need to check errors and make sure they happen when they should
        ADD_FAILURE() << "Problem in Accept";
    }

    AddressPort ap(&addr);
    string address("127.0.0.1");
    string res = ap.get_address();
    EXPECT_EQ(address, res);
    //    cout << "Connected to: " << ap.to_s() << endl;

}

void tcp_tahoe_connect_test() {
    AddressPort to_connect("127.0.0.1", 5002);

    pthread_t t;
    struct var v;
    Timer timer;
    int client;
    int result;


    v.sem_ = new Semaphore();
    v.sem_->init(0);
    v.to_bind_ = new AddressPort("127.0.0.1", 5002);


    if (pthread_create(&t, NULL, &tcp_tahoe_test_thread, &v) != 0) {
        FAIL();
    }

    v.sem_->wait();

    // Make sure that the thread is in the accept state
    usleep(50000);

    // Create client

    timer.start();
    client = wifu_socket(AF_INET, SOCK_STREAM, SIMPLE_TCP);
    result = wifu_connect(client, (const struct sockaddr *) to_connect.get_network_struct_ptr(), sizeof (struct sockaddr_in));
    timer.stop();
    ASSERT_EQ(0, result);

    //    cout << "Duration (us) to create a socket and connect on localhost via wifu: " << timer.get_duration_microseconds() << endl;
    sleep(5);
}

TEST_F(BackEndTest, TCPTahoeConnectTest) {
    tcp_tahoe_connect_test();
}

void tcp_tahoe_drop_none() {
    tcp_tahoe_connect_test();
    NetworkTrace expected;

    // send
    expected.add_packet(get_syn());
    // receive
    expected.add_packet(get_syn());


    // send
    expected.add_packet(get_synack());
    // receive
    expected.add_packet(get_synack());


    // send
    expected.add_packet(get_ack());
    // receive
    expected.add_packet(get_ack());

    compare_traces(expected);
}

TEST_F(BackEndTest, TCPTahoeSocketTest) {

    for (int i = 0; i < 100; i++) {
        // Check valid
        int socket = wifu_socket(AF_INET, SOCK_STREAM, TCP_TAHOE);
        ASSERT_TRUE(socket >= 0);

        // Check invalid (i != TCP_TAHOE)
        socket = wifu_socket(AF_INET, SOCK_STREAM, i);
        ASSERT_TRUE(socket == -1);
    }
}

TEST_F(BackEndMockTestDropNone, TCPTahoeMockConnectTest) {
    tcp_tahoe_drop_none();
}

