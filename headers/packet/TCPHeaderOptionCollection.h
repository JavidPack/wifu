/* 
 * File:   TCPHeaderOptionCollection.h
 * Author: rbuck
 *
 * Created on April 28, 2011, 10:31 AM
 */

#ifndef TCPHEADEROPTIONCOLLECTION_H
#define	TCPHEADEROPTIONCOLLECTION_H

#include <list>
#include <sys/types.h>
#include "GarbageCollector.h"
#include "TCPHeaderOption.h"
#include "visitors/Visitable.h"
#include "visitors/FindTCPHeaderOptionVisitor.h"
#include "visitors/GetTCPHeaderOptionsLengthVisitor.h"

using namespace std;

class TCPHeaderOptionCollection : public gc, public Visitable {
public:
    TCPHeaderOptionCollection();
    virtual ~TCPHeaderOptionCollection();

    void insert(TCPHeaderOption* option);
    TCPHeaderOption* remove(u_int8_t kind);
    bool contains(u_int8_t kind);
    void accept(Visitor* v);
    

private:
    list<TCPHeaderOption*> options_;
};

#endif	/* TCPHEADEROPTIONCOLLECTION_H */

