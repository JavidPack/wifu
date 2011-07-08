/*
 * PacketLogger.h
 *
 *  Created on: Mar 8, 2011
 *      Author: erickson
 */

#ifndef PACKETLOGGER_H_
#define PACKETLOGGER_H_

#include <fstream>
#include <stdint.h>
#include <sys/time.h>
#include <list>

#include "packet/TCPPacket.h"

#include "packet/WiFuPacket.h"
#include "exceptions/IOError.h"
#include "defines.h"
#include "BinarySemaphore.h"
#include "PacketLoggerItem.h"

#define LOG_FILENAME "wifu-log.pcap"

using namespace std;

/**
 * These structs actually calls for guint32's and gint32 in glib.h
 * (as seen here: http://wiki.wireshark.org/Development/LibpcapFileFormat),
 * but this machine doesn't appear to have that header so I'm using uint32_t
 * instead.
 */
struct PcapFileHeader {
	uint32_t magic_number;   /* magic number */
	uint16_t version_major;  /* major version number */
	uint16_t version_minor;  /* minor version number */
	int32_t  thiszone;       /* GMT to local correction */
	uint32_t sigfigs;        /* accuracy of timestamps */
	uint32_t snaplen;        /* max length of captured packets, in octets */
	uint32_t network;        /* data link type */
};
struct PcapPacketHeader {
	uint32_t ts_sec;         /* timestamp seconds */
	uint32_t ts_usec;        /* timestamp microseconds */
	uint32_t incl_len;       /* number of octets of packet saved in file */
	uint32_t orig_len;       /* actual length of packet */
};


class PacketLogger {
public:
	static PacketLogger& instance();

	~PacketLogger();

	void log(WiFuPacket* packet);

        void flush();

        /**
         * Determines when to flush all saved packets to disk every count packets
         * Default is flush count of 1 (write each packet to disk) and should be changed to perform better.
         * @param count The number of packets to save before flushing to disk
         */
        void set_flush_value(int count);

private:
	PacketLogger();

	void close_log();

	void write_file_header();

        

	const char* file_name_;
	ofstream fileout_;
	BinarySemaphore lock_;

        list<PacketLoggerItem*, gc_allocator<PacketLoggerItem*> > items_;
        int flush_count_;

};

#endif /* PACKETLOGGER_H_ */
