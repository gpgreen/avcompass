#ifndef CANAEROMSG_H_
#define CANAEROMSG_H_

#include "canaero.h"

// normal operating data message templates
extern canaero_msg_tmpl_t nod_msg_templates[];
extern int num_nod_templates;

// service message reply functions
extern reply_svc_fn* nsl_dispatcher_fn_array[];

// function to run when a message is received
extern get_incoming_msg_fn* incoming_msg_dispatcher_fn;

#endif	// CANAEROMSG_H_
