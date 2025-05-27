#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <pthread.h>
#include <stdint.h>

#define BUFFER_SIZE 16

typedef struct {
	int joystick_idx; //No of ring buffers used 0/1
	uint8_t type;
	uint8_t number;
	uint16_t value; /* axis/button number */
} JoystickEvent;

typedef struct {
	JoystickEvent events[BUFFER_SIZE];
	int head;
	int tail;
	int count;
	pthread_mutex_t mutex;
} RingBuffer;

static RingBuffer event_buffer = {
	.head=0,
	.tail=0,
	.count=0,
	.mutex=PTHREAD_MUTEX_INITIALIZER//Fast Mutex
};

#endif
