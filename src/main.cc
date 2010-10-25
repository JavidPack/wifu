/* 
 * File:   main.cc
 * Author: rbuck
 *
 * Created on October 18, 2010, 1:47 PM
 */

#include <stdlib.h>
#include <iostream>
#include <semaphore.h>

#include "LocalSocketFullDuplex.h"
#include "Reliability.h"
#include "ConnectionManager.h"
#include "Dispatcher.h"
#include "Socket.h"
#include "Queue.h"
#include "PriorityQueue.h"
#include "TimeoutEvent.h"
#include "TimeoutEventManager.h"
#include "SingletonTemplate.h"
#include "Semaphore.h"
#include "CanceledEvents.h"

using namespace std;

/*
 * 
 */

int main(int argc, char** argv) {

    

    string address("localhost");
    string socket_file("/tmp/socket_file");
    Socket s(socket_file);

    s.connect(address);

//    Queue<int> q;
//    q.enqueue(9);
//    cout << q.size() << endl;
//    cout << q.dequeue() << endl;

    PriorityQueue<TimeoutEvent *, TimeoutEventComparator> queue;

    TimeoutEvent * one = new TimeoutEvent(1, 0);
    TimeoutEvent * oneplus = new TimeoutEvent(1, 0);
    TimeoutEvent * two = new TimeoutEvent(2, 0);
    TimeoutEvent * three = new TimeoutEvent(3, 0);
    TimeoutEvent * four = new TimeoutEvent(4, 0);

//    queue.enqueue(four);
//    queue.enqueue(one);
//    queue.enqueue(three);
//    queue.enqueue(two);
//
//    while(!queue.isEmpty()) {
//        cout << queue.dequeue() << endl;
//    }


    sleep(1);

    TimeoutEventManager manager;

    manager.enqueue(two);
    manager.enqueue(three);
    manager.enqueue(four);
    manager.enqueue(one);
    manager.enqueue(oneplus);

    manager.cancel(two);
    //manager.cancel(three);
    manager.cancel(four);
    //manager.cancel(one);
    manager.cancel(oneplus);

    
    
    //    string file("/tmp/file");
    //    Reliability r(file);
    //
    //    string message("123456789");
    //    r.send_to(file, message);
    //    r.send_to(file, file);
    //
    //    string manager_file("/tmp/manager_file");
    //    ConnectionManager manager(manager_file);
    //
    //    string manager_message("FromManager");
    //    manager.send_to(file, manager_message);
    //
    //    cout << Dispatcher::instance()->getFile() << endl;
    //
    sleep(5);

    cout << "hi" << endl;

    Dispatcher::instance()->destroy();

    return (EXIT_SUCCESS);
}

