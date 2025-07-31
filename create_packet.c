#include "create_packet.h"

int CreatePacket(JoystickState *js0,uint8_t ulinkBuf[MAX_ULINK_PACKET_SIZE]) {

	if (!js0) return -1; //Safety check
	int jsDataLen = JOYSTICK_HEADER_LEN + js0->buttonsLen + js0->number_of_axes * sizeof(js0->axes[0]);
	//create ulink packet
	//
	//structure of the joystick message (before it's ulink packetization)
	//
	//	1byte: number_of_buttons
	//	1byte: number_of_axes;
	//	N bytes: values of button and axes as follows:
	//		buttons:
	//			minimum number of bytes necessary to
	//			hold number_of_buttons bits that each hold
	//			an on/off button values
	//		Axes:
	//			number_of_axes values
	//			first 4 values for throttle,yaw,roll & pitch

	int len = PACKET_PAYLOAD_START_IDX;

	//ulink classId + msgId
	int classId = 4; //stick class
	int msgId = 11; //joystick msg;
	ulinkBuf[len++] = (classId <<4) | msgId; //00

	//ulink msg len
	ulinkBuf[len++] = (uint8_t) jsDataLen;

	//msg
	memcpy(ulinkBuf + len,js0->data,jsDataLen);
	len+=jsDataLen;

	//crc
	char crcHo,crcLo;
	crc_calculate((char *) ulinkBuf + PACKET_PAYLOAD_START_IDX,len - PACKET_PAYLOAD_START_IDX,&crcHo,&crcLo);
	ulinkBuf[len++] = crcHo;
	ulinkBuf[len++] = crcLo;

	//ulink packet len
	int packetLen = len - PACKET_PAYLOAD_START_IDX;
	ulinkBuf[0] = (packetLen >> 8) & 0xff;
	ulinkBuf[1] = packetLen & 0xff;

	return len;

}
