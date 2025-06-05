#include "ring_buffer.h"


static RingBuffer event_buffer = {
        .head=0,
        .tail=0,
        .count=0,
        .mutex=PTHREAD_MUTEX_INITIALIZER//Fast Mutex
};
