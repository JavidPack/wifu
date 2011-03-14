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

TEST_F(BackEndTest, socketTest) {
    // socket
    for (int i = 0; i < 100; i++) {
        // Check valid
        int socket = wifu_socket(AF_INET, SOCK_STREAM, SIMPLE_TCP);
        ASSERT_TRUE(socket >= 0);

        // Check invalid (i != SIMPLE_TCP)
        socket = wifu_socket(AF_INET, SOCK_STREAM, i);
        ASSERT_TRUE(socket == -1);
    }
}