/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2020
 *
 * Author(s): Guvenc Gulce <guvenc@linux.ibm.com>
 *
 * Userspace program for SMC Information display
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
#include "dev.h"

static int netdev_entered = 0;
static int ibdev_entered = 0;
static int type_entered = 0;
static int all_entered = 0;
static int dev_smcr = 1;
static int dev_smcd = 0;
static int d_level = 0;

static char target_ibdev[IB_DEVICE_NAME_MAX] = {0};
static char target_type[SMC_TYPE_STR_MAX] = {0};
static char target_ndev[IFNAMSIZ] = {0};

static void usage(void)
{
	fprintf(stderr,
		"Usage: smc device [show] [all] [type {smcd | smcr}]\n"
		"                               [ibdev <dev>]\n"
		"                               [netdev <dev>]\n");
	exit(-1);
}

static const char *smc_ib_port_state(unsigned int x)
{
	static char buf[16];

	switch (x) {
	case 0:		return "INACTIVE";
	case 1:		return "ACTIVE";
	default:	sprintf(buf, "%#x?", x); return buf;
	}
}

static const char *smc_ib_dev_type(unsigned int x)
{
	static char buf[16];

	switch (x) {
	case 0x1004:		return "RoCE_Express";
	case 0x1016:		return "RoCE_Express2";
	case 0x04ed:		return "ISM";
	default:	sprintf(buf, "%#x", x); return buf;
	}
}

static void print_devs_smcd_header(void)
{
	printf("FID  ");
	printf("Type  ");
	printf("PCI-ID        ");
	printf("PCHID  ");
	printf("Crit  ");
	printf("#LGs  ");
	printf("PNET-ID  ");
	printf("\n");
}

static void print_devs_smcr_header(void)
{
	printf("Net-Dev         ");
	printf("IB-Dev    ");
	printf("IB-P  ");
	printf("IB-State  ");
	printf("Type          ");
	printf("Crit  ");
	if (d_level >= SMC_DETAIL_LEVEL_V) {
		printf(" FID   ");
		printf("PCI-ID        ");
		printf("PCHID  ");
	}
	printf("#Links  ");
	printf("PNET-ID  ");
	printf("\n");
}

static int filter_item(struct smc_diag_dev_info *dev, int idx)
{
	int ignore = 0;

	if (is_str_empty(target_ibdev) && is_str_empty(target_ndev)) {
		return ignore; /* No filter set */
	} else if (!is_str_empty(target_ndev)) {
		if (strncmp(target_ndev, (char *)dev->netdev[idx], sizeof(target_ndev)) == 0)
			ignore = 0;
		else
			ignore = 1;
	} else if (!is_str_empty(target_ibdev)) {
		if (strncmp(target_ibdev, (char *)dev->dev_name, sizeof(target_ibdev)) == 0)
			ignore = 0;
		else
			ignore = 1;
	}

	return ignore;
}

static void show_devs_smcr_details(struct smc_diag_dev_info *dev, int idx)
{
	char buf[SMC_MAX_PNETID_LEN+1] = {0};

	if (dev->port_valid[idx] && !filter_item(dev, idx)) {
		printf("%-15s ", (char *)&dev->netdev[idx]);
		printf("%-.8s    ",  dev->dev_name);
		printf("%4d  ", idx+1);
		printf("%8s  ", smc_ib_port_state(dev->port_state[idx]));
		printf("%-13s  ", smc_ib_dev_type(dev->pci_device));
		printf("%3s   ", dev->is_critical?"Yes":"No");
		if (d_level >= SMC_DETAIL_LEVEL_V) {
			printf("%04x  ", dev->pci_fid);
			printf("%-12s  ", dev->pci_id);
			printf("%04x    ", dev->pci_pchid);
		}
		printf("%5d ", dev->lnk_cnt_by_port[idx]);
		if (dev->pnetid_by_user[idx])
			snprintf(buf, sizeof(buf),"*%s", dev->pnet_id[idx]);
		else
			snprintf(buf, sizeof(buf),"%s", dev->pnet_id[idx]);
		printf(" %-16s ", trim_space(buf));
		printf("\n");
	}
}

static void show_dev_smcr_info(struct rtattr *tb[])
{
	struct smc_diag_dev_info dev;
	int i;

	if (tb[SMC_DIAG_DEV_INFO_SMCR]) {
		dev = *(struct smc_diag_dev_info *)RTA_DATA(tb[SMC_DIAG_DEV_INFO_SMCR]);
		for (i = 0; i < SMC_MAX_PORTS; i++) {
			show_devs_smcr_details(&dev, i);
		}
	}
}

static void show_dev_smcd_info(struct rtattr *tb[])
{
	char buf[SMC_MAX_PNETID_LEN+1] = {0};
	struct smc_diag_dev_info dev;

	if (tb[SMC_DIAG_DEV_INFO_SMCD]) {
		dev = *(struct smc_diag_dev_info *)RTA_DATA(tb[SMC_DIAG_DEV_INFO_SMCD]);
		printf("%04x ", dev.pci_fid);
		printf("%-4s  ", smc_ib_dev_type(dev.pci_device));
		printf("%-12s  ", dev.pci_id);
		printf("%04x   ", dev.pci_pchid);
		printf("%-3s  ", dev.is_critical?"Yes":"No");
		printf("%5d ", dev.use_cnt);
		if (dev.pnetid_by_user[0])
			snprintf(buf, sizeof(buf),"*%s", dev.pnet_id[0]);
		else
			snprintf(buf, sizeof(buf),"%s", dev.pnet_id[0]);
		printf(" %-16s ", trim_space(buf));
		printf("\n");
	}
}

static void handle_devs_reply(struct nlmsghdr *nlh)
{
	struct rtattr *rt_attr;
	struct rtattr *tb[SMC_DIAG_EXT_MAX + 1];
	static int header_printed = 0;
	static int warning_printed = 0;

	if (!header_printed) {
		if (nlh->nlmsg_seq >= MAGIC_SEQ_V2_ACK) {
			if (dev_smcr)
				print_devs_smcr_header();
			else
				print_devs_smcd_header();
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
		if (dev_smcr && tb[SMC_DIAG_GET_DEV_INFO] && tb[SMC_DIAG_DEV_INFO_SMCR])
			show_dev_smcr_info(tb);
		if (dev_smcd && tb[SMC_DIAG_GET_DEV_INFO] && tb[SMC_DIAG_DEV_INFO_SMCD])
			show_dev_smcd_info(tb);
	}
}

static void handle_cmd_params(int argc, char **argv)
{
	if (((argc == 1) && (contains(argv[0], "help") == 0)) || (argc > 4))
		usage();

	if ((argc > 0) && (contains(argv[0], "show") != 0))
		PREV_ARG(); /* no object given, so use the default "show" */

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
				dev_smcd = 1;
				dev_smcr = 0;
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
		} else {
			usage();
		}
	}
	/* Too many parameters or wrong sequence of parameters */
	if (NEXT_ARG_OK())
		usage();
}

int invoke_devs(int argc, char **argv, int detail_level)
{
	rth.dump = MAGIC_SEQ_V2;
	d_level = detail_level;

	handle_cmd_params(argc, argv);

	if (dev_smcd)
		set_extension(SMC_DIAG_DEV_INFO_SMCD);
	else
		set_extension(SMC_DIAG_DEV_INFO_SMCR);

	sockdiag_send(rth.fd, SMC_DIAG_GET_DEV_INFO);
	rtnl_dump(&rth, handle_devs_reply);

	return 0;
}
