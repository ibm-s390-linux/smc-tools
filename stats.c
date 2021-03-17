/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2021
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
#include "stats.h"

#if defined(SMCD)
static int is_smcd = 1;
#else
static int is_smcd = 0;
#endif
static int netdev_entered = 0;
static int ibdev_entered = 0;
static int type_entered = 0;
static int dev_smcr = 1;
static int dev_smcd = 0;
static int d_level = 0;

static char target_ibdev[IB_DEVICE_NAME_MAX] = {0};
static char target_type[SMC_TYPE_STR_MAX] = {0};
static char target_ndev[IFNAMSIZ] = {0};

static struct nla_policy smc_gen_stats_policy[SMC_NLA_STATS_MAX + 1] = {
	[SMC_NLA_STATS_PAD]		= { .type = NLA_UNSPEC },
	[SMC_NLA_STATS_SMCD_TECH]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_SMCR_TECH]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_CLNT_HS_ERR_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_SRV_HS_ERR_CNT]	= { .type = NLA_U64 },
};

static struct nla_policy smc_gen_stats_fback_policy[SMC_NLA_FBACK_STATS_MAX + 1] = {
	[SMC_NLA_FBACK_STATS_PAD]	= { .type = NLA_UNSPEC },
	[SMC_NLA_FBACK_STATS_TYPE]	= { .type = NLA_U8 },
	[SMC_NLA_FBACK_STATS_SRV_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_FBACK_STATS_CLNT_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_FBACK_STATS_RSN_CODE]	= { .type = NLA_U32 },
	[SMC_NLA_FBACK_STATS_RSN_CNT]	= { .type = NLA_U16 },
};

static struct nla_policy smc_gen_stats_tech_policy[SMC_NLA_STATS_T_MAX + 1] = {
	[SMC_NLA_STATS_T_PAD]		= { .type = NLA_UNSPEC },
	[SMC_NLA_STATS_T_TX_RMB_SIZE]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_T_RX_RMB_SIZE]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_T_TXPLOAD_SIZE]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_T_RXPLOAD_SIZE]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_T_CLNT_V1_SUCC]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_CLNT_V2_SUCC]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_SRV_V1_SUCC]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_SRV_V2_SUCC]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_SENDPAGE_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_SPLICE_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_CORK_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_URG_DATA_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_NDLY_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_RX_BYTES]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_TX_BYTES]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_RX_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_TX_CNT]	= { .type = NLA_U64 },
};

static struct nla_policy smc_gen_stats_rmb_policy[SMC_NLA_STATS_RMB_MAX + 1] = {
	[SMC_NLA_STATS_RMB_PAD]			= { .type = NLA_UNSPEC },
	[SMC_NLA_STATS_RMB_SIZE_SM_PEER_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_SIZE_SM_CNT]		= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_FULL_PEER_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_FULL_CNT]		= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_REUSE_CNT]		= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_ALLOC_CNT]		= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_DGRADE_CNT]		= { .type = NLA_U64 },
};

static struct nla_policy smc_gen_stats_pload_policy[SMC_NLA_STATS_PLOAD_MAX + 1] = {
	[SMC_NLA_STATS_PLOAD_PAD]	= { .type = NLA_UNSPEC },
	[SMC_NLA_STATS_PLOAD_8K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_16K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_32K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_64K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_128K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_256K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_512K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_1024K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_G_1024K]	= { .type = NLA_U64 },
};

static void usage(void)
{
	fprintf(stderr,
		"Usage: smc device [show] [all] [type {smcd | smcr}]\n"
		"                               [ibdev <dev>]\n"
		"                               [netdev <dev>]\n");
	exit(-1);
}

static int show_tech_pload_info(struct nlattr **attr, int type, int direction)
{
	struct nlattr *tech_pload_attrs[SMC_NLA_STATS_PLOAD_MAX + 1];
	char *str_type, *str_dir;
	uint64_t trgt = 0;
	int rc = NL_OK;

	if (type == SMC_NLA_STATS_SMCD_TECH)
		str_type = "smcd";
	else
		str_type = "smcr";

	if (direction == SMC_NLA_STATS_T_TXPLOAD_SIZE)
		str_dir = "Reqs Tx";
	else if (direction == SMC_NLA_STATS_T_RXPLOAD_SIZE)
		str_dir = "Reqs Rx";
	else if (direction == SMC_NLA_STATS_T_TX_RMB_STATS)
		str_dir = "Bufs Tx";
	else if (direction == SMC_NLA_STATS_T_RX_RMB_STATS)
		str_dir = "Bufs Rx";
	else
		return NL_STOP;

	if (nla_parse_nested(tech_pload_attrs, SMC_NLA_STATS_PLOAD_MAX,
			     attr[direction],
			     smc_gen_stats_pload_policy)) {
		fprintf(stderr, "Error: Failed to parse nested attributes: smc_gen_stats_pload_policy\n");
		return NL_STOP;
	}

	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_8K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_8K]);
		printf("  %s %s  8K count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_16K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_16K]);
		printf("  %s %s  16K count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_32K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_32K]);
		printf("  %s %s  32K count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_64K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_64K]);
		printf("  %s %s  64K count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_128K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_128K]);
		printf("  %s %s  128K count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_256K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_256K]);
		printf("  %s %s  256K count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_512K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_512K]);
		printf("  %s %s  512K count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_1024K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_1024K]);
		printf("  %s %s  1024K count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_G_1024K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_G_1024K]);
		printf("  %s %s  >1024K count: %lu\n", str_type, str_dir, trgt);
	}
	return rc;
}

static int show_tech_rmb_info(struct nlattr **attr, int type, int direction)
{
	struct nlattr *tech_rmb_attrs[SMC_NLA_STATS_RMB_MAX + 1];
	char *str_type, *str_dir;
	uint64_t trgt = 0;
	int rc = NL_OK;

	if (type == SMC_NLA_STATS_SMCD_TECH)
		str_type = "smcd";
	else
		str_type = "smcr";

	if (direction == SMC_NLA_STATS_T_TX_RMB_STATS)
		str_dir = "Tx";
	else
		str_dir = "Rx";

	if (nla_parse_nested(tech_rmb_attrs, SMC_NLA_STATS_RMB_MAX,
			     attr[direction],
			     smc_gen_stats_rmb_policy)) {
		fprintf(stderr, "Error: Failed to parse nested attributes: smc_gen_stats_rmb_policy\n");
		return NL_STOP;
	}

	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_REUSE_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_REUSE_CNT]);
		printf("  %s %s rmb reuse count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_ALLOC_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_ALLOC_CNT]);
		printf("  %s %s rmb alloc count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_SIZE_SM_PEER_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_SIZE_SM_PEER_CNT]);
		printf("  %s %s peer buffer too small count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_SIZE_SM_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_SIZE_SM_CNT]);
		printf("  %s %s buffer too small count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_FULL_PEER_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_FULL_PEER_CNT]);
		printf("  %s %s peer buffer full count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_FULL_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_FULL_CNT]);
		printf("  %s %s buffer full count: %lu\n", str_type, str_dir, trgt);
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_DGRADE_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_DGRADE_CNT]);
		printf("  %s %s rmb downgrade count: %lu\n", str_type, str_dir, trgt);
	}

	return rc;
}

static int show_tech_info(struct nlattr **attr, int type)
{
	struct nlattr *tech_attrs[SMC_NLA_STATS_T_MAX + 1];
	uint64_t trgt = 0;
	char *str_type;
	int rc = NL_OK;

	if (type == SMC_NLA_STATS_SMCD_TECH) {
		printf("SMCD \n");
		str_type = "smcd";
	} else {
		printf("SMCR \n");
		str_type = "smcr";
	}

	if (nla_parse_nested(tech_attrs, SMC_NLA_STATS_T_MAX,
			     attr[type],
			     smc_gen_stats_tech_policy)) {
		fprintf(stderr, "Error: Failed to parse nested attributes: smc_gen_stats_fback_policy\n");
		return NL_STOP;
	}

	if (tech_attrs[SMC_NLA_STATS_T_SRV_V1_SUCC]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_SRV_V1_SUCC]);
		printf("  %s srv v1 success count: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_SRV_V2_SUCC]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_SRV_V2_SUCC]);
		printf("  %s srv v1 success count: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_CLNT_V1_SUCC]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_CLNT_V1_SUCC]);
		printf("  %s clnt v1 success count: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_CLNT_V2_SUCC]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_CLNT_V2_SUCC]);
		printf("  %s clnt v1 success count: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_RX_BYTES]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_RX_BYTES]);
		printf("  %s Rx bytes: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_TX_BYTES]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_TX_BYTES]);
		printf("  %s Tx bytes: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_RX_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_RX_CNT]);
		printf("  %s Rx count: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_TX_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_TX_CNT]);
		printf("  %s Tx count: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_SENDPAGE_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_SENDPAGE_CNT]);
		printf("  %s sendpage count: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_SPLICE_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_SPLICE_CNT]);
		printf("  %s splice count: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_CORK_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_CORK_CNT]);
		printf("  %s cork count: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_NDLY_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_NDLY_CNT]);
		printf("  %s no delay count: %lu\n", str_type, trgt);
	}
	if (tech_attrs[SMC_NLA_STATS_T_URG_DATA_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_URG_DATA_CNT]);
		printf("  %s urgent data count: %lu\n", str_type, trgt);
	}

	if (show_tech_rmb_info(tech_attrs, type, SMC_NLA_STATS_T_TX_RMB_STATS) != NL_OK)
		goto errout;
	if (show_tech_rmb_info(tech_attrs, type, SMC_NLA_STATS_T_RX_RMB_STATS) != NL_OK)
		goto errout;
	if (show_tech_pload_info(tech_attrs, type, SMC_NLA_STATS_T_TXPLOAD_SIZE) != NL_OK)
		goto errout;
	if (show_tech_pload_info(tech_attrs, type, SMC_NLA_STATS_T_RXPLOAD_SIZE) != NL_OK)
		goto errout;
	if (show_tech_pload_info(tech_attrs, type, SMC_NLA_STATS_T_TX_RMB_STATS) != NL_OK)
		goto errout;
	if (show_tech_pload_info(tech_attrs, type, SMC_NLA_STATS_T_RX_RMB_STATS) != NL_OK)
		goto errout;

errout:
	return rc;
}

static int handle_gen_stats_reply(struct nl_msg *msg, void *arg)
{
	struct nlattr *stats_attrs[SMC_NLA_STATS_MAX + 1];
	struct nlattr *attrs[SMC_GEN_MAX + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);
	uint64_t test = 0;
	int rc = NL_OK;

	if (genlmsg_parse(hdr, 0, attrs, SMC_GEN_MAX,
			  (struct nla_policy *)smc_gen_net_policy) < 0) {
		fprintf(stderr, "%s: invalid data returned\n", "smc");
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}
	if (!attrs[SMC_GEN_STATS])
		return NL_STOP;

	if (nla_parse_nested(stats_attrs, SMC_NLA_STATS_MAX,
			     attrs[SMC_GEN_STATS],
			     smc_gen_stats_policy)) {
		fprintf(stderr, "failed to parse nested attributes!\n");
		return NL_STOP;
	}
	if (stats_attrs[SMC_NLA_STATS_CLNT_HS_ERR_CNT]) {
		test = nla_get_u64(stats_attrs[SMC_NLA_STATS_CLNT_HS_ERR_CNT]);
		printf("Handshake clnt err count: %lu\n", test);
	}

	if (stats_attrs[SMC_NLA_STATS_SRV_HS_ERR_CNT]) {
		test = nla_get_u64(stats_attrs[SMC_NLA_STATS_SRV_HS_ERR_CNT]);
		printf("Handshake srv err count: %lu\n", test);
	}

	if (stats_attrs[SMC_NLA_STATS_SMCR_TECH] && !is_smcd)
		rc = show_tech_info(&stats_attrs[0], SMC_NLA_STATS_SMCR_TECH);
	if (stats_attrs[SMC_NLA_STATS_SMCD_TECH] && is_smcd)
		rc = show_tech_info(&stats_attrs[0], SMC_NLA_STATS_SMCD_TECH);
	return rc;
}

static int handle_gen_fback_stats_reply(struct nl_msg *msg, void *arg)
{
	struct nlattr *stats_fback_attrs[SMC_NLA_FBACK_STATS_MAX + 1];
	struct nlattr *attrs[SMC_GEN_MAX + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);
	unsigned short type = 0;
	int trgt = 0, trgt2 = 0;
	uint64_t trgt64 = 0;
	int rc = NL_OK;

	if (genlmsg_parse(hdr, 0, attrs, SMC_GEN_MAX,
			  (struct nla_policy *)smc_gen_net_policy) < 0) {
		fprintf(stderr, "%s: invalid data returned\n", "smc");
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}
	if (!attrs[SMC_GEN_FBACK_STATS])
		return NL_STOP;

	if (nla_parse_nested(stats_fback_attrs, SMC_NLA_FBACK_STATS_MAX,
			     attrs[SMC_GEN_FBACK_STATS],
			     smc_gen_stats_fback_policy)) {
		fprintf(stderr, "failed to parse nested attributes!\n");
		return NL_STOP;
	}

	if (stats_fback_attrs[SMC_NLA_FBACK_STATS_SRV_CNT]) {
		trgt64 = nla_get_u64(stats_fback_attrs[SMC_NLA_FBACK_STATS_SRV_CNT]);
		printf("Fallback server count: %lu \n", trgt64);
	}
	if (stats_fback_attrs[SMC_NLA_FBACK_STATS_SRV_CNT]) {
		trgt64 = nla_get_u64(stats_fback_attrs[SMC_NLA_FBACK_STATS_CLNT_CNT]);
		printf("Fallback client count: %lu \n", trgt64);
	}
	if (stats_fback_attrs[SMC_NLA_FBACK_STATS_TYPE]) {
		type = nla_get_u8(stats_fback_attrs[SMC_NLA_FBACK_STATS_TYPE]);
		if (type)
			printf("Server ");
		else
			printf("Client ");
	}

	if (stats_fback_attrs[SMC_NLA_FBACK_STATS_RSN_CODE])
		trgt = nla_get_u32(stats_fback_attrs[SMC_NLA_FBACK_STATS_RSN_CODE]);

	if (stats_fback_attrs[SMC_NLA_FBACK_STATS_RSN_CNT]) {
		trgt2 = nla_get_u16(stats_fback_attrs[SMC_NLA_FBACK_STATS_RSN_CNT]);
		printf("Fallback error code %x count: %d\n", trgt, trgt2);
	}
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
		} else {
			usage();
		}
	}
	/* Too many parameters or wrong sequence of parameters */
	if (NEXT_ARG_OK())
		usage();
}

int invoke_stats(int argc, char **argv, int detail_level)
{
	d_level = detail_level;

	handle_cmd_params(argc, argv);
	if (gen_nl_handle_dump(SMC_NETLINK_GET_FBACK_STATS, handle_gen_fback_stats_reply, NULL))
		return 0;
	gen_nl_handle_dump(SMC_NETLINK_GET_STATS, handle_gen_stats_reply, NULL);

	return 0;
}
