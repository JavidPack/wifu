/*
 * main.cc
 *
 *  Created on: Dec 27, 2010
 *      Author: erickson
 */

#include "gtest/gtest.h"
#include "defines.h"
#include "OptionParser.h"
#include <signal.h>

#include "Logger.h"

const PAN_CHAR_T PANTHEIOS_FE_PROCESS_IDENTITY[] = "wifu-integration-tests";

using namespace std;

void change_dir() {
    int size = 1000;
    char buf[size];
    getcwd(buf, size);
    gcstring path = buf;
    gcstring bin = "bin";

    if (path.find(bin) == gcstring::npos) {
        chdir(bin.c_str());
    }
}



int main(int argc, char** argv) {
    //change_dir();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
