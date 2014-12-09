/* Copyright (c) 2014 T-Platforms, JSC.  All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistribution of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistribution in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of T-Platforms, JSC. or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * T-PLATFORMS, JSC. ("T-PLATFORMS") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * T-PLATFORMS OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF T-PLATFORMS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
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
#include <ipmitool/ipmi_tploem.h>

#define IPMI_TPLOEM_FW 0x32           /* NetFn for firmware update procedures */

#define IPMI_TPLOEM_FW_SET_TRANS 0x8b /* Cmd to set transport protocol type */
#define IPMI_TPLOEM_FW_TRANS_HTTP 0   /* HTTP transport protocol type */
#define IPMI_TPLOEM_FW_TRANS_TFTP 1   /* TFTP transport protocol type */

#define IPMI_TPLOEM_FW_GET 0x8a       /* Cmd to get a fwupdate parameter */
#define IPMI_TPLOEM_FW_SET 0x89       /* Cmd to set a fwupdate parameter */

#define IPMI_TPLOEM_FW_IP 1      /* Server IP address. Data length: 200 byte.
                                    String in ASCII HEX, zero byte terminated */
#define IPMI_TPLOEM_FW_FILE 2    /* Firmware file name. Data length: 200 byte.
                                    String in ASCII HEX, zero byte terminated */
#define IPMI_TPLOEM_FW_RETRY 3        /* Download firmware file retry count */
#define IPMI_TPLOEM_FW_TYPE 4         /* Firmware type. */
#define IPMI_TPLOEM_FW_BMC 0          /* BMC firmware type. */
#define IPMI_TPLOEM_FW_BIOS 1         /* BIOS firmware type */

#define IPMI_TPLOEM_FW_STATUS 0x88    /* Cmd to get firmware update status */

#define IPMI_TPLOEM_FW_START 0x87     /* Cmd to start firmware update process */
#define IPMI_TPLOEM_FW_CONF_PRES 1    /* Preserve configuration */
#define IPMI_TPLOEM_FW_CONF_CLR  0    /* Do not preserve configuration */

typedef enum {
    no_action,      /* No action */
    par_check_progress, /* Parameter check in progress */
    par_check_success,  /* Parameter check succeed */
    par_check_fail,     /* Parameter check failed */
    img_dwn_progress,   /* Image download in progress */
    img_dwn_success,    /* Image download succeed */
    img_dwn_fail,       /* Image download failed */
    reserved,       /* Reserved :)  */
    img_flash_progress, /* Image flash in Progress */
    img_flash_success,  /* Image flash succeed */
    img_flash_fail,     /* Image flash failed */
    img_ver_progress,   /* Image verification in progress */
    img_ver_success,    /* Image verification succeed */
    img_ver_fail        /* Image verification failed */
} tploem_fwupdate_status;

const char *tploem_fwupdate_status_str[14] = {
        "No action\0",
        "Parameter check in progress\0",
        "Parameter check succeed\0",
        "Parameter check failed\0",
        "Image download in progress\0",
        "Image download succeed\0",
        "Image download failed\0",
        "Reserved :)\0",
        "Image flash in Progress\0",
        "Image flash succeed\0",
        "Image flash failed\0",
        "Image verification in progress\0",
        "Image verification succeed\0",
        "Image verification failed\0"
};

static void ipmi_tploem_usage(void)
{
    lprintf(LOG_NOTICE, "Usage: tploem <command> [option...]");
    lprintf(LOG_NOTICE, "");
    lprintf(LOG_NOTICE, "Commands:");
    lprintf(LOG_NOTICE, "   - lom mac [port]");
    lprintf(LOG_NOTICE, "      Get LOM (Lan-On-Mainboard) MAC address.");
    lprintf(LOG_NOTICE, "      Optional [port] value specifies the LOM port number (1 or 2).");
    lprintf(LOG_NOTICE, "   - fwupdate <subcommand> [option...]");
    lprintf(LOG_NOTICE, "      Firmware update group of commands. Use \"fwupdate help\" to get more info.");
    lprintf(LOG_NOTICE, "");
}

static void ipmi_tploem_fwupdate_usage(void)
{
    lprintf(LOG_NOTICE, "Usage: tploem fwupdate <subcommand> [option...]");
    lprintf(LOG_NOTICE, "");
    lprintf(LOG_NOTICE, "Subcommands:");
    lprintf(LOG_NOTICE, "   - info");
    lprintf(LOG_NOTICE, "      Get firmware update configuration summary");
    lprintf(LOG_NOTICE, "   - set <option> <value>");
    lprintf(LOG_NOTICE, "      Set firmware update configuration. Valid options are:");
    lprintf(LOG_NOTICE, "         - transport <http|tftp>");
    lprintf(LOG_NOTICE, "            Set transport protocol to download firmware image.");
    lprintf(LOG_NOTICE, "            Supported protocols are http and tftp.");
    lprintf(LOG_NOTICE, "         - server-ip <ip>");
    lprintf(LOG_NOTICE, "            Set the server IP address to download firmware image from.");
    lprintf(LOG_NOTICE, "         - filename <name>");
    lprintf(LOG_NOTICE, "            Set firmware image filename and path on the server.");
    lprintf(LOG_NOTICE, "         - retry <count>");
    lprintf(LOG_NOTICE, "            Set retry count for downloading the firmware image.");
    lprintf(LOG_NOTICE, "            Default is 0.");
    lprintf(LOG_NOTICE, "         - type <bios|bmc>");
    lprintf(LOG_NOTICE, "            Set the firmware type to update. Valid values are bios and bmc.");
    lprintf(LOG_NOTICE, "   - status");
    lprintf(LOG_NOTICE, "      Get the current firmware update status.");
    lprintf(LOG_NOTICE, "");
}

/*
 * ipmi_tploem_fwupdate_set_transport()
 * Sets transport protocol to download firmware image.
 * 
 * @intf: pointer a ipmi interface
 * @trans: tranport protocol. Use IPMI_TPL_FW_TRANS_* constants
 *
 * Returns 0 on success or -1 is case of a failure
*/
static int ipmi_tploem_fwupdate_set_transport(struct ipmi_intf * intf,
                                             uint8_t transport)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;

    req.msg.netfn = IPMI_TPLOEM_FW;
    req.msg.cmd = IPMI_TPLOEM_FW_SET_TRANS;
    req.msg.data = &transport;
    req.msg.data_len = 1;

    rsp = intf->sendrecv(intf, &req);


    if (rsp == NULL || rsp->ccode > 0) {
        lprintf(LOG_NOTICE,
            "T-platforms OEM set firmware udate transport protocol \
                                                               command failed");
        return -1;
    }

    return 0;
}

/*
 * ipmi_tploem_fwupdate_set_serverip()
 * Sets the server IP address to download firmware image from.
 *
 * @intf: pointer to a ipmi interface
 * @ip: Null character terminated string containing IP address
 *
 * Returns 0 on success or -1 in case of a failure.
 */
static int ipmi_tploem_fwupdate_set_serverip(struct ipmi_intf * intf, char * ip)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    uint8_t *data;

    data = malloc(201);
    if (data == NULL)
        return -1;
    data[0] = IPMI_TPLOEM_FW_IP;
    strncpy(data + 1, ip, 201);

    req.msg.netfn = IPMI_TPLOEM_FW;
    req.msg.cmd = IPMI_TPLOEM_FW_SET;
    req.msg.data = data;
    req.msg.data_len = 201;

    rsp = intf->sendrecv(intf, &req);

    if (rsp == NULL || rsp->ccode > 0) {
        lprintf(LOG_NOTICE,
            "T-platforms OEM set firmware udate server ip command failed");
        return -1;
    }

    return 0;
}

/*
 * ipmi_tploem_fwupdate_get_serverip()
 * Gets the server IP address to download firmware image from.
 *
 * @intf: pointer to a ipmi interface
 * @buf:  pointer to a buffer to store server ip string (Up to 16 bytes)
 *
 * Returns 0 on success or -1 in case of a failure.
 */
static int
ipmi_tploem_fwupdate_get_serverip(struct ipmi_intf * intf, char * buf)
{
    struct ipmi_rs * rsp;
    struct ipmi_rq req;
    int data = IPMI_TPLOEM_FW_IP;

    req.msg.netfn = IPMI_TPLOEM_FW;
    req.msg.cmd = IPMI_TPLOEM_FW_GET;
    req.msg.data = &data;
    req.msg.data_len = 1;

    rsp = intf->sendrecv(intf, &req);

    if (rsp == NULL || rsp->ccode > 0) {
        lprintf(LOG_NOTICE,
            "T-platforms OEM get firmware udate server ip command failed");
        return -1;
    }

    strcpy(buf,rsp->data);

    return 0;
}

/*
 * ipmi_tploem_fwupdate_set_filename()
 * Sets firmware image filename and path on the server.
 *
 * @intf: pointer to a ipmi interface
 * @filename: Null character terminated string containing filename
 *
 * Returns 0 on success or -1 in case of a failure.
 */
static int
ipmi_tploem_fwupdate_set_filename(struct ipmi_intf * intf, char * filename)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    uint8_t *data;

    data = malloc(201);
    if (data == NULL)
        return -1;
    data[0] = IPMI_TPLOEM_FW_FILE;
    strncpy(data + 1, filename, 200);

    req.msg.netfn = IPMI_TPLOEM_FW;
    req.msg.cmd = IPMI_TPLOEM_FW_SET;
    req.msg.data = data;
    req.msg.data_len = 201;

    rsp = intf->sendrecv(intf, &req);

    if (rsp == NULL || rsp->ccode > 0) {
        lprintf(LOG_NOTICE,
            "T-platforms OEM set firmware update filename command failed");
        return -1;
    }

    return 0;
}

/*
 * ipmi_tploem_fwupdate_get_filename()
 * Gets firmware image filename and path on the server.
 *
 * @intf: pointer to a ipmi interface
 * @buf:  pointer to a buffer to store filename string (Up to 200 bytes)
 *
 * Returns 0 on success or -1 in case of a failure.
 */
static int ipmi_tploem_fwupdate_get_filename(struct ipmi_intf * intf, char * buf)
{
    struct ipmi_rs * rsp;
    struct ipmi_rq req;
    int data = IPMI_TPLOEM_FW_FILE;

    req.msg.netfn = IPMI_TPLOEM_FW;
    req.msg.cmd = IPMI_TPLOEM_FW_GET;
    req.msg.data = &data;
    req.msg.data_len = 1;

    rsp = intf->sendrecv(intf, &req);

    if (rsp == NULL || rsp->ccode > 0) {
        lprintf(LOG_NOTICE,
            "T-platforms OEM get firmware image filename command failed");
        return -1;
    }

    strcpy(buf,rsp->data);

    return 0;
}

/*
 * ipmi_tploem_fwupdate_set_retry()
 * Sets retry count for downloading the firmware image.
 * 
 * @intf: pointer a ipmi interface
 * @retry: retry count
 *
 * Returns 0 on success or -1 is case of a failure
*/
static int
ipmi_tploem_fwupdate_set_retry(struct ipmi_intf * intf, uint8_t retry)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    uint8_t data[2];

    data[0] = IPMI_TPLOEM_FW_RETRY;
    data[1] = retry;

    req.msg.netfn = IPMI_TPLOEM_FW;
    req.msg.cmd = IPMI_TPLOEM_FW_SET;
    req.msg.data = data;
    req.msg.data_len = 2;

    rsp = intf->sendrecv(intf, &req);


    if (rsp == NULL || rsp->ccode > 0) {
        lprintf(LOG_NOTICE,
            "T-platforms OEM set firmware udate retry count command failed");
        return -1;
    }

    return 0;
}

/*
 * ipmi_tploem_fwupdate_get_retry()
 * Getss retry count for downloading the firmware image.
 * 
 * @intf: pointer a ipmi interface
 * @retry: pointer to retry count to be stored
 *
 * Returns 0 on success or -1 is case of a failure
*/
static int
ipmi_tploem_fwupdate_get_retry(struct ipmi_intf * intf, uint8_t * retry)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    uint8_t data;

    data = IPMI_TPLOEM_FW_RETRY;

    req.msg.netfn = IPMI_TPLOEM_FW;
    req.msg.cmd = IPMI_TPLOEM_FW_GET;
    req.msg.data = &data;
    req.msg.data_len = 1;

    rsp = intf->sendrecv(intf, &req);


    if (rsp == NULL || rsp->ccode > 0) {
        lprintf(LOG_NOTICE,
            "T-platforms OEM get firmware udate retry count command failed");
        return -1;
    }

    *retry = rsp->data[0];

    return 0;
}

/*
 * ipmi_tploem_fwupdate_set_type()
 * Sets the firmware type to update.
 * Valid values are IPMI_TPLOEM_FW_BIOS and IPMI_TPLOEM_FW_BMC.
 * 
 * @intf: pointer a ipmi interface
 * @type: firmware type
 *
 * Returns 0 on success or -1 is case of a failure
*/
static int
ipmi_tploem_fwupdate_set_type(struct ipmi_intf * intf, uint8_t type)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    uint8_t data[2];

    data[0] = IPMI_TPLOEM_FW_TYPE;
    data[1] = type;

    req.msg.netfn = IPMI_TPLOEM_FW;
    req.msg.cmd = IPMI_TPLOEM_FW_SET;
    req.msg.data = data;
    req.msg.data_len = 2;

    rsp = intf->sendrecv(intf, &req);


    if (rsp == NULL || rsp->ccode > 0) {
        lprintf(LOG_NOTICE,
            "T-platforms OEM set firmware type command failed");
        return -1;
    }

    return 0;
}

/*
 * ipmi_tploem_fwupdate_get_type()
 * Gets the firmware type to update.
 * 
 * @intf: pointer a ipmi interface
 * @retry: pointer to type to be stored
 *
 * Returns 0 on success or -1 is case of a failure
*/
static int
ipmi_tploem_fwupdate_get_type(struct ipmi_intf * intf, uint8_t * type)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    uint8_t data;

    data = IPMI_TPLOEM_FW_TYPE;

    req.msg.netfn = IPMI_TPLOEM_FW;
    req.msg.cmd = IPMI_TPLOEM_FW_GET;
    req.msg.data = &data;
    req.msg.data_len = 1;

    rsp = intf->sendrecv(intf, &req);


    if (rsp == NULL || rsp->ccode > 0) {
        lprintf(LOG_NOTICE,
            "T-platforms OEM get firmware type command failed");
        return -1;
    }

    *type = rsp->data[0];

    return 0;
}

/*
 * ipmi_tploem_fwupdate_get_status()
 * Gets the current firmware update status.
 * 
 * @intf: pointer a ipmi interface
 *
 * Returns 0 on success or -1 is case of a failure
*/
static int
ipmi_tploem_fwupdate_get_status(struct ipmi_intf * intf)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    tploem_fwupdate_status status;
    uint8_t data = 0;

    req.msg.netfn = IPMI_TPLOEM_FW;
    req.msg.cmd = IPMI_TPLOEM_FW_STATUS;
    req.msg.data = &data;
    req.msg.data_len = 0;

    rsp = intf->sendrecv(intf, &req);


    if (rsp == NULL || rsp->ccode > 0) {
        lprintf(LOG_NOTICE,
            "T-platforms OEM get firmware udate status command failed");
        return -1;
    }

    status = rsp->data[0];
    if (status == par_check_progress || status == img_dwn_progress ||
        status == img_flash_progress || status == img_ver_progress) {
        lprintf(LOG_NOTICE, "%s: %d%% done.", tploem_fwupdate_status_str[status],
                        rsp->data[1]);
    }
    else {
        lprintf(LOG_NOTICE, "%s.", tploem_fwupdate_status_str[status]);
    }

    return 0;
}

/*
 * ipmi_tploem_fwupdate_start
 * Starts firmware update process.
 * 
 * @intf: pointer a ipmi interface
 * @config: Do preserve configuration or not
 *          Use IPMI_TPLOEM_FW_CONF_CLR or IPMI_TPLOEM_FW_CONF_PRES constants.
 *
 * Returns 0 on success or -1 is case of a failure
*/
static int
ipmi_tploem_fwupdate_start(struct ipmi_intf * intf, int8_t config)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    uint8_t data[2];
    
    data[0] = 0;
    data[1] = config;

    req.msg.netfn = IPMI_TPLOEM_FW;
    req.msg.cmd = IPMI_TPLOEM_FW_START;
    req.msg.data = data;
    req.msg.data_len = 2;

    rsp = intf->sendrecv(intf, &req);


    if (rsp == NULL || rsp->ccode > 0) {
        lprintf(LOG_NOTICE,
            "T-platforms OEM start firmware update command failed");
        return -1;
    }

    return 0;
}

static int ipmi_tploem_lom_mac(struct ipmi_intf *intf, uint8_t port, char * buf)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    uint8_t data[2];

    if (port > 2 || port < 1) {
        lprintf(LOG_NOTICE, "Invalid port number: %d. Should be 1 or 2",
            port);
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
        lprintf(LOG_NOTICE,
            "T-Platforms OEM get lom mac command failed");
        return -1;
    }

    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", rsp->data[0],
        rsp->data[1], rsp->data[2], rsp->data[3], rsp->data[4],
        rsp->data[5]);
    return 0;
}

int ipmi_tploem_main(struct ipmi_intf *intf, int argc, char **argv)
{

    int rc = (-1);

    if (argc == 0 || !strncmp(argv[0], "help", 4)) {
        ipmi_tploem_usage();
        return 0;
    }

    if (!strncmp(argv[0], "lom", 3)) {
        char mac[18];
        if (argc == 3 && !strncmp(argv[1], "mac", 3)) {
            uint8_t prt = 0;
            if (str2uchar(argv[2], &prt)) {
                lprintf(LOG_ERR,
                    "Port number must be specified: 1 or 2");
                return -1;
            }

            rc = ipmi_tploem_lom_mac(intf, prt, mac);
            if(!rc) lprintf(LOG_NOTICE, "%s", mac);
        
        } else if (argc == 2 && !strncmp(argv[1], "mac", 3)) {
            uint8_t prt = 1;
            if (ipmi_tploem_lom_mac(intf, prt, mac)) return -1;
            lprintf(LOG_NOTICE, "Lan-On-Mainboard Port 1 MAC\t: %s", mac);
            prt = 2;
            if (ipmi_tploem_lom_mac(intf, prt, mac)) return -1;
            lprintf(LOG_NOTICE, "Lan-On-Mainboard Port 2 MAC\t: %s", mac);
        } else {
            ipmi_tploem_usage();
            return -1;
        }
    } else if (!strncmp(argv[0], "fwupdate", 8)) {
        if (argc == 4 && !strncmp(argv[1], "set", 3)) {
            if (!strncmp(argv[2], "transport", 9)) {
                uint8_t transport;
                if (!strncmp(argv[3], "http", 4) ||!strncmp(argv[3], "HTTP", 4) ||
                    !strncmp(argv[3], "https", 5) || !strncmp(argv[3], "HTTPS", 5) ||
                    !strncmp(argv[3], "HTTPs", 5)) {
                    transport = IPMI_TPLOEM_FW_TRANS_HTTP;
                    if(ipmi_tploem_fwupdate_set_transport(intf, transport)) return -1;
                    return 0;

                } else if (!strncmp(argv[3], "tftp", 4) || !strncmp(argv[3], "TFTP", 4)) {
                    transport = IPMI_TPLOEM_FW_TRANS_TFTP;
                    if(ipmi_tploem_fwupdate_set_transport(intf, transport)) return -1;
                    return 0;

                } else {
                    lprintf(LOG_NOTICE, "Tranport protocol %s does not supported", argv[3]);
                    ipmi_tploem_fwupdate_usage();
                    return -1;

                }
            } else if (!strncmp(argv[2], "server-ip", 9)) {
                if(ipmi_tploem_fwupdate_set_serverip(intf, argv[3])) return -1;
                return 0;

            } else if (!strncmp(argv[2], "filename", 8)) {
                if(ipmi_tploem_fwupdate_set_filename(intf, argv[3])) return -1;
                return 0;

            } else if (!strncmp(argv[2], "retry", 5)) {
                uint8_t retry=0;
                if(str2uchar(argv[3], &retry)) {
                    lprintf(LOG_NOTICE, "Specify a valid retry count");
                    ipmi_tploem_fwupdate_usage();
                    return -1;
                }

                if(ipmi_tploem_fwupdate_set_retry(intf, retry)) return -1;
                return 0;
            } else if (!strncmp(argv[2], "type", 4)) {
                uint8_t type;
                if(!strncmp(argv[3], "bios", 4) || !strncmp(argv[3], "BIOS", 4))
                    type = IPMI_TPLOEM_FW_BIOS;
                else if (!strncmp(argv[3], "bmc", 3) || !strncmp(argv[3], "BMC", 3))
                    type = IPMI_TPLOEM_FW_BMC;
                else {
                    lprintf(LOG_NOTICE, "Unknown firmware type %s", argv[3]);
                    ipmi_tploem_fwupdate_usage();
                    return -1;
                }

                if(ipmi_tploem_fwupdate_set_type(intf, type)) return -1;
                return 0;

            } else {
                lprintf(LOG_NOTICE, "Unknown fwupdate parameter %s", argv[2]);
                ipmi_tploem_fwupdate_usage();
                return 0;

            }
        } else if (argc == 2 && !strncmp(argv[1], "info", 4)) {
             char * serverip = malloc(200);
             char * filename = malloc(200);
             uint8_t retry;
             uint8_t type;
             char * type_str;
             
             if(serverip == NULL || filename == NULL) return -1;

             if(ipmi_tploem_fwupdate_get_serverip(intf, serverip)) return -1;
             if(ipmi_tploem_fwupdate_get_filename(intf, filename)) return -1;
             if(ipmi_tploem_fwupdate_get_retry(intf, &retry)) return -1;
             if(ipmi_tploem_fwupdate_get_type(intf, &type)) return -1;

             if(type == IPMI_TPLOEM_FW_BIOS) type_str = "bios\0";
             else if(type == IPMI_TPLOEM_FW_BMC) type_str = "bmc\0";


             lprintf(LOG_NOTICE, "Transport protocol\t\t: UNKNOWN");
             lprintf(LOG_NOTICE, "Server IP address\t\t: %s", serverip);
             lprintf(LOG_NOTICE, "Image filename\t\t\t: %s", filename);
             lprintf(LOG_NOTICE, "Max. retry count\t\t: %d", retry);
             lprintf(LOG_NOTICE, "Firmware type\t\t\t: %s", type_str);

             return 0;

        } else if (argc == 2 && !strncmp(argv[1], "status", 6)) {
                if(ipmi_tploem_fwupdate_get_status(intf)) return -1;
                return 0;

        } else if (argc >= 2 && !strncmp(argv[1], "start", 5)) {
            uint8_t config = IPMI_TPLOEM_FW_CONF_CLR;

            if(argc == 3) {
                if(!strncmp(argv[2], "preserve-config=yes", 19))
                        config = IPMI_TPLOEM_FW_CONF_PRES;
                else if(!strncmp(argv[2], "preserve-config=no", 18))
                        config = IPMI_TPLOEM_FW_CONF_CLR;
                else {
                    ipmi_tploem_fwupdate_usage();
                }
            }

            if(ipmi_tploem_fwupdate_start(intf, config)) return -1;
            return 0;

        } else {
             ipmi_tploem_fwupdate_usage();
             return 0;
        }
    }

    return 0;
}
