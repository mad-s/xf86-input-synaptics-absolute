/* Copyright (C) 2001 Stefan Gmeiner <riddlebox@freesurf.ch>
 *
 *   Copyright (c) 1997 C. Scott Ananian <cananian@alumni.priceton.edu>
 *   Copyright (c) 1998-2000 Bruce Kalk <kall@compass.com>
 *     code f�r the special synaptics commands (from the tpconfig-source)
 *
 *   Synaptics Passthrough Support
 *   Copyright (c) 2002 Linuxcare Inc. David Kennedy <dkennedy@linuxcare.com>
 *   adapted to version 0.12.1
 *   Copyright (c) 2003 Fred Hucht <fred@thp.Uni-Duisburg.de>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "ps2comm.h"
#include "synproto.h"
#include <xisb.h>
#define SYNAPTICS_PRIVATE
#include "synaptics.h"
#include <xf86.h>

#define MAX_UNSYNC_PACKETS 10				/* i.e. 10 to 60 bytes */

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

/* synaptics queries */
#define SYN_QUE_IDENTIFY		0x00
#define SYN_QUE_MODES			0x01
#define SYN_QUE_CAPABILITIES		0x02
#define SYN_QUE_MODEL			0x03
#define SYN_QUE_SERIAL_NUMBER_PREFIX	0x06
#define SYN_QUE_SERIAL_NUMBER_SUFFIX	0x07
#define SYN_QUE_RESOLUTION		0x08
#define SYN_QUE_EXT_CAPAB		0x09

/* status request response bits (PS2_CMD_STATUS_REQUEST) */
#define PS2_RES_REMOTE(r)	((r) & (1 << 22))
#define PS2_RES_ENABLE(r)	((r) & (1 << 21))
#define PS2_RES_SCALING(r)	((r) & (1 << 20))
#define PS2_RES_LEFT(r)		((r) & (1 << 18))
#define PS2_RES_MIDDLE(r)	((r) & (1 << 17))
#define PS2_RES_RIGHT(r)	((r) & (1 << 16))
#define PS2_RES_RESOLUTION(r)	(((r) >> 8) & 0x03)
#define PS2_RES_SAMPLE_RATE(r)	((r) & 0xff)

/* #define DEBUG */

#ifdef DEBUG
#define PS2DBG(x) (x)
#else
#define PS2DBG(x)
#endif

/*****************************************************************************
 *	PS/2 Utility functions.
 *     Many parts adapted from tpconfig.c by C. Scott Ananian
 ****************************************************************************/

/*
 * Read a byte from the ps/2 port
 */
static Bool
ps2_getbyte(int fd, byte *b)
{
    if (xf86WaitForInput(fd, 50000) > 0) {
	if (xf86ReadSerial(fd, b, 1) != 1) {
	    PS2DBG(ErrorF("ps2_getbyte: No byte read\n"));
	    return !Success;
	}
	PS2DBG(ErrorF("ps2_getbyte: byte %02X read\n", *b));
	return Success;
    }
    PS2DBG(ErrorF("ps2_getbyte: timeout xf86WaitForInput\n"));
    return !Success;
}

/*
 * Write a byte to the ps/2 port, wait for ACK
 */
static Bool
ps2_putbyte(int fd, byte b)
{
    byte ack;

    if (xf86WriteSerial(fd, &b, 1) != 1) {
	PS2DBG(ErrorF("ps2_putbyte: error xf86WriteSerial\n"));
	return !Success;
    }
    PS2DBG(ErrorF("ps2_putbyte: byte %02X send\n", b));
    /* wait for an ACK */
    if (ps2_getbyte(fd, &ack) != Success) {
	return !Success;
    }
    if (ack != PS2_ACK) {
	PS2DBG(ErrorF("ps2_putbyte: wrong acknowledge 0x%02x\n", ack));
	return !Success;
    }
    return Success;
}

/*
 * Use the Synaptics extended ps/2 syntax to write a special command byte. Needed by
 * ps2_send_cmd and ps2_set_mode.
 * special command: 0xE8 rr 0xE8 ss 0xE8 tt 0xE8 uu where (rr*64)+(ss*16)+(tt*4)+uu
 *                  is the command. A 0xF3 or 0xE9 must follow (see ps2_send_cmd, ps2_set_mode)
 */
static Bool
ps2_special_cmd(int fd, byte cmd)
{
    int i;

    /* initialize with 'inert' command */
    if (ps2_putbyte(fd, PS2_CMD_SET_SCALING_1_1) == Success)
	/* send 4x 2-bits with set resolution command */
	for (i = 0; i < 4; i++) {
	    if (((ps2_putbyte(fd, PS2_CMD_SET_RESOLUTION)) != Success) ||
		((ps2_putbyte(fd, (cmd >> 6) & 0x3) != Success)))
		return !Success;
	    cmd <<= 2;
	}
    else
	return !Success;
    return Success;
}

/*
 * Send a command to the synpatics touchpad by special commands
 */
static Bool
ps2_send_cmd(int fd, byte c)
{
    PS2DBG(ErrorF("send command: 0x%02X\n", c));
    return ps2_special_cmd(fd, c) || ps2_putbyte(fd, PS2_CMD_STATUS_REQUEST);
}

/*****************************************************************************
 *	Synaptics passthrough functions
 ****************************************************************************/

static Bool
ps2_getbyte_passthrough(int fd, byte *response)
{
    byte ack;
    int timeout_count;
#define MAX_RETRY_COUNT 30

    /* Getting a response back through the passthrough could take some time.
     * Spin a little for the first byte */
    for (timeout_count = 0;
	 (ps2_getbyte(fd, &ack) != Success) && timeout_count <= MAX_RETRY_COUNT;
	 timeout_count++)
	;
    /* Do some sanity checking */
    if ((ack & 0xfc) != 0x84) {
	PS2DBG(ErrorF("ps2_getbyte_passthrough: expected 0x84 and got: %02x\n",
		      ack & 0xfc));
	return !Success;
    }

    ps2_getbyte(fd, response);
    ps2_getbyte(fd, &ack);
    ps2_getbyte(fd, &ack);
    if ((ack & 0xcc) != 0xc4) {
	PS2DBG(ErrorF("ps2_getbyte_passthrough: expected 0xc4 and got: %02x\n",
		      ack & 0xcc));
	return !Success;
    }
    ps2_getbyte(fd, &ack);
    ps2_getbyte(fd, &ack);

    return Success;
}

static Bool
ps2_putbyte_passthrough(int fd, byte c)
{
    byte ack;

    ps2_special_cmd(fd, c);
    ps2_putbyte(fd, 0xF3);
    ps2_putbyte(fd, 0x28);

    ps2_getbyte_passthrough(fd, &ack);
    if (ack != PS2_ACK) {
	PS2DBG(ErrorF("ps2_putbyte_passthrough: wrong acknowledge 0x%02x\n", ack));
	return !Success;
    }
    return Success;
}

/*****************************************************************************
 *	Synaptics communications functions
 ****************************************************************************/

/*
 * Set the synaptics touchpad mode byte by special commands
 */
static Bool
synaptics_set_mode(int fd, byte mode)
{
    PS2DBG(ErrorF("set mode byte to: 0x%02X\n", mode));
    return (ps2_special_cmd(fd, mode) ||
	    ps2_putbyte(fd, PS2_CMD_SET_SAMPLE_RATE) ||
	    ps2_putbyte(fd, 0x14));
}

/*
 * reset the touchpad
 */
Bool
synaptics_reset(int fd)
{
    byte r[2];

    xf86FlushInput(fd);
    PS2DBG(ErrorF("Reset the Touchpad...\n"));
    if (ps2_putbyte(fd, PS2_CMD_RESET) != Success) {
	PS2DBG(ErrorF("...failed\n"));
	return !Success;
    }
    xf86WaitForInput(fd, 4000000);
    if ((ps2_getbyte(fd, &r[0]) == Success) &&
	(ps2_getbyte(fd, &r[1]) == Success)) {
	if (r[0] == 0xAA && r[1] == 0x00) {
	    PS2DBG(ErrorF("...done\n"));
	    return Success;
	} else {
	    PS2DBG(ErrorF("...failed. Wrong reset ack 0x%02x, 0x%02x\n", r[0], r[1]));
	    return !Success;
	}
    }
    PS2DBG(ErrorF("...failed\n"));
    return !Success;
}

static Bool
SynapticsResetPassthrough(int fd)
{
    byte ack;

    /* send reset */
    ps2_putbyte_passthrough(fd, 0xff);
    ps2_getbyte_passthrough(fd, &ack);
    if (ack != 0xaa) {
	PS2DBG(ErrorF("SynapticsResetPassthrough: ack was %02x not 0xaa\n", ack));
	return !Success;
    }
    ps2_getbyte_passthrough(fd, &ack);
    if (ack != 0x00) {
	PS2DBG(ErrorF("SynapticsResetPassthrough: ack was %02x not 0x00\n", ack));
	return !Success;
    }

    /* set defaults, turn on streaming, and enable the mouse */
    return (ps2_putbyte_passthrough(fd, 0xf6) ||
	    ps2_putbyte_passthrough(fd, 0xea) ||
	    ps2_putbyte_passthrough(fd, 0xf4));
}

/*
 * Read the model-id bytes from the touchpad
 * see also SYN_MODEL_* macros
 */
static Bool
synaptics_model_id(int fd, struct synapticshw *synhw)
{
    byte mi[3];

    PS2DBG(ErrorF("Read mode id...\n"));

    if ((ps2_send_cmd(fd, SYN_QUE_MODEL) == Success) &&
	(ps2_getbyte(fd, &mi[0]) == Success) &&
	(ps2_getbyte(fd, &mi[1]) == Success) &&
	(ps2_getbyte(fd, &mi[2]) == Success)) {
	synhw->model_id = (mi[0] << 16) | (mi[1] << 8) | mi[2];
	PS2DBG(ErrorF("mode-id %06X\n", synhw->model_id));
	PS2DBG(ErrorF("...done.\n"));
	return Success;
    }
    PS2DBG(ErrorF("...failed.\n"));
    return !Success;
}

/*
 * Read the capability-bits from the touchpad
 * see also the SYN_CAP_* macros
 */
static Bool
synaptics_capability(int fd, struct synapticshw *synhw)
{
    byte cap[3];

    PS2DBG(ErrorF("Read capabilites...\n"));

    synhw->ext_cap = 0;
    if ((ps2_send_cmd(fd, SYN_QUE_CAPABILITIES) == Success) &&
	(ps2_getbyte(fd, &cap[0]) == Success) &&
	(ps2_getbyte(fd, &cap[1]) == Success) &&
	(ps2_getbyte(fd, &cap[2]) == Success)) {
	synhw->capabilities = (cap[0] << 16) | (cap[1] << 8) | cap[2];
	PS2DBG(ErrorF("capabilities %06X\n", synhw->capabilities));
	if (SYN_CAP_VALID(*synhw)) {
	    if (SYN_EXT_CAP_REQUESTS(*synhw)) {
		if ((ps2_send_cmd(fd, SYN_QUE_EXT_CAPAB) == Success) &&
		    (ps2_getbyte(fd, &cap[0]) == Success) &&
		    (ps2_getbyte(fd, &cap[1]) == Success) &&
		    (ps2_getbyte(fd, &cap[2]) == Success)) {
		    synhw->ext_cap = (cap[0] << 16) | (cap[1] << 8) | cap[2];
		    PS2DBG(ErrorF("ext-capability %06X\n", synhw->ext_cap));
		} else {
		    PS2DBG(ErrorF("synaptics says, that it has extended-capabilities, "
				  "but I cannot read them."));
		}
	    }
	    PS2DBG(ErrorF("...done.\n"));
	    return Success;
	}
    }
    PS2DBG(ErrorF("...failed.\n"));
    return !Success;
}

/*
 * Identify Touchpad
 * See also the SYN_ID_* macros
 */
static Bool
synaptics_identify(int fd, struct synapticshw *synhw)
{
    byte id[3];

    PS2DBG(ErrorF("Identify Touchpad...\n"));

    synhw->identity = 0;
    if ((ps2_send_cmd(fd, SYN_QUE_IDENTIFY) == Success) &&
	(ps2_getbyte(fd, &id[0]) == Success) &&
	(ps2_getbyte(fd, &id[1]) == Success) &&
	(ps2_getbyte(fd, &id[2]) == Success)) {
	synhw->identity = (id[0] << 16) | (id[1] << 8) | id[2];
	PS2DBG(ErrorF("ident %06X\n", synhw->identity));
	if (SYN_ID_IS_SYNAPTICS(*synhw)) {
	    PS2DBG(ErrorF("...done.\n"));
	    return Success;
	}
    }
    PS2DBG(ErrorF("...failed.\n"));
    return !Success;
}

static Bool
synaptics_get_hwinfo(int fd, struct synapticshw *synhw)
{
    if (synaptics_identify(fd, synhw) != Success)
	return !Success;

    if (synaptics_model_id(fd, synhw) != Success)
	return !Success;

    if (synaptics_capability(fd, synhw) != Success)
	return !Success;

    return Success;
}

Bool
SynapticsEnableDevice(int fd)
{
    return ps2_putbyte(fd, PS2_CMD_ENABLE);
}

static Bool
SynapticsDisableDevice(int fd)
{
    xf86FlushInput(fd);
    return ps2_putbyte(fd, PS2_CMD_DISABLE);
}

static Bool
QueryIsSynaptics(int fd)
{
    struct synapticshw synhw;
    int i;

    for (i = 0; i < 3; i++) {
	if (SynapticsDisableDevice(fd) == Success)
	    break;
    }

    xf86WaitForInput(fd, 20000);
    xf86FlushInput(fd);
    if (synaptics_identify(fd, &synhw) == Success) {
	return TRUE;
    } else {
	ErrorF("Query no Synaptics: %06X\n", synhw.identity);
	return FALSE;
    }
}

static void
PrintIdent(const synapticshw_t *synhw)
{
    xf86Msg(X_PROBED, " Synaptics Touchpad, model: %d\n", SYN_ID_MODEL(*synhw));
    xf86Msg(X_PROBED, " Firmware: %d.%d\n", SYN_ID_MAJOR(*synhw),
	    SYN_ID_MINOR(*synhw));

    if (SYN_MODEL_ROT180(*synhw))
	xf86Msg(X_PROBED, " 180 degree mounted touchpad\n");
    if (SYN_MODEL_PORTRAIT(*synhw))
	xf86Msg(X_PROBED, " portrait touchpad\n");
    xf86Msg(X_PROBED, " Sensor: %d\n", SYN_MODEL_SENSOR(*synhw));
    if (SYN_MODEL_NEWABS(*synhw))
	xf86Msg(X_PROBED, " new absolute packet format\n");
    if (SYN_MODEL_PEN(*synhw))
	xf86Msg(X_PROBED, " pen detection\n");

    if (SYN_CAP_EXTENDED(*synhw)) {
	xf86Msg(X_PROBED, " Touchpad has extended capability bits\n");
	if (SYN_CAP_MULTI_BUTTON_NO(*synhw))
	    xf86Msg(X_PROBED, " -> %d multi buttons, i.e. besides standard buttons\n",
		    (int)(SYN_CAP_MULTI_BUTTON_NO(*synhw)));
	else if (SYN_CAP_FOUR_BUTTON(*synhw))
	    xf86Msg(X_PROBED, " -> four buttons\n");
	if (SYN_CAP_MULTIFINGER(*synhw))
	    xf86Msg(X_PROBED, " -> multifinger detection\n");
	if (SYN_CAP_PALMDETECT(*synhw))
	    xf86Msg(X_PROBED, " -> palm detection\n");
	if (SYN_CAP_PASSTHROUGH(*synhw))
	    xf86Msg(X_PROBED, " -> pass-through port\n");
    }
}


static void
PS2DeviceOnHook(LocalDevicePtr local)
{
}

static void
PS2DeviceOffHook(LocalDevicePtr local)
{
    synaptics_set_mode(local->fd, 0);
}

static Bool
PS2QueryHardware(LocalDevicePtr local, synapticshw_t *synhw, Bool *hasGuest)
{
    int mode;

    /* is the synaptics touchpad active? */
    if (!QueryIsSynaptics(local->fd))
	return FALSE;

    xf86Msg(X_PROBED, "%s synaptics touchpad found\n", local->name);

    if (synaptics_reset(local->fd) != Success)
	xf86Msg(X_ERROR, "%s reset failed\n", local->name);

    if (synaptics_get_hwinfo(local->fd, synhw) != Success)
	return FALSE;

    mode = SYN_BIT_ABSOLUTE_MODE | SYN_BIT_HIGH_RATE;
    if (SYN_ID_MAJOR(*synhw) >= 4)
	mode |= SYN_BIT_DISABLE_GESTURE;
    if (SYN_CAP_EXTENDED(*synhw))
	mode |= SYN_BIT_W_MODE;
    if (synaptics_set_mode(local->fd, mode) != Success)
	return FALSE;

    /* Check to see if the host mouse supports a guest */
    if (SYN_CAP_PASSTHROUGH(*synhw)) {
        *hasGuest = TRUE;

	/* Enable the guest mouse.  Set it to relative mode, three byte
	 * packets */

	/* Disable the host to talk to the guest */
	SynapticsDisableDevice(local->fd);
	/* Reset it, set defaults, streaming and enable it */
	if ((SynapticsResetPassthrough(local->fd)) != Success) {
	    *hasGuest = FALSE;
	}
    }

    SynapticsEnableDevice(local->fd);

    PrintIdent(synhw);

    return TRUE;
}

/*
 * Decide if the current packet stored in priv->protoBuf is valid.
 */
static Bool
PacketOk(SynapticsPrivate *priv)
{
    unsigned char *buf = priv->protoBuf;
    int newabs = SYN_MODEL_NEWABS(priv->synhw);

    if (newabs ? ((buf[0] & 0xC0) != 0x80) : ((buf[0] & 0xC0) != 0xC0)) {
	DBG(4, ErrorF("Synaptics driver lost sync at 1st byte\n"));
	return FALSE;
    }

    if (!newabs && ((buf[1] & 0x60) != 0x00)) {
	DBG(4, ErrorF("Synaptics driver lost sync at 2nd byte\n"));
	return FALSE;
    }

    if ((newabs ? ((buf[3] & 0xC0) != 0xC0) : ((buf[3] & 0xC0) != 0x80))) {
	DBG(4, ErrorF("Synaptics driver lost sync at 4th byte\n"));
	return FALSE;
    }

    if (!newabs && ((buf[4] & 0x60) != 0x00)) {
	DBG(4, ErrorF("Synaptics driver lost sync at 5th byte\n"));
	return FALSE;
    }

    return TRUE;
}

static Bool
SynapticsGetPacket(LocalDevicePtr local, SynapticsPrivate *priv)
{
    int count = 0;
    int c;
    unsigned char u;

    while ((c = XisbRead(priv->buffer)) >= 0) {
	u = (unsigned char)c;

	/* test if there is a reset sequence received */
	if ((c == 0x00) && (priv->lastByte == 0xAA)) {
	    if (xf86WaitForInput(local->fd, 50000) == 0) {
		DBG(7, ErrorF("Reset received\n"));
		QueryHardware(local);
	    } else
		DBG(3, ErrorF("faked reset received\n"));
	}
	priv->lastByte = u;

	/* when there is no synaptics touchpad pipe the data to the repeater fifo */
	if (!priv->isSynaptics) {
	    xf86write(priv->fifofd, &u, 1);
	    if (++count >= 3)
		return FALSE;
	    continue;
	}

	/* to avoid endless loops */
	if (count++ > 30) {
	    ErrorF("Synaptics driver lost sync... got gigantic packet!\n");
	    return FALSE;
	}

	priv->protoBuf[priv->protoBufTail++] = u;

	/* Check that we have a valid packet. If not, we are out of sync,
	   so we throw away the first byte in the packet.*/
	if (priv->protoBufTail >= 6) {
	    if (!PacketOk(priv)) {
		int i;
		for (i = 0; i < priv->protoBufTail - 1; i++)
		    priv->protoBuf[i] = priv->protoBuf[i + 1];
		priv->protoBufTail--;
		priv->outOfSync++;
		if (priv->outOfSync > MAX_UNSYNC_PACKETS) {
		    priv->outOfSync = 0;
		    DBG(3, ErrorF("Synaptics synchronization lost too long -> reset touchpad.\n"));
		    QueryHardware(local); /* including a reset */
		    continue;
		}
	    }
	}

	if (priv->protoBufTail >= 6) { /* Full packet received */
	    if (priv->outOfSync > 0) {
		priv->outOfSync = 0;
		DBG(4, ErrorF("Synaptics driver resynced.\n"));
	    }
	    priv->protoBufTail = 0;
	    return TRUE;
	}
    }

    return FALSE;
}

static Bool
PS2ReadHwState(LocalDevicePtr local, SynapticsPrivate *priv,
	       struct SynapticsHwState *hwRet)
{
    int newabs = SYN_MODEL_NEWABS(priv->synhw);
    unsigned char *buf = priv->protoBuf;
    struct SynapticsHwState *hw = &(priv->hwState);
    int w, i;

    if (!SynapticsGetPacket(local, priv))
	return FALSE;

    /* Handle guest packets */
    hw->guest_dx = hw->guest_dy = 0;
    if (newabs && priv->hasGuest) {
	w = (((buf[0] & 0x30) >> 2) |
	     ((buf[0] & 0x04) >> 1) |
	     ((buf[3] & 0x04) >> 2));
	if (w == 3) {	       /* If w is 3, this is a guest packet */
	    if (buf[4] != 0)
		hw->guest_dx =   buf[4] - ((buf[1] & 0x10) ? 256 : 0);
	    if (buf[5] != 0)
		hw->guest_dy = -(buf[5] - ((buf[1] & 0x20) ? 256 : 0));
	    hw->guest_left  = (buf[1] & 0x01) ? TRUE : FALSE;
	    hw->guest_mid   = (buf[1] & 0x04) ? TRUE : FALSE;
	    hw->guest_right = (buf[1] & 0x02) ? TRUE : FALSE;
	    *hwRet = *hw;
	    return TRUE;
	}
    }

    /* Handle normal packets */
    hw->x = hw->y = hw->z = hw->numFingers = hw->fingerWidth = 0;
    hw->left = hw->right = hw->up = hw->down = hw->middle = FALSE;
    for (i = 0; i < 8; i++)
	hw->multi[i] = FALSE;

    if (newabs) {			    /* newer protos...*/
	DBG(7, ErrorF("using new protocols\n"));
	hw->x = (((buf[3] & 0x10) << 8) |
		 ((buf[1] & 0x0f) << 8) |
		 buf[4]);
	hw->y = (((buf[3] & 0x20) << 7) |
		 ((buf[1] & 0xf0) << 4) |
		 buf[5]);

	hw->z = buf[2];
	w = (((buf[0] & 0x30) >> 2) |
	     ((buf[0] & 0x04) >> 1) |
	     ((buf[3] & 0x04) >> 2));

	hw->left  = (buf[0] & 0x01) ? 1 : 0;
	hw->right = (buf[0] & 0x02) ? 1 : 0;

	if (SYN_CAP_EXTENDED(priv->synhw)) {
	    if (SYN_CAP_FOUR_BUTTON(priv->synhw)) {
		hw->up = ((buf[3] & 0x01)) ? 1 : 0;
		if (hw->left)
		    hw->up = !hw->up;
		hw->down = ((buf[3] & 0x02)) ? 1 : 0;
		if (hw->right)
		    hw->down = !hw->down;
	    }
	    if (SYN_CAP_MULTI_BUTTON_NO(priv->synhw)) {
		if ((buf[3] & 2) ? !hw->right : hw->right) {
		    switch (SYN_CAP_MULTI_BUTTON_NO(priv->synhw) & ~0x01) {
		    default:
			break;
		    case 8:
			hw->multi[7] = ((buf[5] & 0x08)) ? 1 : 0;
			hw->multi[6] = ((buf[4] & 0x08)) ? 1 : 0;
		    case 6:
			hw->multi[5] = ((buf[5] & 0x04)) ? 1 : 0;
			hw->multi[4] = ((buf[4] & 0x04)) ? 1 : 0;
		    case 4:
			hw->multi[3] = ((buf[5] & 0x02)) ? 1 : 0;
			hw->multi[2] = ((buf[4] & 0x02)) ? 1 : 0;
		    case 2:
			hw->multi[1] = ((buf[5] & 0x01)) ? 1 : 0;
			hw->multi[0] = ((buf[4] & 0x01)) ? 1 : 0;
		    }
		}
	    }
	}
    } else {			    /* old proto...*/
	DBG(7, ErrorF("using old protocol\n"));
	hw->x = (((buf[1] & 0x1F) << 8) |
		 buf[2]);
	hw->y = (((buf[4] & 0x1F) << 8) |
		 buf[5]);

	hw->z = (((buf[0] & 0x30) << 2) |
		 (buf[3] & 0x3F));
	w = (((buf[1] & 0x80) >> 4) |
	     ((buf[0] & 0x04) >> 1));

	hw->left  = (buf[0] & 0x01) ? 1 : 0;
	hw->right = (buf[0] & 0x02) ? 1 : 0;
    }

    hw->y = YMAX_NOMINAL + YMIN_NOMINAL - hw->y;

    if (hw->z > 0) {
	int w_ok = 0;
	/*
	 * Use capability bits to decide if the w value is valid.
	 * If not, set it to 5, which corresponds to a finger of
	 * normal width.
	 */
	if (SYN_CAP_EXTENDED(priv->synhw)) {
	    if ((w >= 0) && (w <= 1)) {
		w_ok = SYN_CAP_MULTIFINGER(priv->synhw);
	    } else if (w == 2) {
		w_ok = SYN_MODEL_PEN(priv->synhw);
	    } else if ((w >= 4) && (w <= 15)) {
		w_ok = SYN_CAP_PALMDETECT(priv->synhw);
	    }
	}
	if (!w_ok)
	    w = 5;

	switch (w) {
	case 0:
	    hw->numFingers = 2;
	    hw->fingerWidth = 5;
	    break;
	case 1:
	    hw->numFingers = 3;
	    hw->fingerWidth = 5;
	    break;
	default:
	    hw->numFingers = 1;
	    hw->fingerWidth = w;
	    break;
	}
    }

    *hwRet = *hw;
    return TRUE;
}

struct SynapticsProtocolOperations psaux_proto_operations = {
    PS2DeviceOnHook,
    PS2DeviceOffHook,
    PS2QueryHardware,
    PS2ReadHwState
};
