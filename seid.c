/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2020
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
#include "seid.h"

static int enable_cmd = 0;
static int disable_cmd = 0;
static int show_cmd = 0;

extern int smc_id;
extern struct nl_sock *sk;

const struct nla_policy
smc_gen_seid_policy[SMC_NLA_SEID_TABLE_MAX + 1] = {
	[SMC_NLA_SEID_UNSPEC]	= { .type = NLA_UNSPEC },
	[SMC_NLA_SEID_ENTRY]	= { .type = NLA_NUL_STRING },
	[SMC_NLA_SEID_ENABLED]	= { .type = NLA_U8 },
};

static void usage(void)
{
	fprintf(stderr,
		"Usage: smcd seid [show]\n"
		"       smcd seid enable\n"
		"       smcd seid disable\n"
	);
	exit(-1);
}

/* arg is an (int *) */
static int is_seid_defined_reply(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[SMC_GEN_MAX + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);
	int *is_seid = (int *)arg;

	if (genlmsg_parse(hdr, 0, attrs, SMC_NLA_SEID_TABLE_MAX,
			  (struct nla_policy *)smc_gen_seid_policy) < 0) {
		fprintf(stderr, "Error: Invalid data returned: smc_gen_seid_policy\n");
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}

	if (!attrs[SMC_NLA_SEID_ENTRY])
		*is_seid = 0;
	else
		*is_seid = 1;

	return NL_OK;
}

static int is_seid_defined(int *is_seid)
{
	*is_seid = 0;
	return gen_nl_handle_dump(SMC_NETLINK_DUMP_SEID, is_seid_defined_reply, is_seid);
}

static int handle_gen_seid_reply(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[SMC_NLA_SEID_TABLE_MAX + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);
	int rc = NL_OK;
	char *state;

	if (!show_cmd)
		return rc;

	if (genlmsg_parse(hdr, 0, attrs, SMC_NLA_SEID_TABLE_MAX,
			  (struct nla_policy *)smc_gen_seid_policy) < 0) {
		fprintf(stderr, "Error: invalid data returned: smc_gen_seid_policy\n");
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}

	if (!attrs[SMC_NLA_SEID_ENTRY] || !attrs[SMC_NLA_SEID_ENABLED]) {
		printf("n/a\n");
		return NL_STOP;
	}

	if (nla_get_u8(attrs[SMC_NLA_SEID_ENABLED]))
		state = "[enabled]";
	else
		state = "[disabled]";

	printf("%s %s\n", nla_get_string(attrs[SMC_NLA_SEID_ENTRY]), state);
	return rc;
}

int gen_nl_seid_handle(int cmd, char dump, int (*cb_handler)(struct nl_msg *msg, void *arg))
{
	int rc = EXIT_FAILURE, nlmsg_flags = 0;
	struct nl_msg *msg;

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM, cb_handler, NULL);

	/* Allocate a netlink message and set header information. */
	msg = nlmsg_alloc();
	if (!msg) {
		nl_perror(NLE_NOMEM, "Error");
		rc = EXIT_FAILURE;
		goto err;
	}

	if (dump)
		nlmsg_flags = NLM_F_DUMP;

	if (!genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, smc_id, 0, nlmsg_flags,
			 cmd, SMC_GENL_FAMILY_VERSION)) {
		nl_perror(rc, "Error");
		rc = EXIT_FAILURE;
		goto err;
	}

	/* Send message */
	rc = nl_send_auto(sk, msg);
	if (rc < 0) {
		nl_perror(rc, "Error");
		rc = EXIT_FAILURE;
		goto err;
	}

	/* Receive reply message, returns number of cb invocations. */
	rc = nl_recvmsgs_default(sk);

	if (rc < 0) {
		/* For cmd "SEID disable" the kernel might return ENOENT when
		 * no UEID is defined and the SEID cannot be disabled.
		 * This is mapped to NLE_OBJ_NOTFOUND in libnl, lib/error.c.
		 */
		if (rc == -NLE_OPNOTSUPP) {
			fprintf(stderr, "Error: operation not supported by kernel\n");
		} else if (cmd == SMC_NETLINK_DISABLE_SEID && rc == -NLE_OBJ_NOTFOUND) {
			fprintf(stderr, "Error: System EID cannot be disabled because no User EID is defined\n");
		} else {
			nl_perror(rc, "Error");
		}
		rc = EXIT_FAILURE;
		goto err;
	}

	nlmsg_free(msg);
	return EXIT_SUCCESS;
err:
	nlmsg_free(msg);
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
		} else if (contains(argv[0], "enable") == 0) {
			enable_cmd = 1;
		} else if (contains(argv[0], "disable") == 0) {
			disable_cmd = 1;
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

	/* Only single cmd expected */
	if ((enable_cmd + disable_cmd + show_cmd) != 1)
		usage();
}

int invoke_seid(int argc, char **argv, int detail_level)
{
	int rc = EXIT_SUCCESS;

	handle_cmd_params(argc, argv);

	if (enable_cmd || disable_cmd) {
		int is_seid = 0;

		is_seid_defined(&is_seid);
		if (!is_seid) {
			printf("Error: System EID not available\n");
			return EXIT_FAILURE;
		}
	}

	if (enable_cmd) {
		rc = gen_nl_seid_handle(SMC_NETLINK_ENABLE_SEID, 0, handle_gen_seid_reply);
	} else if (disable_cmd) {
		rc = gen_nl_seid_handle(SMC_NETLINK_DISABLE_SEID, 0, handle_gen_seid_reply);
	} else if (show_cmd) {
		rc = gen_nl_seid_handle(SMC_NETLINK_DUMP_SEID, 1, handle_gen_seid_reply);
	} else {
		printf("Error: unknown command\n"); /* we should never come here ... */
	}

	return rc;
}
