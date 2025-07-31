#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include <linux/joystick.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h> //Contains file controls like O_RDWR
#include <errno.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include <inttypes.h>
#include "wsServer.h"
#include "utilities.h"
#include "crc.h"
#include "dll.h"
#include "aesctr.h"
//#include "aesgcm.h"
//#include "create_packet.h"

#define MAX_JOYSTICKS 1
#define MAX_EVENTS 2
#define JOYSTICK_HEADER_LEN 2
#define MAX_ULINK_PACKET_SIZE 4096


//volatile sig_atomic_t stop_flag=0;

//Function Prototypes
void *event_thread(void *arg);
int Init_Uart(speed_t baud);
void *send_thread(void *arg);
void Init_WSSocket(void);

typedef struct {

    char name[128];
    int16_t *axes; //most cost efficeint access to memory is accessing int sized data
    uint8_t *buttons; //points to button data within data , byte array each byte stores state of 8 buttons
    int number_of_axes;
    int number_of_buttons;
    int buttonsLen;
    uint8_t *data; //starting pointer to combined buttons and axes data i.e combined buffer
    //int total_bytes;
} JoystickState;

int CreatePacket(JoystickState *js0,uint8_t ulinkBuf[MAX_ULINK_PACKET_SIZE]);

static int joystick_fds[MAX_JOYSTICKS];
static JoystickState joysticks[MAX_JOYSTICKS];


#endif
