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


#define MAX_JOYSTICKS 2
#define MAX_EVENTS 2

volatile sig_atomic_t stop_flag=0;

//Function Prototypes
void *event_thread(void *arg);
int Init_Uart(speed_t baud);
void *send_thread(void *arg);
void Init_WSSocket(void);

typedef struct {
    char name[128];
    int16_t axes[8];
    uint8_t buttons[32];
    int number_of_axes;
    int number_of_buttons;
} JoystickState;


static int joystick_fds[MAX_JOYSTICKS];
static int num_joysticks=0;
static JoystickState joysticks[MAX_JOYSTICKS];
static long int serial_fd1 =-1;
static int timer_fd=-1;
static int epoll_fd =-1;
//start routine for event thread
//performs event based joystick updates

static pthread_mutex_t joystick_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static char *itimerspec_dump(struct itimerspec *ts);
static uint64_t m_interval = 10;

int Init_Uart(speed_t baud){
        int serial_port=open("/dev/serial0",O_RDWR);
        if (serial_port < 0) {
                printf("Error %i from open: %s\n", errno,strerror(errno));
		return -1;
        }
        struct termios tty;
        if(tcgetattr(serial_port, &tty) !=0) {
                printf("Error %i from tcgetattr: %s\n", errno,strerror(errno));
                return -1;
        }//tcgetattr() gets the parameters associated with the object referred by fd
	tty.c_cflag &= ~CSTOPB; //Clear the stop bit, only one stop bit is used in communication
        tty.c_cflag &= ~CSIZE; //Clear all bits that set the data size
        tty.c_cflag |=CS8; //8 bits per byte
        tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
        tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
        tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)

	//Local Modes (c_Iflag)
	tty.c_lflag &= ~ICANON; //Disable canonical mode
	tty.c_lflag &= ~ECHO; // Disable echo
	tty.c_lflag &= ~ECHOE; // Disable erasure
	tty.c_lflag &= ~ECHONL; // Disable new-line echo
	tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP

	//Input flags
	tty.c_iflag &= ~(IXON | IXOFF | IXANY); //Turn off s/w flow control
	tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
	//Output Modes c_oflag
	tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
	//tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

	cfsetispeed(&tty,baud);
	cfsetospeed(&tty,baud);

	//Save tty settings , also checking for error
	if(tcsetattr(serial_port,TCSANOW,&tty) !=0) {
		printf("Error %i from tcsetattr: %s\n",errno,strerror(errno));
		return -1;
	}
	//return 0; //Success
	return serial_port;
}
void *event_thread(void *arg){
         printf("hello from T1\n");
         struct js_event e;
         while(1){
                for(int i=0;i<num_joysticks;i++){
	            while (read(joystick_fds[i], &e, sizeof(e)) > 0) {
                                if(e.type & JS_EVENT_INIT) continue;
				pthread_mutex_lock(&joystick_mutex);
                                switch(e.type & ~JS_EVENT_INIT){
                                    case JS_EVENT_AXIS:
                                        if(e.number <8){
                                            joysticks[i].axes[e.number]=e.value;
                                            printf("%s AXIS %d value: %d\n",joysticks[i].name,e.number,e.value);
                                        }
                                        break;
                                    case JS_EVENT_BUTTON:
					if(e.number <32){
					    joysticks[i].buttons[e.number]=e.value;
					    printf("%s BUTTON %d value:%d\n",joysticks[i].name,e.number,e.value);
                                        }
					break;
                                }
                                pthread_mutex_unlock(&joystick_mutex);

                    }
                }
                usleep(1000);
         }
	return NULL;
}

void Init_Epoll_Timerfd(void){
  /*epoll_fd =epoll_create(1);
        if(epoll_fd<0){
                perror("epoll_create");
                exit(EXIT_FAILURE);
        }
 	printf("created epollfd %d\n", epoll_fd);*/

	timer_fd=timerfd_create(CLOCK_MONOTONIC,0);
 	if(timer_fd==-1) {
                printf("timerfd_create() failed: error=%d\n", errno);
                exit(EXIT_FAILURE);
        }
        printf("created timerfd %d\n",timer_fd);
        fcntl(timer_fd, F_SETFL, O_NONBLOCK);

}

//Sets up the WebSocket server to listen on 0.0.0.0:9000 with the event handlers.
void Init_WSSocket(void)
{
	ws_socket(&(struct ws_server){
		.host = "0.0.0.0",
		.port = 9000,
		.thread_loop   = 1,
		.timeout_ms    = 1000,
		.evs.onopen    = &onopen,
		.evs.onclose   = &onclose,
		.evs.onmessage = &onmessage
	});
}

void *send_thread(void *arg) {
        long int a = (long int)arg;
	uint64_t res;
	struct epoll_event ev,events[MAX_EVENTS];
        struct itimerspec new_value;
        new_value.it_value.tv_sec = m_interval / 1000;
        new_value.it_value.tv_nsec = (m_interval % 1000) * 1000000;
        new_value.it_interval.tv_sec = m_interval / 1000;
        new_value.it_interval.tv_nsec = (m_interval % 1000) * 1000000;

        /*if(timerfd_settime(timer_fd,0,&new_value,NULL)<0) {
                printf("timerfd_settime() failed: errno=%d\n",errno);
                close(timer_fd);
                exit(EXIT_FAILURE);
        }
        printf("set timerfd time=%s\n",itimerspec_dump(&new_value));
	*/


	/* Creating an Epoll Instance */

	epoll_fd = epoll_create(1);
	if(epoll_fd==-1) {
		printf("epoll_create() failed: errno=%d\n", errno);
		close(timer_fd);
		exit(EXIT_FAILURE);
	}
	printf("created epollfd %d\n", epoll_fd);

	/* Register interest in particular file descriptors */

	//ev.events = EPOLLIN; //The associated file is available for read(2) operations.

	ev.events=EPOLLIN ; // Interested in readability
	ev.data.fd=timer_fd;
	//events[0].data.fd=timer_fd;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD,timer_fd,&ev)==-1) {
		printf("epoll_ctl(ADD) failed for timer_fd: errno=%d\n",errno);
		close(epoll_fd);
		close(timer_fd);
		exit(EXIT_FAILURE);
	}
	printf("added timerfd to epoll set\n");



	/*ev.events=EPOLLIN | EPOLLOUT |EPOLLET;
	events[1].data.fd=serial_fd1;


	if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,serial_fd1,&ev)) {
		printf("epoll_ctl(ADD) for serial failed:errno=%d\n",errno);
		close(epoll_fd);
		close(serial_fd1);
		exit(EXIT_FAILURE);

	}
	printf("added serialfd to epoll interested set\n");*/

	sleep(1);
	/*if(timerfd_settime(timer_fd,0,&new_value,NULL)<0) {
                printf("timerfd_settime() failed: errno=%d\n",errno);
                close(timer_fd);
                exit(EXIT_FAILURE);
        }*/
	int ret;
	memset(&ev,0,sizeof(ev));
	//unsigned char msg[] ={'A','\r','\n'};
	char buffer[128];
	if(timerfd_settime(timer_fd,0,&new_value,NULL)<0) {
                printf("timerfd_settime() failed: errno=%d\n",errno);
                close(timer_fd);
                exit(EXIT_FAILURE);
        }
	while(1){

	ret = epoll_wait(epoll_fd,&ev,1,10); //10ms
	//printf("%d\n",ret);

	if(ret < 0 ) {
		printf("epoll_wait() failed: error=%d\n",errno);
		close(epoll_fd);
		close(timer_fd);
		exit(EXIT_FAILURE);
	}
	printf("waited on epoll,ret=%d\n",ret);
        printf("%d ready events\n", ret);


	ret = read(timer_fd,&res,sizeof(res));
	if(ret!=sizeof(uint64_t)){
		exit(EXIT_FAILURE);
	}
	//ret = read(timer_fd,&res,sizeof(res));
	printf("read() returned %d, res=%" PRIu64 "\n", ret, res);
	if(res==1){

        //unsigned char msg[] ={'A'};
		pthread_mutex_lock(&joystick_mutex);
		//for(int i=0;i<num_joysticks;i++) {
			//joysticks[i].number_of_axes
		for(int j=0;j<3;j++){
			snprintf(buffer,sizeof(buffer),"%s Axis %d value %d\r\n","JS0",j,joysticks[0].axes[j]);
                        //snprintf(buffer,sizeof(buffer),"Axis %d value %d\r\n",j,joysticks[0].axes[j]);

  		      	write(serial_fd1,buffer,strlen(buffer));
			send_joystick_data(buffer);
		}
		for(int j=0;j<2;j++){
			snprintf(buffer,sizeof(buffer),"%s Buttons %d value %d\r\n","JS1",j,joysticks[0].buttons[j]);
                        //snprintf(buffer,sizeof(buffer),"Buttons %d value %d\r\n",j,joysticks[0].buttons[j]);
			write(serial_fd1,buffer,strlen(buffer));
			send_joystick_data(buffer);
		}
		//}
		pthread_mutex_unlock(&joystick_mutex);

	}
			//close(serial_fd1);
    			//serial_fd1=Init_Uart(B115200);

	}
	                close(serial_fd1);





	if (close(epoll_fd) == -1) {
		printf("failed to close epollfd: errno=%d\n", errno);
		exit(EXIT_FAILURE);
	}

	if (close(timer_fd) == -1) {
		printf("failed to close timerfd: errno=%d\n", errno);
		exit(EXIT_FAILURE);
	}
	return NULL;

}



int main(){
//Joystick Initialization
    char path[16];
    char number_of_buttons,number_of_axes;
    char joystick_name[128];
    int status_1,status_2;
    for(int i=0;i<MAX_JOYSTICKS;i++){
        //We need to write size bytes to str
        snprintf(path,sizeof(path),"/dev/input/js%d",i);
        int fd=open(path,O_RDONLY|O_NONBLOCK);
        if(fd<0) break;
        joystick_fds[i] = fd;
        num_joysticks++;
        ioctl (fd,JSIOCGAXES,&joysticks[i].number_of_axes);
        ioctl (fd,JSIOCGBUTTONS,&joysticks[i].number_of_buttons);
        if(ioctl(fd,JSIOCGNAME(sizeof(joysticks[i].name)),joysticks[i].name)<0){
		strncpy(joysticks[i].name,"Unknown",sizeof(joysticks[i].name));
        }
        printf("Name %s\n",joysticks[i].name);
        printf("%d\n",joystick_fds[i]);
    }

    if(num_joysticks == 0) {
        printf("No joysticks found\n");
        //return 1;
    }
    printf("%d\n",num_joysticks);
    Init_WSSocket();
    serial_fd1=Init_Uart(B115200);
    if(serial_fd1<0){
	printf("FAILURE");
    }
    pthread_t event_tid,send_tid;
    status_1=pthread_create(&event_tid,NULL,event_thread,NULL);
    if(status_1<0) {
	fprintf(stderr,"Error - pthread_create() Thread 1 return code: %d\n",status_1);
        exit(EXIT_FAILURE);
    }
    Init_Epoll_Timerfd();
    status_2=pthread_create(&send_tid,NULL,send_thread,(void *)(serial_fd1));
    if(status_2<0) {
        fprintf(stderr,"Error - pthread_create() Thread 2 return code: %d\n",status_2);
        exit(EXIT_FAILURE);
    }

    /*else{
	unsigned char msg[] ={'A','B'};
	while(1){
		sleep(1);
        	write(serial_fd1,msg,sizeof(msg));
        }
	close(serial_fd1);
    }*/
    pthread_join(event_tid,NULL);

    pthread_join(send_tid,NULL);
    return 0;
}

static char * itimerspec_dump(struct itimerspec *ts)
{
	static char buf[1024];

	snprintf(buf,sizeof(buf),
		"itimer: [interval=%lu s %lu ns,next expire=%lu s %lu ns ]",
		ts->it_interval.tv_sec,
		ts->it_interval.tv_nsec,
		ts->it_value.tv_sec,
		ts->it_value.tv_nsec
		);
	return (buf);
}
