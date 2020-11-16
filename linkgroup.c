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
#define SMC_INVALID_LINK_ID 0xFFFFFFFF

static __u32 unmasked_trgt_lgid = 0;
static int netdev_entered = 0;
static __u32 target_lgid = 0;
static int ibdev_entered = 0;
static int type_entered = 0;
static int all_entered = 0;
static int show_links = 0;
#if defined(SMCD)
static int lgr_smcr = 0;
static int lgr_smcd = 1;
#else
static int lgr_smcr = 1;
static int lgr_smcd = 0;
#endif
static int d_level = 0;

static char target_ibdev[IB_DEVICE_NAME_MAX] = {0};
static char target_type[SMC_TYPE_STR_MAX] = {0};
static char target_ndev[IFNAMSIZ] = {0};

static struct nla_policy smc_gen_lgr_smcr_sock_policy[SMC_NLA_LGR_R_MAX + 1] = {
	[SMC_NLA_LGR_R_UNSPEC]		= { .type = NLA_UNSPEC },
	[SMC_NLA_LGR_R_ID]		= { .type = NLA_U32 },
	[SMC_NLA_LGR_R_ROLE]		= { .type = NLA_U8 },
	[SMC_NLA_LGR_R_TYPE]		= { .type = NLA_U8 },
	[SMC_NLA_LGR_R_PNETID]		= { .type = NLA_NUL_STRING,
					    .maxlen = SMC_MAX_PNETID_LEN + 1 },
	[SMC_NLA_LGR_R_VLAN_ID]		= { .type = NLA_U8 },
	[SMC_NLA_LGR_R_CONNS_NUM]	= { .type = NLA_U32 },
};

static struct nla_policy smc_gen_lgr_smcd_sock_policy[SMC_NLA_LGR_D_MAX + 1] = {
	[SMC_NLA_LGR_D_UNSPEC]		= { .type = NLA_UNSPEC },
	[SMC_NLA_LGR_D_ID]		= { .type = NLA_U32 },
	[SMC_NLA_LGR_D_PNETID]		= { .type = NLA_NUL_STRING,
					    .maxlen = SMC_MAX_PNETID_LEN + 1 },
	[SMC_NLA_LGR_D_VLAN_ID]		= { .type = NLA_U8 },
	[SMC_NLA_LGR_D_CONNS_NUM]	= { .type = NLA_U32 },
};

static struct nla_policy smc_gen_link_smcr_sock_policy[SMC_NLA_LINK_MAX + 1] = {
	[SMC_NLA_LINK_UNSPEC]		= { .type = NLA_UNSPEC },
	[SMC_NLA_LINK_ID]		= { .type = NLA_U8 },
	[SMC_NLA_LINK_IB_DEV]		= { .type = NLA_NUL_STRING,
					    .maxlen = IB_DEVICE_NAME_MAX + 1 },
	[SMC_NLA_LINK_IB_PORT]		= { .type = NLA_U8 },
	[SMC_NLA_LINK_GID]		= { .type = NLA_NUL_STRING,
					    .maxlen = 40 + 1 },
	[SMC_NLA_LINK_PEER_GID]		= { .type = NLA_NUL_STRING,
					    .maxlen = 40 + 1 },
	[SMC_NLA_LINK_CONN_CNT]		= { .type = NLA_U32 },
	[SMC_NLA_LINK_NET_DEV]          = { .type = NLA_U32},
	[SMC_NLA_LINK_UID]		= { .type = NLA_U32 },
	[SMC_NLA_LINK_PEER_UID]		= { .type = NLA_U32 },
	[SMC_NLA_LINK_STATE]		= { .type = NLA_U32 },
};

static void usage(void)
{
	fprintf(stderr,
#if defined(SMCD)
		"Usage: smcd linkgroup [show] [all | LG-ID]\n"
#elif defined(SMCR)
		"Usage: smcr linkgroup [show | link-show] [all | LG-ID]\n"
		"                                         [ibdev <dev>]\n"
		"                                         [netdev <dev>]\n"
#else
		"Usage: smc linkgroup [show | link-show] [all | LG-ID] [type {smcd | smcr}]\n"
		"                                                      [ibdev <dev>]\n"
		"                                                      [netdev <dev>]\n"
#endif
	);
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
			printf("IB-Dev    ");
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

static int filter_smcd_item(struct smcd_diag_dmbinfo_v2 *lgr)
{
	int ignore = 0;

	if (unmasked_trgt_lgid == 0 )
		return ignore; /* No filter set */
	else if (unmasked_trgt_lgid != lgr->v1.linkid)
		ignore = 1;

	return ignore;
}

static int filter_smcr_item(struct smc_diag_linkinfo_v2 *link, struct smc_diag_lgr *lgr)
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
		if (strncmp(target_ibdev, (char*)link->v1.ibname, sizeof(target_ibdev)) == 0)
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

static int fill_link_struct(struct smc_diag_linkinfo_v2 *link, struct nlattr **attrs)
{
	struct nlattr *link_attrs[SMC_NLA_LINK_MAX + 1];
	__u32 temp_link_uid;

	if (nla_parse_nested(link_attrs, SMC_NLA_LINK_MAX,
			     attrs[SMC_GEN_LINK_SMCR],
			     smc_gen_link_smcr_sock_policy)) {
		fprintf(stderr, "failed to parse nested attributes: smc_gen_link_smcr_sock_policy\n");
		return NL_STOP;
	}
	if (link_attrs[SMC_NLA_LINK_STATE])
		link->link_state = nla_get_u32(link_attrs[SMC_NLA_LINK_STATE]);
	if (link_attrs[SMC_NLA_LINK_CONN_CNT])
		link->conn_cnt = nla_get_u32(link_attrs[SMC_NLA_LINK_CONN_CNT]);
	if (link_attrs[SMC_NLA_LINK_UID]) {
		temp_link_uid = nla_get_u32(link_attrs[SMC_NLA_LINK_UID]);
		memcpy(&link->link_uid[0], &temp_link_uid, sizeof(temp_link_uid));
	}
	if (link_attrs[SMC_NLA_LINK_IB_PORT])
		link->v1.ibport = nla_get_u8(link_attrs[SMC_NLA_LINK_IB_PORT]);
	if (link_attrs[SMC_NLA_LINK_PEER_UID]) {
		temp_link_uid = nla_get_u32(link_attrs[SMC_NLA_LINK_PEER_UID]);
		memcpy(&link->peer_link_uid[0], &temp_link_uid, sizeof(temp_link_uid));
	}
	if (link_attrs[SMC_NLA_LINK_NET_DEV] && nla_get_u32(link_attrs[SMC_NLA_LINK_NET_DEV]))
		if_indextoname(nla_get_u32(link_attrs[SMC_NLA_LINK_NET_DEV]), (char*)link->netdev);
	if (link_attrs[SMC_NLA_LINK_IB_DEV])
		snprintf((char*)link->v1.ibname, sizeof(link->v1.ibname), "%s",
			 nla_get_string(link_attrs[SMC_NLA_LINK_IB_DEV]));
	if (link_attrs[SMC_NLA_LINK_GID])
		snprintf((char*)link->v1.gid, sizeof(link->v1.gid), "%s",
			 nla_get_string(link_attrs[SMC_NLA_LINK_GID]));
	if (link_attrs[SMC_NLA_LINK_PEER_GID])
		snprintf((char*)link->v1.peer_gid, sizeof(link->v1.peer_gid), "%s",
			 nla_get_string(link_attrs[SMC_NLA_LINK_PEER_GID]));
	return NL_OK;
}

static int fill_lgr_struct(struct smc_diag_lgr *lgr, struct nlattr **attrs)
{
	struct nlattr *lgr_attrs[SMC_NLA_LGR_R_MAX + 1];

	if (nla_parse_nested(lgr_attrs, SMC_NLA_LGR_R_MAX,
			     attrs[SMC_GEN_LGR_SMCR],
			     smc_gen_lgr_smcr_sock_policy)) {
		fprintf(stderr, "failed to parse nested attributes: smc_gen_lgr_smcr_sock_policy\n");
		return NL_STOP;
	}
	if (lgr_attrs[SMC_NLA_LGR_R_ID])
		*(__u32*)lgr->lgr_id = nla_get_u32(lgr_attrs[SMC_NLA_LGR_R_ID]);
	if (lgr_attrs[SMC_NLA_LGR_R_ROLE])
		lgr->lgr_role = nla_get_u8(lgr_attrs[SMC_NLA_LGR_R_ROLE]);
	if (lgr_attrs[SMC_NLA_LGR_R_TYPE])
		lgr->lgr_type = nla_get_u8(lgr_attrs[SMC_NLA_LGR_R_TYPE]);
	if (lgr_attrs[SMC_NLA_LGR_R_VLAN_ID])
		lgr->vlan_id = nla_get_u8(lgr_attrs[SMC_NLA_LGR_R_VLAN_ID]);
	if (lgr_attrs[SMC_NLA_LGR_R_CONNS_NUM])
		lgr->conns_num = nla_get_u32(lgr_attrs[SMC_NLA_LGR_R_CONNS_NUM]);
	if (lgr_attrs[SMC_NLA_LGR_R_PNETID])
		snprintf((char*)lgr->pnet_id, sizeof(lgr->pnet_id), "%s",
			 nla_get_string(lgr_attrs[SMC_NLA_LGR_R_PNETID]));
	return NL_OK;
}

static int fill_lgr_smcd_struct(struct smcd_diag_dmbinfo_v2 *lgr, struct nlattr **attrs)
{
	struct nlattr *lgr_attrs[SMC_NLA_LGR_D_MAX + 1];

	if (nla_parse_nested(lgr_attrs, SMC_NLA_LGR_D_MAX,
			     attrs[SMC_GEN_LGR_SMCD],
			     smc_gen_lgr_smcd_sock_policy)) {
		fprintf(stderr, "failed to parse nested attributes: smc_gen_lgr_smcd_sock_policy\n");
		return NL_STOP;
	}
	if (lgr_attrs[SMC_NLA_LGR_D_ID])
		lgr->v1.linkid = nla_get_u32(lgr_attrs[SMC_NLA_LGR_D_ID]);
	if (lgr_attrs[SMC_NLA_LGR_D_VLAN_ID])
		lgr->vlan_id = nla_get_u8(lgr_attrs[SMC_NLA_LGR_D_VLAN_ID]);
	if (lgr_attrs[SMC_NLA_LGR_D_CONNS_NUM])
		lgr->conns_num = nla_get_u32(lgr_attrs[SMC_NLA_LGR_D_CONNS_NUM]);
	if (lgr_attrs[SMC_NLA_LGR_D_PNETID])
		snprintf((char*)lgr->pnet_id, sizeof(lgr->pnet_id), "%s",
			 nla_get_string(lgr_attrs[SMC_NLA_LGR_D_PNETID]));
	return NL_OK;
}

static int show_lgr_smcr_info(struct nlattr **attr)
{
	static struct smc_diag_lgr lgr = {0};
	struct smc_diag_linkinfo_v2 link = {0};
	int rc = NL_OK;

	if (attr[SMC_GEN_LGR_SMCR])
		rc = fill_lgr_struct(&lgr, attr);

	if (attr[SMC_GEN_LINK_SMCR])
		rc = fill_link_struct(&link, attr);

	if (attr[SMC_GEN_LGR_SMCR] && show_links)
		return rc;

	if (filter_smcr_item(&link, &lgr))
		return rc;

	printf("%08x ", *(__u32*)lgr.lgr_id);
	printf("%-8s ", lgr.lgr_role ? "SERV" : "CLNT");
	printf("%-8s ", smc_lgr_type(lgr.lgr_type));
	if (show_links) {
		if (strnlen((char*)link.netdev, sizeof(link.netdev)) > (IFNAMSIZ - 1))
			printf("%-.15s ", link.netdev);
		else
			printf("%-15s ", link.netdev);
		printf("%-15s ", smc_link_state(link.link_state));
		printf("%6d  ", link.conn_cnt);
		if (d_level >= SMC_DETAIL_LEVEL_V) {
			printf("%08x  ",  ntohl(*(__u32*)link.link_uid));
			printf("%08x  ", ntohl(*(__u32*)link.peer_link_uid));
			if (strnlen((char*)link.v1.ibname, sizeof(link.v1.ibname)) > SMC_MAX_IBNAME)
				printf("%-.8s  ", link.v1.ibname);
			else
				printf("%-8s  ", link.v1.ibname);
			printf("%4d  ", link.v1.ibport);
			if (d_level >= SMC_DETAIL_LEVEL_VV) {
				printf("%-40s  ", link.v1.gid);
				printf("%s  ", link.v1.peer_gid);
			}
		}
	} else {
		printf("%#4x ", lgr.vlan_id);
		printf(" %6d ", lgr.conns_num);
		printf(" %-16s ", trim_space((char *)lgr.pnet_id));
	}
	printf("\n");

	return rc;
}

static int show_lgr_smcd_info(struct nlattr **attr)
{
	struct smcd_diag_dmbinfo_v2 lgr;
	int rc = NL_OK;

	if (attr[SMC_GEN_LGR_SMCD])
		rc = fill_lgr_smcd_struct(&lgr, attr);
	if(filter_smcd_item(&lgr))
		return rc;
	printf("%08x ", lgr.v1.linkid);
	printf("%#4x  ", lgr.vlan_id);
	printf("%6d  ", lgr.conns_num);
	printf("%-16s ", trim_space((char *)lgr.pnet_id));
	printf("\n");
	return rc;
}

static int handle_gen_lgr_reply(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[SMC_GEN_MAX + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);
	static int header_printed = 0;
	int rc = NL_OK;

	if (!header_printed) {
		if (lgr_smcd)
			print_lgr_smcd_header();
		else
			print_lgr_smcr_header();
		header_printed = 1;
	}

	if (genlmsg_parse(hdr, 0, attrs, SMC_GEN_MAX,
			  (struct nla_policy *)smc_gen_net_policy) < 0) {
		fprintf(stderr, "invalid data returned: smc_gen_net_policy\n");
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}
	if (!attrs[SMC_GEN_LGR_SMCR] && !attrs[SMC_GEN_LINK_SMCR] && !attrs[SMC_GEN_LGR_SMCD])
		return NL_STOP;

	if (lgr_smcr && (attrs[SMC_GEN_LGR_SMCR] || attrs[SMC_GEN_LINK_SMCR]))
		rc = show_lgr_smcr_info(&attrs[0]);

	if (lgr_smcd && attrs[SMC_GEN_LGR_SMCD])
		rc = show_lgr_smcd_info(&attrs[0]);
	return rc;
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
		} else if (!all_entered){
			char *endptr = NULL;

			unmasked_trgt_lgid = (unsigned int)strtol(argv[0], &endptr, 16);
			if (argv[0] == endptr) /* string doesn't contain any digits */
				unmasked_trgt_lgid = SMC_INVALID_LINK_ID;
			else
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
	d_level = detail_level;

	handle_cmd_params(argc, argv);

	if (lgr_smcd)
		gen_nl_handle(SMC_NETLINK_GET_LGR_SMCD, handle_gen_lgr_reply);
	else if (show_links)
		gen_nl_handle(SMC_NETLINK_GET_LINK_SMCR, handle_gen_lgr_reply);
	else
		gen_nl_handle(SMC_NETLINK_GET_LGR_SMCR, handle_gen_lgr_reply);

	return 0;
}
