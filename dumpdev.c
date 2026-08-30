#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "smctools_common.h"
#include "util.h"
#include "libnetlink.h"
#include "dumpdev.h"

static int set_cmd = 0;
static int reset_cmd = 0;
static int show_cmd = 0;

static char target_ndev[SMC_MAX_DUMP_DEV_LEN + 1] = { 0 };

extern int smc_id;
extern struct nl_sock *sk;

const struct nla_policy
smc_gen_dump_ndev_policy[SMC_NLA_DUMP_DEV_MAX + 1] = {
	[SMC_NLA_DUMP_DEV_UNSPEC]	= { .type = NLA_UNSPEC },
	[SMC_NLA_DUMP_DEV_NAME]		= { .type = NLA_NUL_STRING },
};

static void usage(void)
{
	fprintf(stderr,
		"Usage: smc{r|d} dumpdev [show]\n"
		"       smc{r|d} dumpdev set <dev_name>\n"
		"       smc{r|d} dumpdev [reset]\n"
	);
	exit(-1);
}

static int gen_nl_dump_ndev_handle(int cmd, char *dev, int (*cb_handler)(struct nl_msg *msg, void *arg))
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

	if (cmd == SMC_NETLINK_GET_DUMP_DEV)
		nlmsg_flags = NLM_F_DUMP;

	if (!genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, smc_id, 0, nlmsg_flags,
			 cmd, SMC_GENL_FAMILY_VERSION)) {
		nl_perror(rc, "Error");
		rc = EXIT_FAILURE;
		goto err;
	}

	if (dev && dev[0]) {
		rc = nla_put_string(msg, SMC_NLA_DUMP_DEV_NAME, dev);
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
		if (rc == -NLE_OPNOTSUPP) {
			fprintf(stderr, "Error: operation not supported by kernel\n");
		} else if (cmd == SMC_NETLINK_SET_DUMP_DEV) {
			if (rc == -NLE_NODEV) {
				fprintf(stderr, "Error: specified dump device is not valid\n");
			} else if (rc == -NLE_NOMEM) {
				fprintf(stderr, "Error: lack memory to set dump device\n");
			} else if (rc == -NLE_INVAL) {
				fprintf(stderr, "Error: lack memory to set dump device\n");
			} else {
				fprintf(stderr, "Error: fail to set specified dump device: %d\n", rc);
			}
		} else if (cmd == SMC_NETLINK_RESET_DUMP_DEV) {
			if (rc == -NLE_INVAL) {
				fprintf(stderr, "Error: smc dump device was not set\n");
			} else {
				fprintf(stderr, "Error: resetting specified dump device fail: %d\n", rc);
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

static int handle_gen_dump_ndev_reply(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[SMC_NLA_DUMP_DEV_NAME + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);
	int rc = NL_OK;

	if (genlmsg_parse(hdr, 0, attrs, SMC_NLA_DUMP_DEV_NAME,
			  (struct nla_policy *)smc_gen_dump_ndev_policy) < 0) {
		fprintf(stderr, "Error: invalid data returned: smc_gen_dump_ndev_policy\n");
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}

	if (!attrs[SMC_NLA_DUMP_DEV_NAME])
		return NL_STOP;

	printf("%s\n", nla_get_string(attrs[SMC_NLA_DUMP_DEV_NAME]));
	return rc;
}

static void set_ndev(char *dev)
{
	if (strlen(dev) > SMC_MAX_DUMP_DEV_LEN) {
		fprintf(stderr, "Error: Invalid interface specified: interface name is longer than 16 characters\n");
		exit(-1);
	}
	strncpy(target_ndev, dev, SMC_MAX_DUMP_DEV_LEN);
}

static void handle_cmd_params(int argc, char **argv)
{
	if (argc == 0) {
		show_cmd = 1; /* no object given, so use the default "show" */
		return;
	}

	while (1) {
		if (set_cmd) {
			set_ndev(argv[0]);
			break;
		} else if (contains(argv[0], "help") == 0) {
			usage();
		} else if (contains(argv[0], "set") == 0) {
			set_cmd = 1;
		} else if (contains(argv[0], "reset") == 0) {
			reset_cmd = 1;
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
	if ((set_cmd + reset_cmd + show_cmd) != 1)
		usage();

	/* device name required for command */
	if (!target_ndev[0] && set_cmd)
		usage();
}

int invoke_dumpdev(int argc, char **argv, int detail_level)
{
	int rc = EXIT_SUCCESS;

	handle_cmd_params(argc, argv);

	if (set_cmd) {
		rc = gen_nl_dump_ndev_handle(SMC_NETLINK_SET_DUMP_DEV, target_ndev,
					     handle_gen_dump_ndev_reply);
	} else if (reset_cmd) {
		rc = gen_nl_dump_ndev_handle(SMC_NETLINK_RESET_DUMP_DEV, NULL,
					     handle_gen_dump_ndev_reply);
	} else if (show_cmd) {
		rc = gen_nl_dump_ndev_handle(SMC_NETLINK_GET_DUMP_DEV, NULL,
					     handle_gen_dump_ndev_reply);
	} else {
		printf("Error: Unknown command\n"); /* we should never come here ... */
	}

	return rc;
}

