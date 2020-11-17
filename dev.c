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
#if defined(SMCD)
static int dev_smcr = 0;
static int dev_smcd = 1;
#else
static int dev_smcr = 1;
static int dev_smcd = 0;
#endif

static int d_level = 0;

static char target_ibdev[IB_DEVICE_NAME_MAX] = {0};
static char target_type[SMC_TYPE_STR_MAX] = {0};
static char target_ndev[IFNAMSIZ] = {0};

static struct nla_policy smc_gen_dev_smcd_sock_policy[SMC_NLA_DEV_MAX + 1] = {
	[SMC_NLA_DEV_UNSPEC]		= { .type = NLA_UNSPEC },
	[SMC_NLA_DEV_USE_CNT]		= { .type = NLA_U32 },
	[SMC_NLA_DEV_IS_CRIT]		= { .type = NLA_U8 },
	[SMC_NLA_DEV_PCI_FID]		= { .type = NLA_U32 },
	[SMC_NLA_DEV_PCI_CHID]		= { .type = NLA_U16 },
	[SMC_NLA_DEV_PCI_VENDOR]	= { .type = NLA_U16 },
	[SMC_NLA_DEV_PCI_DEVICE]	= { .type = NLA_U16 },
	[SMC_NLA_DEV_PCI_ID]		= { .type = NLA_NUL_STRING },
	[SMC_NLA_DEV_PORT]		= { .type = NLA_NESTED },
};

static struct nla_policy smc_gen_dev_port_smcd_sock_policy[SMC_NLA_DEV_PORT_MAX + 1] = {
	[SMC_NLA_DEV_PORT_UNSPEC]	= { .type = NLA_UNSPEC },
	[SMC_NLA_DEV_PORT_PNET_USR]	= { .type = NLA_U8 },
	[SMC_NLA_DEV_PORT_PNETID]	= { .type = NLA_NUL_STRING },
};

static void usage(void)
{
	fprintf(stderr,
#if defined(SMCD)
		"Usage: smcd device [show] [all]\n"
#elif defined(SMCR)
		"Usage: smcr device [show] [all]\n"
		"                          [ibdev <dev>]\n"
		"                          [netdev <dev>]\n"
#else
		"Usage: smc device [show] [all] [type {smcd | smcr}]\n"
		"                               [ibdev <dev>]\n"
		"                               [netdev <dev>]\n"
#endif
	);
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
	printf("InUse  ");
	printf("#LGs  ");
	printf("PNET-ID  ");
	printf("\n");
}

static void print_devs_smcr_header(void)
{
	printf("Net-Dev         ");
	printf("IB-Dev   ");
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
		if (strnlen((char*)dev->netdev[idx], sizeof(dev->netdev[idx])) > (IFNAMSIZ - 1))
			printf("%-.15s ", (char *)&dev->netdev[idx]);
		else
			printf("%-15s ", (char *)&dev->netdev[idx]);
		if (strnlen((char*)dev->dev_name, sizeof(dev->dev_name)) > SMC_MAX_IBNAME)
			printf("%-.8s ", dev->dev_name);
		else
			printf("%-8s ", dev->dev_name);
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

static int fill_dev_port_smcd_struct(struct smc_diag_dev_info *dev, struct nlattr **attrs, int idx)
{
	struct nlattr *port_attrs[SMC_NLA_DEV_PORT_MAX + 1];

	if (nla_parse_nested(port_attrs, SMC_NLA_DEV_PORT_MAX,
			     attrs[SMC_NLA_DEV_PORT],
			     smc_gen_dev_port_smcd_sock_policy)) {
		fprintf(stderr, "failed to parse nested attributes: smc_gen_dev_port_smcd_sock_policy\n");
		return NL_STOP;
	}
	if (port_attrs[SMC_NLA_DEV_PORT_PNETID])
		snprintf((char*)&dev->pnet_id[idx], sizeof(dev->pnet_id[idx]), "%s",
				nla_get_string(port_attrs[SMC_NLA_DEV_PORT_PNETID]));
	if (port_attrs[SMC_NLA_DEV_PORT_PNET_USR])
		dev->pnetid_by_user[idx] = nla_get_u8(port_attrs[SMC_NLA_DEV_PORT_PNET_USR]);

	return NL_OK;
}

static int fill_dev_smcd_struct(struct smc_diag_dev_info *dev, struct nlattr **attrs)
{
	struct nlattr *dev_attrs[SMC_NLA_DEV_MAX + 1];

	if (nla_parse_nested(dev_attrs, SMC_NLA_DEV_MAX,
			     attrs[SMC_GEN_DEV_SMCD],
			     smc_gen_dev_smcd_sock_policy)) {
		fprintf(stderr, "failed to parse nested attributes: smc_gen_dev_smcd_sock_policy\n");
		return NL_STOP;
	}
	if (dev_attrs[SMC_NLA_DEV_USE_CNT])
		dev->use_cnt = nla_get_u32(dev_attrs[SMC_NLA_DEV_USE_CNT]);
	if (dev_attrs[SMC_NLA_DEV_IS_CRIT])
		dev->is_critical = nla_get_u8(dev_attrs[SMC_NLA_DEV_IS_CRIT]);
	if (dev_attrs[SMC_NLA_DEV_PCI_FID])
		dev->pci_fid = nla_get_u32(dev_attrs[SMC_NLA_DEV_PCI_FID]);
	if (dev_attrs[SMC_NLA_DEV_PCI_CHID])
		dev->pci_pchid = nla_get_u16(dev_attrs[SMC_NLA_DEV_PCI_CHID]);
	if (dev_attrs[SMC_NLA_DEV_PCI_VENDOR])
		dev->pci_vendor = nla_get_u16(dev_attrs[SMC_NLA_DEV_PCI_VENDOR]);
	if (dev_attrs[SMC_NLA_DEV_PCI_DEVICE])
		dev->pci_device = nla_get_u16(dev_attrs[SMC_NLA_DEV_PCI_DEVICE]);
	if (dev_attrs[SMC_NLA_DEV_PCI_ID])
		snprintf((char*)dev->pci_id, sizeof(dev->pci_id), "%s",
			 nla_get_string(dev_attrs[SMC_NLA_DEV_PCI_ID]));

	if (fill_dev_port_smcd_struct(dev, &dev_attrs[0], 0) != NL_OK)
		return NL_STOP;

	return NL_OK;
}

static int show_dev_smcd_info(struct nlattr **attr)
{
	char buf[SMC_MAX_PNETID_LEN+1] = {0};
	struct smc_diag_dev_info dev;

	if (attr[SMC_GEN_DEV_SMCD]) {
		if (fill_dev_smcd_struct(&dev, attr) != NL_OK)
			return NL_STOP;
		printf("%04x ", dev.pci_fid);
		printf("%-4s  ", smc_ib_dev_type(dev.pci_device));
		printf("%-12s  ", dev.pci_id);
		printf("%04x   ", dev.pci_pchid);
		printf("%-4s  ", dev.is_critical?"Yes":"No");
		printf("%5d ", dev.use_cnt);
		if (dev.pnetid_by_user[0])
			snprintf(buf, sizeof(buf),"*%s", dev.pnet_id[0]);
		else
			snprintf(buf, sizeof(buf),"%s", dev.pnet_id[0]);
		printf(" %-16s ", trim_space(buf));
		printf("\n");
	}

	return NL_OK;
}

static int handle_gen_dev_reply(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[SMC_GEN_MAX + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);
	static int header_printed = 0;
	int rc = NL_OK;

	if (!header_printed) {
		if (dev_smcr)
			print_devs_smcr_header();
		else
			print_devs_smcd_header();
		header_printed = 1;
	}

	if (genlmsg_parse(hdr, 0, attrs, SMC_GEN_MAX,
			  (struct nla_policy *)smc_gen_net_policy) < 0) {
		fprintf(stderr, "invalid data returned: smc_gen_net_policy\n");
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}

	if (!attrs[SMC_GEN_DEV_SMCD])
		return NL_STOP;

	if (dev_smcd && attrs[SMC_GEN_DEV_SMCD])
		rc = show_dev_smcd_info(&attrs[0]);

	return rc;
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
#if !defined(SMCD) && !defined(SMCR)
		} else if (contains(argv[0], "type") == 0) {
			type_entered=1;
#endif
#if !defined(SMCD)
		} else if (contains(argv[0], "ibdev") == 0) {
			ibdev_entered =1;
		} else if (contains(argv[0], "netdev") == 0) {
			netdev_entered =1;
#endif
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
	d_level = detail_level;

	handle_cmd_params(argc, argv);
	if (dev_smcd)
		gen_nl_handle(SMC_NETLINK_GET_DEV_SMCD, handle_gen_dev_reply);

	return 0;
}
