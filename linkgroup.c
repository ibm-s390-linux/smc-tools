/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2020
 *
 * Author(s): Guvenc Gulce <guvenc@linux.ibm.com>
 *
 * User space program for SMC Information display
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 */
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "smctools_common.h"
#include "util.h"
#include "libnetlink.h"
#include "linkgroup.h"

#define SMC_MASK_LINK_ID 0xFFFFFF00

static __u32 unmasked_trgt_lgid = 0;
static int netdev_entered = 0;
static __u32 target_lgid = 0;
static int ibdev_entered = 0;
static int type_entered = 0;
static int all_entered = 0;
static int show_links = 0;
static int lgr_smcr = 1;
static int lgr_smcd = 0;
static int d_level = 0;

static char target_ibdev[IB_DEVICE_NAME_MAX] = {0};
static char target_type[SMC_TYPE_STR_MAX] = {0};
static char target_ndev[IFNAMSIZ] = {0};

static void usage(void)
{
	fprintf(stderr,
		"Usage: smc linkgroup [show | link-show] [all | LG-ID] [type {smcd | smcr}]\n"
		"                                                      [ibdev <dev>]\n"
		"                                                      [netdev <dev>]\n");
	exit(-1);
}

static void print_lgr_smcr_header(void)
{
	printf("LG-ID    ");
	printf("LG-Role  ");
	printf("LG-Type  ");
	if (show_links) {
		printf("Net-Dev         ");
		printf("Link-State      ");
		printf("#Conns  ");
		if (d_level >= SMC_DETAIL_LEVEL_V) {
			printf("Link-UID  ");
			printf("Peer-UID  ");
			printf("IB-Dev  ");
			printf("IB-P  ");
			if (d_level >= SMC_DETAIL_LEVEL_VV) {
				printf("Local-GID                                 ");
				printf("Peer-GID  ");
			}
		}
	} else {
		printf("VLAN  ");
		printf("#Conns  ");
		printf("PNET-ID ");
	}
	printf("\n");
}

static void print_lgr_smcd_header(void)
{
	printf("LG-ID    ");
	printf("VLAN  ");
	printf("#Conns  ");
	printf("PNET-ID " );

	printf("\n");
}

static const char *smc_link_state(unsigned int x)
{
	static char buf[16];

	switch (x) {
	case 0:		return "LINK_UNUSED";
	case 1:		return "LINK_INACTIVE";
	case 2:		return "LINK_ACTIVATING";
	case 3:		return "LINK_ACTIVE";
	default:	sprintf(buf, "%#x?", x); return buf;
	}
}

static const char *smc_lgr_type(unsigned int x)
{
	static char buf[16];

	switch (x) {
	case 0:		return "NONE";
	case 1:		return "SINGLE";
	case 2:		return "SYM";
	case 3:		return "ASYMP";
	case 4:		return "ASYML";
	default:	sprintf(buf, "%#x?", x); return buf;
	}
}

static int filter_item(struct smc_diag_linkinfo *link, struct smc_diag_lgr *lgr)
{
	int ignore = 0;

	if (is_str_empty(target_ibdev) && is_str_empty(target_ndev) &&
	    (unmasked_trgt_lgid == 0 )) {
		return ignore; /* No filter set */
	}else if (!is_str_empty(target_ndev) && show_links) {
		if (strncmp(target_ndev, (char*)link->netdev, sizeof(target_ndev)) == 0)
			ignore = 0;
		else
			ignore = 1;
	} else if (!is_str_empty(target_ibdev) && show_links) {
		if (strncmp(target_ibdev, (char*)link->ibname, sizeof(target_ibdev)) == 0)
			ignore = 0;
		else
			ignore = 1;
	} else if (unmasked_trgt_lgid != 0 ) {
		if (target_lgid == *(__u32*)lgr->lgr_id)
			ignore = 0;
		else
			ignore = 1;
	}

	return ignore;
}

static void show_lgr_smcr_info(struct rtattr *tb[])
{
	static struct smc_diag_lgr lgr = {0};
	struct smc_diag_linkinfo link = {0};

	if (tb[SMC_DIAG_LGR_INFO_SMCR]) {
		lgr = *(struct smc_diag_lgr *)RTA_DATA(tb[SMC_DIAG_LGR_INFO_SMCR]);
		if (show_links)
			return;
	}
	if (tb[SMC_DIAG_LGR_INFO_SMCR_LINK]) {
		link = *(struct smc_diag_linkinfo *)RTA_DATA(tb[SMC_DIAG_LGR_INFO_SMCR_LINK]);
	}

	if (filter_item(&link, &lgr))
		return;

	printf("%08x ", *(__u32*)lgr.lgr_id);
	printf("%-8s ", lgr.lgr_role ? "SERV" : "CLNT");
	printf("%-8s ", smc_lgr_type(lgr.lgr_type));
	if (show_links) {
		printf("%-15s ", link.netdev);
		printf("%-15s ", smc_link_state(link.link_state));
		printf("%6d  ", link.conn_cnt);
		if (d_level >= SMC_DETAIL_LEVEL_V) {
			printf("%08x  ",  ntohl(*(__u32*)link.link_uid));
			printf("%08x  ", ntohl(*(__u32*)link.peer_link_uid));
			printf("%-.8s  ", link.ibname);
			printf("%4d  ", link.ibport);
			if (d_level >= SMC_DETAIL_LEVEL_VV) {
				printf("%-40s  ", link.gid);
				printf("%s  ", link.peer_gid);
			}
		}
	} else {
		printf("%#4x ", lgr.vlan_id);
		printf(" %6d ", lgr.conns_num);
		printf(" %-16s ", trim_space((char *)lgr.pnet_id));
	}
	printf("\n");
}

static void show_lgr_smcd_info(struct rtattr *tb[])
{
	if (tb[SMC_DIAG_LGR_INFO_SMCD]) {
		struct smcd_diag_dmbinfo lgr;

		lgr = *(struct smcd_diag_dmbinfo *)RTA_DATA(tb[SMC_DIAG_LGR_INFO_SMCD]);
		printf("%08x ", lgr.linkid);
		printf("%#4x  ", lgr.vlan_id);
		printf("%6d  ", lgr.conns_num);
		printf("%-16s ", trim_space((char *)lgr.pnet_id));
		printf("\n");
	}
}

static void handle_lgs_reply(struct nlmsghdr *nlh)
{
	struct rtattr *tb[SMC_DIAG_EXT_MAX + 1];
	static int warning_printed = 0;
	static int header_printed = 0;
	struct rtattr *rt_attr;

	if (!header_printed) {
		if (nlh->nlmsg_seq >= MAGIC_SEQ_V2_ACK) {
			if (lgr_smcd)
				print_lgr_smcd_header();
			else
				print_lgr_smcr_header();
		} else if (nlh->nlmsg_seq == MAGIC_SEQ_V2) {
			/* This is an old kernel (<v2) responding */
			if (!warning_printed) {
				print_unsup_msg();
				warning_printed = 1;
			}
			return;
		}
		header_printed = 1;
	}

	if (nlh->nlmsg_seq >= MAGIC_SEQ_V2_ACK) {
		rt_attr = (struct rtattr *)NLMSG_DATA(nlh);
		parse_rtattr(tb, SMC_DIAG_EXT_MAX, rt_attr,
			     nlh->nlmsg_len - NLMSG_HDRLEN);
		if ((lgr_smcr && tb[SMC_DIAG_GET_LGR_INFO] && tb[SMC_DIAG_LGR_INFO_SMCR]) ||
		    (lgr_smcr && tb[SMC_DIAG_GET_LGR_INFO] && tb[SMC_DIAG_LGR_INFO_SMCR_LINK]))
			show_lgr_smcr_info(tb);
		if (lgr_smcd && tb[SMC_DIAG_GET_LGR_INFO] && tb[SMC_DIAG_LGR_INFO_SMCD])
			show_lgr_smcd_info(tb);
	}
}

static void handle_cmd_params(int argc, char **argv)
{
	if (((argc == 1) && (contains(argv[0], "help") == 0)) || (argc > 4))
		usage();

	if (argc > 0) {
		if (contains(argv[0], "show") == 0)
			show_links=0;
		else if (contains(argv[0], "link-show") == 0)
			show_links=1;
		else
			PREV_ARG(); /* no object given, so use the default "show" */
	}

	while (NEXT_ARG_OK()) {
		NEXT_ARG();
		if (ibdev_entered) {
			snprintf(target_ibdev, sizeof(target_ibdev), "%s", argv[0]);
			ibdev_entered = 0;
			break;
		} else if (netdev_entered) {
			snprintf(target_ndev, sizeof(target_ndev), "%s", argv[0]);
			netdev_entered = 0;
			break;
		} else if (type_entered) {
			snprintf(target_type, sizeof(target_type), "%s", argv[0]);
			if (strncmp(target_type, "smcd", SMC_TYPE_STR_MAX) == 0) {
				lgr_smcd = 1;
				lgr_smcr = 0;
			} else if ((strnlen(target_type, sizeof(target_type)) < 4) ||
				   (strncmp(target_type, "smcr", SMC_TYPE_STR_MAX) != 0)) {
				print_type_error();
			}
			type_entered = 0;
			break;
		} else if (contains(argv[0], "help") == 0) {
			usage();
		} else if (contains(argv[0], "all") == 0) {
			all_entered=1;
		} else if (contains(argv[0], "type") == 0) {
			type_entered=1;
		} else if (contains(argv[0], "ibdev") == 0) {
			ibdev_entered =1;
		} else if (contains(argv[0], "netdev") == 0) {
			netdev_entered =1;
		} else if (!all_entered){
			unmasked_trgt_lgid = (unsigned int)strtol(argv[0], NULL, 16);
			target_lgid = (unmasked_trgt_lgid & SMC_MASK_LINK_ID);
			break;
		} else {
			usage();
		}
	}
	/* Too many parameters or wrong sequence of parameters */
	if (NEXT_ARG_OK())
		usage();
}

int invoke_lgs(int argc, char **argv, int detail_level)
{
	rth.dump = MAGIC_SEQ_V2;
	d_level = detail_level;

	handle_cmd_params(argc, argv);

	if (lgr_smcd) {
		set_extension(SMC_DIAG_LGR_INFO_SMCD);
	} else {
		set_extension(SMC_DIAG_LGR_INFO_SMCR);
		if (show_links)
			set_extension(SMC_DIAG_LGR_INFO_SMCR_LINK);
	}
	sockdiag_send(rth.fd, SMC_DIAG_GET_LGR_INFO);
	rtnl_dump(&rth, handle_lgs_reply);
	return 0;
}
