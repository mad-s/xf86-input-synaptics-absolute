#ifndef	_SAMPLE_H_
#define _SAMPLE_H_

/******************************************************************************
 *		Definitions
 *									structs, typedefs, #defines, enums
 *****************************************************************************/
#define SYNAPTICS_MOVE_HISTORY	5
#define SHM_SYNAPTICS 23947

typedef struct _SynapticsTapRec {
	int x, y;
	unsigned int packet;
} SynapticsTapRec;

typedef struct _SynapticsMoveHist {
	int x, y;
} SynapticsMoveHistRec;

typedef struct _SynapticsSHM {
	int x, y;							/* actual x, y Coordinates */
	int z;								/* pressure value */
	int w;								/* finger width value */
	int left, right, up, down;			/* left/right/up/down buttons */

	unsigned long int model_id;			/* Model-ID */
	unsigned long int capabilities; 	/* Capabilities */
	unsigned long int identity;			/* Identification */
	Bool isSynaptics;					/* Synaptics touchpad aktiv */

	/* Parameter data */
	int	left_edge, right_edge, top_edge, bottom_edge;
										/* edge coordinates absolute */
	int	finger_low, finger_high;		/* finger detection values in Z-values */
	int	tap_time, tap_move;				/* max. tapping-time and movement in packets and coord. */
	int	scroll_dist_vert;				/* Scrolling distance in absolute coordinates */
	double min_speed, max_speed, accl;  /* movement parameters */
	char* repeater;						/* Repeater on or off */
} SynapticsSHM, *SynapticsSHMPtr;

typedef struct _SynapticsPrivateRec
{
	/* shared memory pointer */
	SynapticsSHMPtr synpara;	

	/* Data read from the touchpad */
	unsigned long int model_id;			/* Model-ID */
	unsigned long int capabilities; 	/* Capabilities */
	unsigned long int identity;			/* Identification */
	Bool isSynaptics;					/* Synaptics touchpad aktiv */

	/* Data for normal processing */
	XISBuffer *buffer;
	unsigned char protoBuf[6];			/* Buffer for Packet */
	unsigned char lastByte;				/* letztes gelesene byte */
	Bool inSync;						/* Packets in sync */
	int protoBufTail;
	int fifofd;		 					/* fd for fifo */
	SynapticsTapRec touch_on;			/* data when the touchpad is touched */
	SynapticsMoveHistRec move_hist[SYNAPTICS_MOVE_HISTORY];
										/* movement history */
	int scroll_y;						/* last y-scroll position */
	unsigned int count_packet_finger;	/* packet counter with finger on the touchpad */
	unsigned int count_packet;			/* packet counter */
	unsigned int count_packet_tapping;	/* packet counter for tapping */
	unsigned int count_button_delay;	/* button delay for 3rd button emulation */
	Bool finger_flag;					/* previous finger */
	Bool tap, drag, doubletap;			/* feature flags */
	Bool tap_left, tap_mid, tap_right;	/* tapping buttons */
	Bool vert_scroll_on;				/* scrolling flag */
	double frac_x, frac_y;				/* absoulte -> relative fraction */
	Bool third_button;					/* emulated 3rd button */
	OsTimerPtr repeat_timer;			/* for up/down-button repeat */
	int repeatButtons;					/* buttons for repeat */
	int lastButtons;					/* last State of the buttons */
	int finger_count;					/* tap counter for fingers */
	int palm;							/* Set to true when palm detected, reset to false when
										 * palm/finger contact disappears */
	int prev_z;							/* previous z value, for palm detection */
	int avg_w;							/* weighted average of previous w values */
}
SynapticsPrivateRec, *SynapticsPrivatePtr;


static Bool DeviceControl(DeviceIntPtr, int);
static void ReadInput(LocalDevicePtr);
static int ControlProc(LocalDevicePtr, xDeviceCtl*);
static void CloseProc(LocalDevicePtr);
static int SwitchMode(ClientPtr, DeviceIntPtr, int);
static Bool ConvertProc(LocalDevicePtr, int, int, int, int, int, int, int, int, int*, int*);
static Bool QueryHardware(LocalDevicePtr);
static Bool DeviceInit(DeviceIntPtr);
static Bool DeviceOn(DeviceIntPtr);
static Bool DeviceOff(DeviceIntPtr);
static Bool DeviceInit(DeviceIntPtr);
static Bool SynapticsGetPacket(LocalDevicePtr, SynapticsPrivatePtr);
static void PrintIdent(SynapticsPrivatePtr);
/* 
 *    DO NOT PUT ANYTHING AFTER THIS ENDIF
 */
#endif

/* Local Variables: */
/* tab-width: 4 */
/* End: */
/* vim:ts=4:sw=4:cindent:
*/
