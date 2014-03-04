#include "events/framework_events/DataEvent.h"

DataEvent::DataEvent() : LibraryEvent() {

}

DataEvent::~DataEvent() {

}

size_t DataEvent::get_data_length() {
printf("get_data_lenght in dataevent\n");
    return ((struct DataMessage*) get_buffer())->buffer_length;
}

int DataEvent::get_flags() {
printf("get_flags in dataevent\n");
    return ((struct DataMessage*) get_buffer())->flags;
}
