/*
 *  Copyright (c) 2014 T-Platforms, JSC. All Rights Reserved.
 *
 */


#include <stdlib.h>
#include <stdio.h>


#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_raw.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_tpoem.h>



static void ipmi_tpoem_usage(void) {
	lprintf(LOG_NOTICE, "usage tpoem <command> [option...]");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "lom mac <port>");
	lprintf(LOG_NOTICE, "");
}

static int ipmi_tpoem_lom_mac(struct ipmi_intf * intf, uint8_t port)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t data[2];

	if ( port > 2 || port < 1 ) {
		lprintf(LOG_NOTICE, "Invalid port number: %d. Should be 1 or 2", port);
		return -1;
	}

	data[0] = port;
	data[1] = 1;
	req.msg.netfn = 0x3a;
	req.msg.cmd = 0x44;
	req.msg.data = data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_NOTICE, "T-Platforms OEM get lom mac command failed");
		return -1;
	}

	lprintf(LOG_NOTICE, "%02X:%02X:%02X:%02X:%02X:%02X", rsp->data[0], rsp->data[1], rsp->data[2], rsp->data[3], rsp->data[4], rsp->data[5]);
	return 0;
}

int ipmi_tpoem_main(struct ipmi_intf * intf, int argc, char ** argv) {

	int rc = (-1);

	if (argc == 0 || strncmp(argv[0], "help", 4) == 0) {
		ipmi_tpoem_usage();
		return 0;
	}

	if (strncmp(argv[0], "lom", 3) ==0) {
		if (argc == 3 && strncmp(argv[1], "mac", 3) == 0) {
			uint8_t prt = 0;
			if(str2uchar(argv[2], &prt) != 0 ) {
				lprintf(LOG_ERR, "Port number must be specified: 1 or 2");
				return -1;
			}

			rc = ipmi_tpoem_lom_mac(intf, prt);
		}
	}

	return rc;
}

