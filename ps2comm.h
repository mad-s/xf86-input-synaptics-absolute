#ifndef _PS2COMM_H_
#define _PS2COMM_H_

#include <sys/ioctl.h>
#include "xf86_OSproc.h"

/* acknowledge for commands and parameter */
#define PS2_ACK 			0xFA
#define PS2_ERROR			0xFC

/* standard PS/2 commands */
#define PS2_CMD_RESET			0xFF
#define PS2_CMD_RESEND			0xFE
#define PS2_CMD_SET_DEFAULT		0xF6
#define PS2_CMD_DISABLE			0xF5
#define PS2_CMD_ENABLE			0xF4
#define PS2_CMD_SET_SAMPLE_RATE		0xF3
#define PS2_CMD_READ_DEVICE_TYPE	0xF2
#define PS2_CMD_SET_REMOTE_MODE		0xF0
#define PS2_CMD_SET_WRAP_MODE		0xEE
#define PS2_CMD_RESET_WRAP_MODE		0xEC
#define PS2_CMD_READ_DATA		0xEB
#define PS2_CMD_SET_STREAM_MODE		0xEA
#define PS2_CMD_STATUS_REQUEST		0xE9
#define PS2_CMD_SET_RESOLUTION		0xE8
#define PS2_CMD_SET_SCALING_2_1		0xE7
#define PS2_CMD_SET_SCALING_1_1		0xE6

/* synatics modes */
#define SYN_BIT_ABSOLUTE_MODE		(1 << 7)
#define SYN_BIT_HIGH_RATE		(1 << 6)
#define SYN_BIT_SLEEP_MODE		(1 << 3)
#define SYN_BIT_DISABLE_GESTURE		(1 << 2)
#define SYN_BIT_W_MODE			(1 << 0)

/* synaptics model ID bits */
#define SYN_MODEL_ROT180(synhw) 	((synhw).model_id & (1 << 23))
#define SYN_MODEL_PORTRAIT(synhw)	((synhw).model_id & (1 << 22))
#define SYN_MODEL_SENSOR(synhw)		(((synhw).model_id >> 16) & 0x3f)
#define SYN_MODEL_HARDWARE(synhw)	(((synhw).model_id >> 9) & 0x7f)
#define SYN_MODEL_NEWABS(synhw)		((synhw).model_id & (1 << 7))
#define SYN_MODEL_PEN(synhw)		((synhw).model_id & (1 << 6))
#define SYN_MODEL_SIMPLIC(synhw)	((synhw).model_id & (1 << 5))
#define SYN_MODEL_GEOMETRY(synhw)	((synhw).model_id & 0x0f)

/* synaptics capability bits */
#define SYN_CAP_EXTENDED(synhw)		((synhw).capabilities & (1 << 23))
#define SYN_CAP_MIDDLE_BUTTON(synhw)	((synhw).capabilities & (1 << 18))
#define SYN_CAP_PASSTHROUGH(synhw)	((synhw).capabilities & (1 << 7))
#define SYN_CAP_SLEEP(synhw)		((synhw).capabilities & (1 << 4))
#define SYN_CAP_FOUR_BUTTON(synhw)	((synhw).capabilities & (1 << 3))
#define SYN_CAP_MULTIFINGER(synhw)	((synhw).capabilities & (1 << 1))
#define SYN_CAP_PALMDETECT(synhw)	((synhw).capabilities & (1 << 0))
#define SYN_CAP_VALID(synhw)		((((synhw).capabilities & 0x00ff00) >> 8) == 0x47)
#define SYN_EXT_CAP_REQUESTS(synhw)	(((synhw).capabilities & 0x700000) == 0x100000)
#define SYN_CAP_MULTI_BUTTON_NO(synhw)	(((synhw).ext_cap & 0x00f000) >> 12)

/* synaptics modes query bits */
#define SYN_MODE_ABSOLUTE(m)		((m) & (1 << 7))
#define SYN_MODE_RATE(m)		((m) & (1 << 6))
#define SYN_MODE_BAUD_SLEEP(m)		((m) & (1 << 3))
#define SYN_MODE_DISABLE_GESTURE(m)	((m) & (1 << 2))
#define SYN_MODE_PACKSIZE(m)		((m) & (1 << 1))
#define SYN_MODE_WMODE(m)		((m) & (1 << 0))
#define SYN_MODE_VALID(m)		(((m) & 0xffff00) == 0x3B47)

/* synaptics identify query bits */
#define SYN_ID_MODEL(synhw)		(((synhw).identity >> 4) & 0x0f)
#define SYN_ID_MAJOR(synhw)		((synhw).identity & 0x0f)
#define SYN_ID_MINOR(synhw)		(((synhw).identity >> 16) & 0xff)
#define SYN_ID_IS_SYNAPTICS(synhw)	((((synhw).identity >> 8) & 0xff) == 0x47)

typedef unsigned char byte;


Bool ps2_putbyte(int fd, byte b);

struct SynapticsHwInfo;
void ps2_print_ident(const struct SynapticsHwInfo *synhw);

#endif /* _PS2COMM_H_ */
