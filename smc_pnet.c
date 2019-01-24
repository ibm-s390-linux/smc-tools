/*
 * Shared Memory Communications over RDMA (SMC-R) and RoCE
 *
 * Copyright IBM Corp. 2017
 *
 * Author(s):  Thomas Richter <tmricht@linux.ibm.com>
 *
 * User space program for SMC-R PNET Table manipulation with generic netlink.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink/msg.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

#include "smctools_common.h"
#include "smc.h"

static char *progname;

static struct pnetentry {
	char *pnetid;		/* Pnetid */
	char *ethname;		/* Ethernet device name */
	char *ibname;		/* Infiniband/ISM device name */
	int ibport;		/* Infiniband device port number */
	unsigned char cmd;	/* Command to execute */
} pnetcmd;

static void _usage(FILE *dest)
{
	fprintf(dest,
"Usage: %s [ OPTIONS ] [pnetid]\n"
"\t-h, --help            this message\n"
"\t-V, --version         show version information\n"
"\t-a, --add             add a pnetid entry, requires interface or ib/ism device\n"
"\t-d, --delete          delete a pnetid entry\n"
"\t-s, --show            show a pnetid entry\n"
"\t-f, --flush           flush the complete pnet table\n"
"\t-I, --interface       Ethernet interface name of a pnetid entry\n"
"\t-D, --ibdevice        Infiniband device name of a pnetid entry\n"
"\t-P, --ibport          Infiniband device port (default: 1)\n"
"\t\n"
"\tno OPTIONS            show complete pnet table\n",
		progname);
}

static void help(void) __attribute__((noreturn));
static void help(void)
{
	_usage(stdout);
	exit(EXIT_SUCCESS);
}

static void usage(void) __attribute__((noreturn));
static void usage(void)
{
	_usage(stderr);
	exit(EXIT_FAILURE);
}

static int convert(char *string)
{
	unsigned long no;
	char *endp;

	no = strtoul(string, &endp, 0);
	if (*endp != '\0' || no > 2) {
		fprintf(stderr, "%s invalid ib port:%s\n", progname, string);
		usage();
	}
	return no;
}

static const struct option long_opts[] = {
	{ "interface", 1, 0, 'I' },
	{ "ibdevice", 1, 0, 'D' },
	{ "ibport", 1, 0, 'P' },
	{ "flush", 0, 0, 'f' },
	{ "add", 0, 0, 'a'},
	{ "show", 0, 0, 's'},
	{ "delete", 0, 0, 'd'},
	{ "version", 0, 0, 'V' },
	{ "help", 0, 0, 'h' },
	{ NULL, 0, NULL, 0}
};

static struct nla_policy smc_pnet_policy[SMC_PNETID_MAX + 1] = {
	[SMC_PNETID_NAME] = {
				.type = NLA_STRING,
				.maxlen = 17
			    },
	[SMC_PNETID_ETHNAME] = {
				.type = NLA_STRING,
				.maxlen = 16
			    },
	[SMC_PNETID_IBNAME] = {
				.type = NLA_STRING,
				.maxlen = 64
			    },
	[SMC_PNETID_IBPORT] = {
				.type = NLA_U8,
				.maxlen = 1
			    }
};


/* Netlink library call back handler to be called on data reception. */
static int cb_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[SMC_PNETID_MAX + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);

	if (genlmsg_validate(hdr, 0, SMC_PNETID_MAX, smc_pnet_policy) ||
	   genlmsg_parse(hdr, 0, attrs, SMC_PNETID_MAX, smc_pnet_policy) < 0) {
		fprintf(stderr, "%s: invalid data returned\n", progname);
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}
	printf("%s %s %s %d\n", nla_get_string(attrs[SMC_PNETID_NAME]),
	       nla_get_string(attrs[SMC_PNETID_ETHNAME]),
	       nla_get_string(attrs[SMC_PNETID_IBNAME]),
	       nla_get_u8(attrs[SMC_PNETID_IBPORT]));
	return NL_OK;
}

static int genl_command(void)
{
	int rc = EXIT_FAILURE, id, nlmsg_flags = 0;
	struct nl_sock *sk;
	struct nl_msg *msg;

	/* Allocate a netlink socket and connect to it */
	sk = nl_socket_alloc();
	if (!sk) {
		nl_perror(NLE_NOMEM, progname);
		return rc;
	}
	rc = genl_connect(sk);
	if (rc) {
		nl_perror(rc, progname);
		rc = EXIT_FAILURE;
		goto out1;
	}
	id = genl_ctrl_resolve(sk, SMCR_GENL_FAMILY_NAME);
	if (id < 0) {
		rc = EXIT_FAILURE;
		if (id == -NLE_OBJ_NOTFOUND)
			fprintf(stderr, "%s: SMC-R module not loaded\n",
				progname);
		else
			nl_perror(id, progname);
		goto out2;
	}
	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM, cb_handler, NULL);

	/* Allocate a netlink message and set header information. */
	msg = nlmsg_alloc();
	if (!msg) {
		nl_perror(NLE_NOMEM, progname);
		rc = EXIT_FAILURE;
		goto out2;
	}

	if ((pnetcmd.cmd == SMC_PNETID_DEL || pnetcmd.cmd == SMC_PNETID_GET) &&
	    !pnetcmd.pnetid)		/* List all */
		nlmsg_flags = NLM_F_DUMP;

	if (!genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, id, 0, nlmsg_flags,
			 pnetcmd.cmd, SMCR_GENL_FAMILY_VERSION)) {
		nl_perror(rc, progname);
		rc = EXIT_FAILURE;
		goto out3;
	}

	switch (pnetcmd.cmd) {		/* Start message construction */
	case SMC_PNETID_ADD:
		if (pnetcmd.ethname)
			rc = nla_put_string(msg, SMC_PNETID_ETHNAME,
					    pnetcmd.ethname);
		if (rc < 0) {
			nl_perror(rc, progname);
			rc = EXIT_FAILURE;
			goto out3;
		}

		if (pnetcmd.ibname)
			rc = nla_put_string(msg, SMC_PNETID_IBNAME,
					    pnetcmd.ibname);
		if (rc < 0) {
			nl_perror(rc, progname);
			rc = EXIT_FAILURE;
			goto out3;
		}

		if (pnetcmd.ibname)
			rc = nla_put_u8(msg, SMC_PNETID_IBPORT, pnetcmd.ibport);
		if (rc < 0) {
			nl_perror(rc, progname);
			rc = EXIT_FAILURE;
			goto out3;
		}
		/* Fall through intended */
	case SMC_PNETID_DEL:
	case SMC_PNETID_GET:
		if (!pnetcmd.pnetid)		/* List all */
			break;
		rc = nla_put_string(msg, SMC_PNETID_NAME, pnetcmd.pnetid);
		if (rc < 0) {
			nl_perror(rc, progname);
			rc = EXIT_FAILURE;
			goto out3;
		}
	}

	/* Send message */
	rc = nl_send_auto(sk, msg);
	if (rc < 0) {
		nl_perror(rc, progname);
		rc = EXIT_FAILURE;
		goto out3;
	}

	/* Receive reply message, returns number of cb invocations. */
	rc = nl_recvmsgs_default(sk);
	if (rc < 0) {
		nl_perror(rc, progname);
		rc = EXIT_FAILURE;
		goto out3;
	}
	rc = EXIT_SUCCESS;
out3:
	nlmsg_free(msg);
out2:
	nl_close(sk);
out1:
	nl_socket_free(sk);
	return rc;
}

int main(int argc, char **argv)
{
	char *slash;
	int rc, ch;

	progname = (slash = strrchr(argv[0], '/')) ? slash + 1 : argv[0];
	while ((ch = getopt_long(argc, argv, "I:D:P:fasdhvV", long_opts,
				 NULL )) != EOF) {
		switch (ch) {
		case 'f':
			if (pnetcmd.cmd)
				usage();
			pnetcmd.cmd = SMC_PNETID_FLUSH;
			break;
		case 's':
			if (pnetcmd.cmd)
				usage();
			pnetcmd.cmd = SMC_PNETID_GET;
			pnetcmd.pnetid = optarg;
			break;
		case 'd':
			if (pnetcmd.cmd)
				usage();
			pnetcmd.cmd = SMC_PNETID_DEL;
			pnetcmd.pnetid = optarg;
			break;
		case 'a':
			if (pnetcmd.cmd)
				usage();
			pnetcmd.cmd = SMC_PNETID_ADD;
			pnetcmd.pnetid = optarg;
			break;
		case 'I':
			pnetcmd.ethname = optarg;
			break;
		case 'D':
			pnetcmd.ibname = optarg;
			break;
		case 'P':
			pnetcmd.ibport = convert(optarg);
			break;
		case 'v':
		case 'V':
			printf("smc_pnet utility, smc-tools-%s (%s)\n",
			       RELEASE_STRING, RELEASE_LEVEL);
			exit(0);
		case 'h':
			help();
		case '?':
		default:
			usage();
		}
	}

	if (optind + 1 < argc) {
		fprintf(stderr, "%s too many parameters\n", progname);
		usage();
	}
	if (optind + 1 == argc)
		pnetcmd.pnetid = argv[optind];
	if (!pnetcmd.cmd) {
		if (optind < argc) {
			fprintf(stderr, "%s: parameters without option\n",
				progname);
			usage();
		}
		pnetcmd.cmd = SMC_PNETID_GET;
	}
	if (pnetcmd.cmd == SMC_PNETID_FLUSH) {
		if (optind < argc) {
			fprintf(stderr, "%s: -f takes no parameters\n",
				progname);
			usage();
		}
	}

	if (pnetcmd.cmd == SMC_PNETID_ADD) {
		if (!pnetcmd.ethname && !pnetcmd.ibname) {
			fprintf(stderr, "%s: interface or device missing\n",
				progname);
			usage();
		}
		if (!pnetcmd.ibport)
			pnetcmd.ibport = 1;
	}

	if (pnetcmd.cmd == SMC_PNETID_GET || pnetcmd.cmd == SMC_PNETID_DEL) {
		if (pnetcmd.ethname) {
			fprintf(stderr, "%s: interface %s ignored\n", progname,
					pnetcmd.ethname);
			pnetcmd.ethname = NULL;
		}
		if (pnetcmd.ibname) {
			fprintf(stderr, "%s: ibdevice %s ignored\n", progname,
					pnetcmd.ibname);
			pnetcmd.ibname = NULL;
		}
		if (pnetcmd.ibport) {
			fprintf(stderr, "%s: ibport %d ignored\n", progname,
					pnetcmd.ibport);
			pnetcmd.ibport = 0;
		}
	}
	rc = genl_command();
	return rc;
}
