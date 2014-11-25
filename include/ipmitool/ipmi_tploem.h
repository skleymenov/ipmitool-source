/*
 *  Copyright (c) 2014 T-platforms, JSC.  All Rights Reserved.
 *
 */

#ifndef IPMI_TPLOEM_H
#define IPMI_TPLOEM_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_sdr.h>


int ipmi_tploem_main(struct ipmi_intf * intf, int argc, char ** argv);

#endif /* IPMI_TPLOEM_H */
