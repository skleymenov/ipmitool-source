/*
 *  Copyright (c) 2014 T-platforms, JSC.  All Rights Reserved.
 *
 */

#ifndef IPMI_TPOEM_H
#define IPMI_TPOEM_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_sdr.h>

#define IPMI_TPOEM_FW 0x32           /* NetFn for firmware update procedures */
#define IPMI_TPOEM_FW_GET_SIP 0x8a   /* to get server IP */
#define IPMI_TPOME_FW_SET_SIP 0x89   /* to set server IP / URL. Data length 200. Zero terminated strig. */

int ipmi_tpoem_main(struct ipmi_intf * intf, int argc, char ** argv);

#endif /* IPMI_TPOEM_H */
