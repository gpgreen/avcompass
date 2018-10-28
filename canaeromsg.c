#include "defs.h"
#include "globals.h"
#include "hmc5883l.h"
#include "canaeromsg.h"
#include "canaero_ids.h"
#include "canaero_bss.h"
#include "canaero_nis.h"
#include "canaero_filters.h"

/*-----------------------------------------------------------------------*/

// message data functions

// units of gauss
static void get_mag_x(can_msg_t *msg)
{
	// x will be inverted, so that north will be towards the can
	// connector on the board
	convert_float_to_big_endian(-hmc5883l_raw_mag_data(0), msg);
}

// units of gauss
static void get_mag_y(can_msg_t *msg)
{
	convert_float_to_big_endian(hmc5883l_raw_mag_data(1), msg);
}

// units of gauss
static void get_mag_z(can_msg_t *msg)
{
	// z will be inverted to match the inversion of x
	convert_float_to_big_endian(-hmc5883l_raw_mag_data(2), msg);
}

// +- 180 degrees, need to convert to 0-360
static void get_mag_heading(can_msg_t *msg)
{
	convert_float_to_big_endian(g_magnetic_heading, msg);
}

/*-----------------------------------------------------------------------*/

// messages defined for this unit
canaero_msg_tmpl_t nod_msg_templates[] = {
	/* mag x */
	{NOD, 0x0d3, 0, FLOAT, 0, 0, get_mag_x},
	/* mag y */
	{NOD, 0x0d4, 0, FLOAT, 0, 0, get_mag_y},
	/* mag z */
	{NOD, 0x0d5, 0, FLOAT, 0, 0, get_mag_z},
	/* magnetic heading */
	{NOD, 0x42d, 0, FLOAT, 0, 0, get_mag_heading},
};

int num_nod_templates = sizeof(nod_msg_templates)
	/ sizeof(canaero_msg_tmpl_t);

/*-----------------------------------------------------------------------*/

// service message data functions

static void get_mis0_data(can_msg_t* msg)
{
	// first byte is 1 if in LISTEN state
	msg->data[4] = (g_state == LISTEN) ? 1 : 0;
	// second byte is 1 if filtering is on
	msg->data[5] = g_ci.can_settings.filters.filtering_on;
}

static void get_mis1_data(can_msg_t* msg)
{
	msg->data[4] = 'C';
	msg->data[5] = 'O';
	msg->data[6] = 'M';
	msg->data[7] = 'P';
}

static void get_mis3_data(can_msg_t* msg)
{
	// byte is 1 if magnetometer enabled
	msg->data[4] = g_compass_enabled;
}

/*-----------------------------------------------------------------------*/

// MIS service reply
// get module configuration
uint8_t reply_mis(service_msg_id_t* svc, can_msg_t* msg)
{
	/* Module Information Service Request code 0 */
	static canaero_svc_msg_tmpl_t t0 = {UCHAR2, 12, 0, get_mis0_data};
	
	/* Module Information Service Request code 1 */
	canaero_svc_msg_tmpl_t t1 = {UCHAR4, 12, 1, get_mis1_data};
	
	/* Module Information Service Request code 3 */
	static canaero_svc_msg_tmpl_t t3 = {UCHAR, 12, 3, get_mis3_data};
	
	/* Module Information Service Request invalid code */
	static canaero_svc_msg_tmpl_t invalid_t = {NODATA, 12, 255, 0};
		
	uint8_t snd_stat;

	// ensure the data format is as we expect
	if (msg->data[1] != NODATA)
	{
		return canaero_send_svc_reply_message(svc, &invalid_t);
	}
	
	// the message code gives the type of module configuration requested
	switch (msg->data[3]) {
	case 0:
		snd_stat = canaero_send_svc_reply_message(svc, &t0);
		break;
	case 1:
		snd_stat = canaero_send_svc_reply_message(svc, &t1);
		break;
	case 3:
		snd_stat = canaero_send_svc_reply_message(svc, &t3);
		break;
	default:
		// we don't respond to these message codes
		snd_stat = canaero_send_svc_reply_message(svc, &invalid_t);
	}
	
	return snd_stat;
}

/*-----------------------------------------------------------------------*/

// MCS service reply
// set module configuration
uint8_t reply_mcs(service_msg_id_t* svc, can_msg_t* msg)
{
	/* Module Information Service Request code 0 */
	static canaero_svc_msg_tmpl_t t0 = {UCHAR2, 13, 0, get_mis0_data};
	
	/* Module Configuration Service Request code 3 */
	static canaero_svc_msg_tmpl_t t3 = {UCHAR, 13, 3, get_mis3_data};
	
	/* Module Configuration Service Request invalid code */
	static canaero_svc_msg_tmpl_t invalid_t = {NODATA, 13, 255, 0};
	
	uint8_t snd_stat;

	// the message code gives the type of module configuration requested
	switch (msg->data[3]) {
	case 0:

		// ensure the data format is as we expect
		if (msg->data[1] != UCHAR2)
		{
			return canaero_send_svc_reply_message(svc, &invalid_t);
		}
	
		// set listen/active state
		if (msg->data[4])
			g_state = LISTEN;
		else
			g_state = ACTIVE;
		// second byte is filtering on/off
		if (g_ci.can_settings.filters.filtering_on != msg->data[5])
		{
			// setup the filters as setting is changed
			if (msg->data[5])
				canaero_high_priority_service_filters(&g_ci);
			else
				canaero_no_filters(&g_ci);
			// reinitialize the canaero stack
			errcode = canaero_init(&g_ci);
			if (errcode == CAN_FAILINIT)
				offline();
		}
		
		snd_stat = canaero_send_svc_reply_message(svc, &t0);
		break;
	case 3:

		// ensure the data format is as we expect
		if (msg->data[1] != UCHAR)
		{
			return canaero_send_svc_reply_message(svc, &invalid_t);
		}
	
		// here we should read the configuration and set it
		g_compass_enabled = msg->data[4];

		snd_stat = canaero_send_svc_reply_message(svc, &t3);
		break;
	default:
		// we don't respond to these message codes
		snd_stat = canaero_send_svc_reply_message(svc, &invalid_t);
	}
	
	return snd_stat;
}

/*-----------------------------------------------------------------------*/

reply_svc_fn* nsl_dispatcher_fn_array[] = {
	// 0 - IDS
	&canaero_reply_ids,
	// 1 - NSS 
	0,
	// 2 - DDS
	0,
	// 3 - DUS
	0,
	// 4 - SCS
	0,
	// 5 - TIS
	0,
	// 6 - FPS
	0,
	// 7 - STS
	0,
	// 8 - FSS
	0,
	// 9 - TCS
	0,
	// 10 - BSS
	&canaero_reply_bss,
	// 11 - NIS
	&canaero_reply_nis,
	// 12 - MIS
	&reply_mis,
	// 13 - MCS
	&reply_mcs,
	// 14 - CSS
	0,
	// 15 - DSS
	0,
};
	
