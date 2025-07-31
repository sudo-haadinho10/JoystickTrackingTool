#include "main.h"
#include <openssl/bio.h>

static int num_joysticks=0;
static long int serial_fd1 =-1;
static int timer_fd=-1;
static int epoll_fd =-1;
static int no_of_packets_sent = 0;

//start routine for event thread
//performs event based joystick updates

static pthread_mutex_t joystick_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static char *itimerspec_dump(struct itimerspec *ts);
static uint64_t m_interval = 10; //change for ms



static const unsigned char key [32] = {
                0x2B,0x7E,0x15,0x16,
                0x28,0xAE,0xD2,0xA6,
                0xAB,0xF7,0x15,0x88,
                0x09,0xCF,0x4F,0x3C,

                0xA0,0xFA,0xFE,0x17,
                0x88,0x54,0x2C,0xB1,
                0x23,0xA3,0x39,0x39,
                0x2A,0x6C,0x76,0x05
        };


//static const unsigned char key[5] = "hello";

/* Unique initialisation vector */
        /* Must be unique for every encryption with the same key */

        const unsigned char gcm_iv[] = {
                0x99, 0xaa, 0x3e, 0x68, 0xed, 0x81, 0x73, 0xa0, 0xee, 0xd0, 0x66, 0x84,
		0x99, 0xaa, 0x84, 0x85
        };

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
	printf("UART SERIAL_FD %d\n",serial_port);
	return serial_port;
}

void *event_thread(void *arg){
         printf("hello from T1\n");
         struct js_event e;
	 int16_t mappedAxisValue;
         while(1){
                //for(int i=0;i<num_joysticks;i++){

		  //--------------
	            while (read(joystick_fds[0], &e, sizeof(e)) > 0) {

				pthread_mutex_lock(&joystick_mutex);

				if(e.type & JS_EVENT_INIT) {
					//continue;
					if(e.type & ~JS_EVENT_AXIS == JS_EVENT_AXIS) {

						if(e.number<=4) {
							mappedAxisValue = AxesValue(e.number,e.value);
							joysticks[0].axes[e.number]=mappedAxisValue;
                                        		printf("%s %s %d value:%d\n",joysticks[0].name,"AXIS",e.number,mappedAxisValue);
						}
						e.type&=~JS_EVENT_INIT;
					}
					//pthread_mutex_unlock(&joystick_mutex);
				}
				//Addition  1
				//end of addition 1

				// Addition 2
				//pthread_mutex_lock(&joystick_mutex);
				//for(int i=0;i<4;i++) {
				if((e.type & ~JS_EVENT_INIT) ==JS_EVENT_AXIS) {
					//joysticks[0].axes[e.number]=e.value;
					if(e.number<4) {
					mappedAxisValue = AxesValue(e.number,e.value);
					joysticks[0].axes[e.number] = mappedAxisValue;
					printf("%s %s %d value:%d\n",joysticks[0].name,"AXIS",e.number,mappedAxisValue);
					}
				}

				if((e.type &~JS_EVENT_INIT) ==JS_EVENT_BUTTON) {
					//button 0 is the most significant bit (pos 7)
					//button 1 is pos 6
					//...
					//button 8 is the msb of next byte
					//joysticks[0].buttons[e.number]=e.value;
					//button is pressed on
					if(e.value) {
						if(e.value <=2) {
							joysticks[0].buttons[e.number/8] |= (1<<(7-e.number%8)); //big endian
							printf("%s %s %d value:%d\n",joysticks[0].name,"BUTTON",e.number,e.value);

						}
					}
					else {
						//button is pressed off
						joysticks[0].buttons[e.number/8] &=~(1<<(7-e.number%8));
					}
					//printf("%s %s %d value:%d\n",joysticks[0].name,"BUTTON",e.number,e.value);
				}
				if(errno !=EAGAIN) {
					//fprintf(stderr,"QUEUE EMPTY");
				}

                                pthread_mutex_unlock(&joystick_mutex);
                    }
                //}
                //usleep(1000);

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
	//
	//uint8_t ulinkBuf[MAX_ULINK_PACKET_SIZE];
	unsigned char ulinkBuf[MAX_ULINK_PACKET_SIZE];
	int len; //length of actual bytes in ulinkBuf
	int dlllen;
	//

	unsigned char ciphertext[128];
	unsigned char decrypted_text[128];
	unsigned char tag[16];

	int ciphertext_len;
	int decrypted_len;

        long int a = (long int)arg;
	uint64_t res=0;
	struct epoll_event ev,events[MAX_EVENTS];
        struct itimerspec new_value;
        new_value.it_value.tv_sec = m_interval / 1000;
        new_value.it_value.tv_nsec = (m_interval % 1000) * 1000000;
        new_value.it_interval.tv_sec = m_interval / 1000;
        new_value.it_interval.tv_nsec = (m_interval % 1000) * 1000000;

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

	ev.events=EPOLLIN  ; // Interested in readability
	ev.data.fd=timer_fd;
	//events[0].data.fd=timer_fd;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD,timer_fd,&ev)==-1) {
		printf("epoll_ctl(ADD) failed for timer_fd: errno=%d\n",errno);
		close(epoll_fd);
		close(timer_fd);
		exit(EXIT_FAILURE);
	}
	printf("added timerfd to epoll set\n");

	sleep(1);
	int ret=-1;
	memset(&ev,0,sizeof(ev));
	//unsigned char msg[] ={'A','\r','\n'};
	char buffer[128];
	if(timerfd_settime(timer_fd,0,&new_value,NULL)<0) {
                printf("timerfd_settime() failed: errno=%d\n",errno);
                close(timer_fd);
                exit(EXIT_FAILURE);
        }
	printf("set timerfd time=%s\n",itimerspec_dump(&new_value));

	dll_t dll;
	dllInit(&dll);

	while(1){
	//ADDITION 2
	/*len = PACKET_PAYLOAD_START_IDX;
	//ulink clsId + msgId
	int clsId = 4; //control class
	int msgId - 11; //joystick msg
	ulinkBuf[len++] = (clsId << 4) | msgId;*/

	//msg

	ret = epoll_wait(epoll_fd,&ev,1,13); //10ms is 13
	printf("%d\n",ret);

	if(ret < 0 ) {
		printf("epoll_wait() failed: error=%d\n",errno);
		close(epoll_fd);
		close(timer_fd);
		exit(EXIT_FAILURE);
	}
	printf("waited on epoll,ret=%d\n",ret);
        printf("%d ready events\n", ret);
	ret=-1;

	ret = read(timer_fd,&res,sizeof(res));
	if(ret!=sizeof(uint64_t)){
		exit(EXIT_FAILURE);
	}

	//ret = read(timer_fd,&res,sizeof(res));
	printf("read() returned %d, res=%" PRIu64 "\n", ret, res);
	//printf("read() returned %d,res=%d\n",ret,res);
	unsigned char * (bytes) = (unsigned char *)&res;
	//printf("res bytes: %02x %02x %02x %02x %02x%02x %02x %02x\n",
	//	bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7]);

	if(res==1){

		pthread_mutex_lock(&joystick_mutex);

		len=CreatePacket(&joysticks[0],ulinkBuf); //ulinkBuf pass by reference
		for(int i=0;i<len;i++) {
			printf("%02x ",ulinkBuf[i]);
		}
		printf("\n");
		printf("\n");
		printf("ulink buf len:%d\n",len);

		int tempLen = ((len/16)+(len%16?1:0))*16;

		ciphertext_len = aes_ctr_encrypt(key,gcm_iv,(unsigned char *)ulinkBuf,tempLen,ciphertext);
                //ciphertext_len = aes_gcm_encrypt(key,gcm_iv,(unsigned char *)ulinkBuf,tempLen,ciphertext,tag);

                printf("ciphertext len:%d\n",ciphertext_len);

		//size_t packet_size = ciphertext_len + sizeof(tag);
                //unsigned char packet[packet_size];
                //memcpy(packet,ciphertext,ciphertext_len);
                //memcpy(packet+ciphertext_len,tag,sizeof(tag));

		size_t packet_size = ciphertext_len;
		unsigned char packet[packet_size];
		memcpy(packet,ciphertext,ciphertext_len);
                printf("packet len %d\n",packet_size);

		dllPack(packet,packet_size,&dll);


		int write_status = write(serial_fd1,dll.dataBuffer,dll.dataCount);
		printf("Final size after dll %d\n",dll.dataCount);
		if(write_status>-1) {
		no_of_packets_sent++;
		}
		//for(int i=0;i<dll.dataCount;i++) {
		//	printf("%02x ",dll.dataBuffer[i]);
		//}
		printf("\n");
		printf("no of packets:%d\n",no_of_packets_sent);

 		//write(serial_fd1,dll.dataBuffer,dll.dataCount);
                for(int i=0;i<dll.dataCount;i++) {
                     printf("%02x ",dll.dataBuffer[i]);
                }
		printf("\n");


		//OLD CHANGES
		/*dllPack(ulinkBuf,len,&dll);
		for(int i=0;i<dll.dataCount;i++) {
			printf("%02x ",dll.dataBuffer[i]);
		}
		printf("\n");*/

		//---UART ADDITIONS ---
		//send_joystick_data(ulinkBuf,len);
		//write(serial_fd1,dll.dataBuffer,dll.dataCount);

		//ciphertext_len = aes_gcm_encrypt(key,gcm_iv,(unsigned char *)dll.dataBuffer,dll.dataCount,ciphertext,tag);

		/*size_t packet_size = ciphertext_len + sizeof(tag);
		unsigned char packet[packet_size];
		memcpy(packet,ciphertext,ciphertext_len);
		memcpy(packet+ciphertext_len,tag,sizeof(tag));

		printf("packet len %d\n",packet_size);*/

		//END OF OLD CHANGES

		/* Decryption */

		/*printf("\n-- AES GCM DECRYPTION --\n");
        	decrypted_len = aes_ctr_decrypt(key,(const unsigned char *)gcm_iv,ciphertext,ciphertext_len,decrypted_text);

        	if(decrypted_len < 0) {
                	fprintf(stderr,"Decryption failed (tag may be invalid)\n");
                	exit(EXIT_FAILURE);
        	}*/

        	/* Add a null terminator for printing */
        	//decrypted_text[decrypted_len] = '\0';
        	//printf("\nDecrypted Text: %s\n",decrypted_text);

        	/* Verification */

        	/*if(strncmp((char *)(unsigned char *)dll.dataBuffer,(char *)decrypted_text,decrypted_len) == 0) {
                	printf("\nSUCCESS: Decrypted text matches original plaintext.\n");
        	}
        	else {
                	printf("\nFAILURE: Decrypted text does not matchoriginal plaintext.\n");
                	exit(EXIT_FAILURE);
        	}*/


		//BIO_dump_fp(stdout,dll.dataBuffer,dll.dataCount);
		//BIO_dump_fp(stdout,ulinkBuf,len);
		//BIO_dump_fp(stdout,ciphertext,ciphertext_len);
		//BIO_dump_fp(stdout,tag,sizeof(tag));


		//write(serial_fd1,packet,packet_size);

		//write(serial_fd1,dll.dataBuffer,dll.dataCount);


		//--- END OF UART ADDITIONS ---

		//for(int i=0;i<6;i++) {

			//snprintf(buffer,sizeof(buffer),"%s %s %d value %d\r\n","JS0","AXIS",i,joysticks[0].axes[i]);
			//write(serial_fd1,buffer,strlen(buffer));
			//send_joystick_data(buffer);
		//}
		//for(int i=0;i<2;i++) {
			//snprintf(buffer,sizeof(buffer),"%s %s %d value %d\r\n","JS0","BUTTON",i,joysticks[0].buttons[i]);
			//write(serial_fd1,buffer,strlen(buffer));
			//send_joystick_data(buffer);
		//}
        	pthread_mutex_unlock(&joystick_mutex);
		res=0;
		ret=-1;
	}

	else{
		printf("RESETTING TIMER DUE TO ERRORS\n");
		 if(timerfd_settime(timer_fd,0,&new_value,NULL)<0) {
                printf("timerfd_settime() failed: errno=%d\n",errno);
                close(timer_fd);
                exit(EXIT_FAILURE);
        	}
        printf("set timerfd time=%s\n",itimerspec_dump(&new_value));
	}
	//pthread_mutex_unlock(&joystick_mutex);
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
       // ioctl (fd,JSIOCGAXES,&joysticks[i].number_of_axes);
       // ioctl (fd,JSIOCGBUTTONS,&joysticks[i].number_of_buttons);
        if(ioctl(fd,JSIOCGNAME(sizeof(joysticks[i].name)),joysticks[i].name)<0){
		strncpy(joysticks[i].name,"Unknown",sizeof(joysticks[i].name));
        }
	joysticks[i].number_of_buttons=2;
	joysticks[i].number_of_axes=4;

        joysticks[i].buttonsLen = 2 / 8 + (2 % 8 ?1:0); //1 bit per buttons calculates bytes are required to represent number_of_buttons
        joysticks[i].data=malloc(JOYSTICK_HEADER_LEN + joysticks[i].buttonsLen + joysticks[i].number_of_axes * sizeof(*joysticks[i].axes));

	joysticks[i].data[0] = joysticks[i].number_of_buttons;
	joysticks[i].data[1] = joysticks[i].number_of_axes;

	//set pointers to buttons and axes within data
	joysticks[i].buttons = joysticks[i].data + JOYSTICK_HEADER_LEN;
	joysticks[i].axes = (int16_t*) (joysticks[i].data + JOYSTICK_HEADER_LEN + joysticks[i].buttonsLen);

        printf("Name %s\n",joysticks[i].name);
        printf("%d\n",joystick_fds[i]);
    }

    if(num_joysticks == 0) {
        printf("No joysticks found\n");
        //return 1;
    }
    printf("%d\n",num_joysticks);
    crc_init();
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
    //uart debugging
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

   for(int i=0;i<MAX_JOYSTICKS;i++) {
		free(joysticks[0].data);
	}

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
