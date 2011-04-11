/*
 * EventTest.cpp
 *
 *  Created on: Mar 31, 2011
 *      Author: erickson
 */

#include "gtest/gtest.h"
#include "events/Event.h"
#include "Socket.h"
#include "IModule.h"

using namespace std;

namespace {

	class StubEvent : public Event {
	public:
		StubEvent() : Event() {}

		StubEvent(Socket* s) : Event(s) {}

		void execute(IModule* m) {}

	};

	TEST(Event, SocketConstructor) {
		Socket* socket = new Socket(0, 0, 0);
		StubEvent event(socket);

		ASSERT_TRUE(*socket == *event.get_socket());


		StubEvent* e1 = new StubEvent();
		StubEvent* e2 = new StubEvent();
		ASSERT_THROW(e1->less_than(e2), WiFuException);
	}

	TEST(Event, BlankConstructor) {
		StubEvent event;

		ASSERT_THROW(event.get_socket(), WiFuException);

		Socket* socket = new Socket(1, 1, 1);
		event.set_socket(socket);

		ASSERT_TRUE(*socket == *event.get_socket());


		StubEvent* e1 = new StubEvent();
		StubEvent* e2 = new StubEvent();
		ASSERT_THROW(e1->less_than(e2), WiFuException);
	}

}