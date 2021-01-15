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
#include "ueid.h"

static int add_cmd = 0;
static int del_cmd = 0;
static int flush_cmd = 0;
static int show_cmd = 0;

static char target_eid[SMC_MAX_EID_LEN + 1] = {0};

extern int smc_id;
extern struct nl_sock *sk;

const struct nla_policy
smc_gen_ueid_policy[SMC_NLA_EID_TABLE_MAX + 1] = {
	[SMC_NLA_EID_TABLE_UNSPEC]	= { .type = NLA_UNSPEC },
	[SMC_NLA_EID_TABLE_ENTRY]	= { .type = NLA_NUL_STRING },
};

static void usage(void)
{
	fprintf(stderr,
		"Usage: smcd ueid [show]\n"
		"       smcd ueid add <eid>\n"
		"       smcd ueid del <eid>\n"
		"       smcd ueid flush\n"
	);
	exit(-1);
}

static int gen_nl_ueid_handle(int cmd, char *ueid, int (*cb_handler)(struct nl_msg *msg, void *arg))
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

	if (cmd == SMC_NETLINK_DUMP_UEID)
		nlmsg_flags = NLM_F_DUMP;

	if (!genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, smc_id, 0, nlmsg_flags,
			 cmd, SMC_GENL_FAMILY_VERSION)) {
		nl_perror(rc, "Error");
		rc = EXIT_FAILURE;
		goto err;
	}

	if (ueid && ueid[0]) {
		rc = nla_put_string(msg, SMC_NLA_EID_TABLE_ENTRY, ueid);
		if (rc < 0) {
			nl_perror(rc, "Error");
			rc = EXIT_FAILURE;
			goto err;
		}

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
		/* For cmd "UEID remove" the kernel might return ENOENT when
		 * the specified UEID is not in the list.
		 * This is mapped to NLE_OBJ_NOTFOUND in libnl, lib/error.c.
		 */
		if (rc == -NLE_OPNOTSUPP) {
			fprintf(stderr, "Error: operation not supported by kernel\n");
		} else if (cmd == SMC_NETLINK_REMOVE_UEID) {
			if (rc == -NLE_OBJ_NOTFOUND) {
				fprintf(stderr, "Error: specified User EID is not defined\n");
			} else if (rc == -NLE_AGAIN) {
				fprintf(stderr, "Info: the System EID was activated because the last User EID was removed\n");
			} else {
				fprintf(stderr, "Error: specified User EID is not defined\n");
			}
		} else if (cmd == SMC_NETLINK_ADD_UEID) {
			if (rc == -NLE_INVAL) {
				fprintf(stderr, "Error: specified User EID was rejected by the kernel\n");
			} else if (rc == -NLE_NOMEM) {
				fprintf(stderr, "Error: kernel reported an out of memory condition\n");
			} else if (rc == -NLE_RANGE) {
				fprintf(stderr, "Error: specified User EID was rejected because the maximum number of User EIDs is reached\n");
			} else if (rc == -NLE_EXIST) {
				fprintf(stderr, "Error: specified User EID is already defined\n");
			} else {
				nl_perror(rc, "Error");
			}
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

static int handle_gen_ueid_reply(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[SMC_NLA_EID_TABLE_ENTRY + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);
	int rc = NL_OK;

	if (genlmsg_parse(hdr, 0, attrs, SMC_NLA_EID_TABLE_ENTRY,
			  (struct nla_policy *)smc_gen_ueid_policy) < 0) {
		fprintf(stderr, "Error: invalid data returned: smc_gen_ueid_policy\n");
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}

	if (!attrs[SMC_NLA_EID_TABLE_ENTRY])
		return NL_STOP;

	printf("%s\n", nla_get_string(attrs[SMC_NLA_EID_TABLE_ENTRY]));
	return rc;
}

static char ueid_valid(char *ueid)
{
	char *end = ueid + SMC_MAX_EID_LEN;

	while (--end >= ueid && isspace(*end))
		;
	if (end < ueid) {
		fprintf(stderr, "Error: Invalid User EID specified: EID is empty\n");
		return 0;
	}
	if (!isalnum(*ueid)) {
		fprintf(stderr, "Error: Invalid User EID specified: first character must be alphanumeric\n");
		return 0;
	}
	if (strstr(ueid, "..")) {
		fprintf(stderr, "Error: Invalid User EID specified: consecutive dots not allowed\n");
		return 0;
	}
	while (ueid <= end) {
		if ((!isalnum(*ueid) || islower(*ueid)) && *ueid != '.' && *ueid != '-') {
			fprintf(stderr, "Error: Invalid User EID specified: unsupported character: '%c'\n", *ueid);
			fprintf(stderr, "       Supported characters are: A-Z, 0-9, '.' and '-'\n");
			return 0;
		}
		ueid++;
	}
	return 1;
}

static void set_eid(char *eid)
{
	if (strlen(eid) > SMC_MAX_EID_LEN) {
		fprintf(stderr, "Error: Invalid User EID specified: EID is longer than 32 characters\n");
		exit(-1);
	}
	/* pad to 32 byte using blanks */
	sprintf(target_eid, "%-32s", eid);

	if (!ueid_valid(target_eid))
		exit(-1);
}

static void handle_cmd_params(int argc, char **argv)
{
	if (argc == 0) {
		show_cmd = 1; /* no object given, so use the default "show" */
		return;
	}

	while (1) {
		if (add_cmd) {
			set_eid(argv[0]);
			break;
		} else if (del_cmd) {
			set_eid(argv[0]);
			break;
		} else if (contains(argv[0], "help") == 0) {
			usage();
		} else if (contains(argv[0], "add") == 0) {
			add_cmd = 1;
		} else if (contains(argv[0], "del") == 0) {
			del_cmd = 1;
		} else if (contains(argv[0], "flush") == 0) {
			flush_cmd = 1;
			break;
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
	if ((add_cmd + del_cmd + flush_cmd + show_cmd) != 1)
		usage();

	/* eid required for command */
	if (!target_eid[0] && (add_cmd || del_cmd))
		usage();
}

int invoke_ueid(int argc, char **argv, int detail_level)
{
	int rc = EXIT_SUCCESS;

	handle_cmd_params(argc, argv);

	if (add_cmd) {
		rc = gen_nl_ueid_handle(SMC_NETLINK_ADD_UEID, target_eid, handle_gen_ueid_reply);
	} else if (del_cmd) {
		rc = gen_nl_ueid_handle(SMC_NETLINK_REMOVE_UEID, target_eid, handle_gen_ueid_reply);
	} else if (flush_cmd) {
		rc = gen_nl_ueid_handle(SMC_NETLINK_FLUSH_UEID, NULL, handle_gen_ueid_reply);
	} else if (show_cmd) {
		rc = gen_nl_ueid_handle(SMC_NETLINK_DUMP_UEID, NULL, handle_gen_ueid_reply);
	} else {
		printf("Error: Unknown command\n"); /* we should never come here ... */
	}

	return rc;
}
