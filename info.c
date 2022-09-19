/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2021
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
#include "info.h"
#include "dev.h"

static int show_cmd = 0;
static int ism_count, rocev1_count, rocev2_count, rocev3_count;

static struct nla_policy
smc_gen_info_policy[SMC_NLA_SYS_MAX + 1] = {
	[SMC_NLA_SYS_UNSPEC]	= { .type = NLA_UNSPEC },
	[SMC_NLA_SYS_VER]	= { .type = NLA_U8 },
	[SMC_NLA_SYS_REL]	= { .type = NLA_U8 },
	[SMC_NLA_SYS_IS_ISM_V2]	= { .type = NLA_U8 },
	[SMC_NLA_SYS_LOCAL_HOST]= { .type = NLA_NUL_STRING },
	[SMC_NLA_SYS_SEID]	= { .type = NLA_NUL_STRING },
	[SMC_NLA_SYS_IS_SMCR_V2]= { .type = NLA_U8 },
};

static void usage(void)
{
	fprintf(stderr,
#if defined(SMCD)
		"Usage: smcd info [show]\n"
#elif defined(SMCR)
		"Usage: smcr info [show]\n"
#else
		"Usage: smc info [show]\n"
#endif
	);
	exit(-1);
}

static int handle_gen_info_reply(struct nl_msg *msg, void *arg)
{
	struct nlattr *info_attrs[SMC_NLA_SYS_MAX + 1];
	struct nlattr *attrs[SMC_GEN_MAX + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);
	int rc = NL_OK;
	char tmp[80];
	int smc_version = 1;

	if (!show_cmd)
		return rc;

	if (genlmsg_parse(hdr, 0, attrs, SMC_GEN_MAX,
			  (struct nla_policy *)smc_gen_net_policy) < 0) {
		fprintf(stderr, "Error: Invalid data returned: smc_gen_net_policy\n");
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}

	if (!attrs[SMC_GEN_SYS_INFO])
		return rc;

	if (nla_parse_nested(info_attrs, SMC_NLA_DEV_MAX,
			     attrs[SMC_GEN_SYS_INFO],
			     smc_gen_info_policy)) {
		fprintf(stderr, "Error: Failed to parse nested attributes: smc_gen_info_policy\n");
		return NL_STOP;
	}

	printf("Kernel Capabilities\n");

	/* Version */
	tmp[0] = '\0';
	if (info_attrs[SMC_NLA_SYS_VER] && info_attrs[SMC_NLA_SYS_REL]) {
		sprintf(tmp, "%d.%d", nla_get_u8(info_attrs[SMC_NLA_SYS_VER]), nla_get_u8(info_attrs[SMC_NLA_SYS_REL]));
		smc_version = nla_get_u8(info_attrs[SMC_NLA_SYS_VER]);
	}
	printf("SMC Version:      %s\n", (tmp[0] != '\0' ? tmp : "n/a"));

	/* Hostname */
	tmp[0] = '\0';
	if (info_attrs[SMC_NLA_SYS_LOCAL_HOST]) {
		sprintf(tmp, "%s", nla_get_string(info_attrs[SMC_NLA_SYS_LOCAL_HOST]));
	}
	printf("SMC Hostname:     %s\n", (tmp[0] != '\0' ? tmp : "n/a"));

	/* SMC-D */
	sprintf(tmp, "%s", "v1");
	if (smc_version >= 2) {
		strcat(tmp, " v2");
	}
	printf("SMC-D Features:   %s\n", tmp);

	/* SMC-R */
	sprintf(tmp, "%s", "v1");
	if (info_attrs[SMC_NLA_SYS_IS_SMCR_V2] && nla_get_u8(info_attrs[SMC_NLA_SYS_IS_SMCR_V2])) {
		strcat(tmp, " v2");
	}
	printf("SMC-R Features:   %s\n", tmp);

	printf("\n");
	printf("Hardware Capabilities\n");

	/* SEID */
	tmp[0] = '\0';
	if (info_attrs[SMC_NLA_SYS_SEID]) {
		sprintf(tmp, "%s", nla_get_string(info_attrs[SMC_NLA_SYS_SEID]));
	}
	printf("SEID:             %s\n", (tmp[0] != '\0' ? tmp : "n/a"));

	/* ISM hardware */
	tmp[0] = '\0';
	if (ism_count) {
		/* Kernel found any ISM device */
		sprintf(tmp, "%s", "v1"); /* dev found, v1 is possible */
		if (info_attrs[SMC_NLA_SYS_IS_ISM_V2]) {
			if (nla_get_u8(info_attrs[SMC_NLA_SYS_IS_ISM_V2]))
				strcat(tmp, " v2");
		}
	}
	printf("ISM:              %s\n", (tmp[0] != '\0' ? tmp : "n/a"));

	/* RoCE hardware */
	tmp[0] = '\0';
	if (rocev1_count || rocev2_count || rocev3_count) {
		/* Kernel found any RoCE device */
		strcpy(tmp, "");
		if (rocev1_count || rocev2_count || rocev3_count)
			strcat(tmp, "v1 ");
		if (rocev2_count || rocev3_count)
			strcat(tmp, "v2");
	}
	printf("RoCE:             %s\n", (tmp[0] != '\0' ? tmp : "n/a"));

	return rc;
}

static void handle_cmd_params(int argc, char **argv)
{
	if (argc == 0) {
		show_cmd = 1; /* no object given, so use the default "show" */
		return;
	}

	while (1) {
		if (contains(argv[0], "help") == 0) {
			usage();
		} else if (contains(argv[0], "show") == 0) {
			show_cmd = 1;
			break;
		} else {
			usage();
		}
		if (!NEXT_ARG_OK())
			break;
		NEXT_ARG();
	}
	/* Too many parameters or wrong sequence of parameters */
	if (NEXT_ARG_OK())
		usage();
}

int invoke_info(int argc, char **argv, int detail_level)
{
	int rc = EXIT_SUCCESS;

	handle_cmd_params(argc, argv);

	if (show_cmd) {
		if (dev_count_ism_devices(&ism_count)) {
			fprintf(stderr, "Error: Failed to retrieve ISM device count\n");
			return EXIT_FAILURE;
		}
		if (dev_count_roce_devices(&rocev1_count, &rocev2_count, &rocev3_count)) {
			fprintf(stderr, "Error: Failed to retrieve RoCE device count\n");
			return EXIT_FAILURE;
		}

		rc = gen_nl_handle_dump(SMC_NETLINK_GET_SYS_INFO, handle_gen_info_reply, NULL);
	} else {
		printf("Error: Unknown command\n"); /* we should never come here ... */
		return EXIT_FAILURE;
	}

	return rc;
}
