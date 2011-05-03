/* 
 * File:   PacketTraceHelper.h
 * Author: rbuck
 *
 * Created on April 18, 2011, 12:16 PM
 */

#ifndef PACKETTRACEHELPER_H
#define	PACKETTRACEHELPER_H

#include "gtest/gtest.h"
#include "../headers/PacketLogReader.h"
#include "../headers/packet/TCPPacket.h"
#include "../headers/defines.h"
#include "../headers/packet/TCPTimestampOption.h"

void compare_traces(NetworkTrace& expected);

TCPPacket* get_base_tcp_packet();
TCPPacket* get_base_tcp_packet_ts();
TCPPacket* get_syn();
TCPPacket* get_synack();
TCPPacket* get_ack();

#endif	/* PACKETTRACEHELPER_H */

