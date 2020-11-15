/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2020
 *
 * Author(s): Ursula Braun <ubraun@linux.ibm.com>
 *            Guvenc Gulce <guvenc@linux.ibm.com>
 *
 * Userspace program for SMC Information display
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <netlink/socket.h>
#include <netlink/msg.h>
#include <netlink/genl/ctrl.h>

#include "smctools_common.h"
#include "libnetlink.h"

static int local_ext, smc_id = 0;
static struct nl_sock *sk;
static char progname[16];

/* Operations on sock_diag netlink socket */

void set_extension(int ext)
{
	local_ext |= (1<<(ext-1));
};

int rtnl_open(struct rtnl_handle *rth)
{
	socklen_t addr_len;
	int rcvbuf = 1024 * 1024;
	int sndbuf = 32768;

	rth->fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC,
			 NETLINK_SOCK_DIAG);
	if (rth->fd < 0) {
		perror("Cannot open netlink socket");
		return EXIT_FAILURE;
	}
	if (setsockopt(rth->fd, SOL_SOCKET, SO_SNDBUF, &sndbuf,
		       sizeof(sndbuf)) < 0) {
		perror("SO_SNDBUF");
		return EXIT_FAILURE;
	}
	if (setsockopt(rth->fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf,
		       sizeof(rcvbuf)) < 0) {
		perror("SO_RCVBUF");
		return EXIT_FAILURE;
	}
	memset(&rth->local, 0, sizeof(rth->local));
	rth->local.nl_family = AF_NETLINK;
	rth->local.nl_groups = 0;
	if (bind(rth->fd, (struct sockaddr*)&rth->local,
		 sizeof(rth->local)) < 0) {
		perror("Cannot bind netlink socket");
		return EXIT_FAILURE;
	}
	addr_len = sizeof(rth->local);
	if (getsockname(rth->fd, (struct sockaddr*)&rth->local,
			&addr_len) < 0) {
		perror("Cannot getsockname");
		return EXIT_FAILURE;
	}
	if (addr_len != sizeof(rth->local)) {
		fprintf(stderr, "Wrong address length %d\n", addr_len);
		return EXIT_FAILURE;
	}
	if (rth->local.nl_family != AF_NETLINK) {
		fprintf(stderr, "Wrong address family %d\n",
			rth->local.nl_family);
		return EXIT_FAILURE;
	}

	rth->seq = time(NULL);
	return 0;
}

void rtnl_close(struct rtnl_handle *rth)
{
	if (rth->fd >= 0) {
		close(rth->fd);
		rth->fd = -1;
	}
}

int rtnl_dump(struct rtnl_handle *rth, void (*handler)(struct nlmsghdr *nlh))
{
	int msglen, found_done = 0;
	struct sockaddr_nl nladdr;
	struct iovec iov;
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	char buf[32768];
	struct nlmsghdr *h = (struct nlmsghdr *)buf;

	memset(buf, 0, sizeof(buf));
	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
again:
	msglen = recvmsg(rth->fd, &msg, 0);
	if (msglen < 0) {
		if (errno == EINTR || errno == EAGAIN)
			goto again;
		fprintf(stderr, "netlink receive error %s (%d)\n",
			strerror(errno), errno);
		return EXIT_FAILURE;
	}
	if (msglen == 0) {
		fprintf(stderr, "EOF on netlink\n");
		return EXIT_FAILURE;
	}

	while(NLMSG_OK(h, msglen)) {
		if (h->nlmsg_flags & NLM_F_DUMP_INTR)
			fprintf(stderr, "Dump interrupted\n");
		if (h->nlmsg_type == NLMSG_DONE) {
			found_done = 1;
			break;
		}
		if (h->nlmsg_type == NLMSG_ERROR) {
			if (h->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
				fprintf(stderr, "ERROR truncated\n");
			} else {
				perror("RTNETLINK answers");
			}
			return EXIT_FAILURE;
		}
		(*handler)(h);
		h = NLMSG_NEXT(h, msglen);
	}
	if (msg.msg_flags & MSG_TRUNC) {
		fprintf(stderr, "Message truncated\n");
		goto again;
	}
	if (!found_done) {
		h = (struct nlmsghdr *)buf;
		goto again;
	}
	return EXIT_SUCCESS;
}

void parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta,
			int len)
{
	unsigned short type;

	memset(tb, 0, sizeof(struct rtattr *) * (max + 1));
	while (RTA_OK(rta, len)) {
		type = rta->rta_type;
		if ((type <= max) && (!tb[type]))
			tb[type] = rta;
		rta = RTA_NEXT(rta,len);
	}
	if (len)
		fprintf(stderr, "!!!Deficit %d, rta_len=%d\n", len, rta->rta_len);
}

int sockdiag_send(int fd, int cmd)
{
	struct sockaddr_nl nladdr = { .nl_family = AF_NETLINK };
	DIAG_REQUEST(req, struct smc_diag_req_v2 r, MAGIC_SEQ_V2);
	struct msghdr msg;
	struct iovec iov[1];
	int iovlen = 1;

	memset(&req.r, 0, sizeof(req.r));
	req.r.diag_family = PF_SMC;

	iov[0] = (struct iovec) {
		.iov_base = &req,
		.iov_len = sizeof(req)
	};

	msg = (struct msghdr) {
		.msg_name = (void *)&nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = iov,
		.msg_iovlen = iovlen,
	};

	req.r.cmd = cmd;
	req.r.diag_ext = (char)local_ext;
	req.r.cmd_ext = local_ext;

	if (sendmsg(fd, &msg, 0) < 0) {
		close(fd);
		return EXIT_FAILURE;
	}

	return 0;
}

/* Operations on generic netlink sockets */

int gen_nl_open(char *pname)
{
	int rc = EXIT_FAILURE;

	snprintf(progname, sizeof(progname), "%s", pname);
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
		goto err1;
	}
	smc_id = genl_ctrl_resolve(sk, SMC_GENL_FAMILY_NAME);
	if (smc_id < 0) {
		rc = EXIT_FAILURE;
		if (smc_id == -NLE_OBJ_NOTFOUND)
			fprintf(stderr, "%s: SMC module not loaded\n",
				progname);
		else
			nl_perror(smc_id, progname);
		goto err2;
	}

	return EXIT_SUCCESS;
err2:
	nl_close(sk);
err1:
	nl_socket_free(sk);
	return rc;

}

int gen_nl_handle(int cmd, int (*cb_handler)(struct nl_msg *msg, void *arg))
{
	int rc = EXIT_FAILURE, nlmsg_flags = 0;
	struct nl_msg *msg;

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM, cb_handler, NULL);

	/* Allocate a netlink message and set header information. */
	msg = nlmsg_alloc();
	if (!msg) {
		nl_perror(NLE_NOMEM, progname);
		rc = EXIT_FAILURE;
		goto err;
	}

	nlmsg_flags = NLM_F_DUMP;

	if (!genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, smc_id, 0, nlmsg_flags,
			 cmd, SMC_GENL_FAMILY_VERSION)) {
		nl_perror(rc, progname);
		rc = EXIT_FAILURE;
		goto err;
	}

	/* Send message */
	rc = nl_send_auto(sk, msg);
	if (rc < 0) {
		nl_perror(rc, progname);
		rc = EXIT_FAILURE;
		goto err;
	}

	/* Receive reply message, returns number of cb invocations. */
	rc = nl_recvmsgs_default(sk);

	if (rc < 0) {
		nl_perror(rc, progname);
		rc = EXIT_FAILURE;
		goto err;
	}

	return EXIT_SUCCESS;
err:
	return rc;
}
