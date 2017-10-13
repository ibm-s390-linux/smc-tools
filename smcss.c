/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright (c) IBM Corp. 2017
 *
 * Author(s):  Ursula Braun <ubraun@linux.vnet.ibm.com>
 *
 * User space program for SMC Socket display
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

#include <linux/tcp.h>
#include <linux/sock_diag.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>

#include "smctools_common.h"
#include "smc_diag.h"

#define MAGIC_SEQ 123456

struct rtnl_handle {
	int			fd;
	struct sockaddr_nl	local;
	struct sockaddr_nl	peer;
	__u32			seq;
	__u32			dump;
	int			proto;
	FILE			*dump_fp;
	int			flags;
};

static char *progname;
int show_details;
int show_lgr;
int listening = 0;
int all = 0;

static int rtnl_open(struct rtnl_handle *rth)
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

#define DIAG_REQUEST(_req, _r)						    \
	struct {							    \
		struct nlmsghdr nlh;					    \
		_r;							    \
	} _req = {							    \
		.nlh = {						    \
			.nlmsg_type = SOCK_DIAG_BY_FAMILY,		    \
			.nlmsg_flags = NLM_F_ROOT|NLM_F_REQUEST,	    \
			.nlmsg_seq = MAGIC_SEQ,				    \
			.nlmsg_len = sizeof(_req),			    \
		},                                                          \
	}

static int sockdiag_send(int fd)
{
	struct sockaddr_nl nladdr = { .nl_family = AF_NETLINK };
	DIAG_REQUEST(req, struct smc_diag_req r);
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

	if (show_details)
		req.r.diag_ext |= (1<<(SMC_DIAG_CONNINFO-1));

	if (show_lgr)
		req.r.diag_ext |= (1<<(SMC_DIAG_LGRINFO-1));

	if (sendmsg(fd, &msg, 0) < 0) {
		close(fd);
		return EXIT_FAILURE;
	}

	return 0;
}

static void print_header(void)
{
	printf("     State     ");
	printf(" Inode ");
	printf(" UID  ");
	printf("        Local Address ");
	printf("      Foreign Address ");
	printf("Intf ");
        printf("Mode ");

	if (show_details) {
		printf("Shutd ");
		printf("Token    ");
		printf("Sndbuf   ");
		printf("Rcvbuf   ");
		printf("Peerbuf  ");
		printf("rxprod-Cursor ");
		printf("rxcons-Cursor ");
		printf("rxFlags ");
		printf("txprod-Cursor ");
		printf("txcons-Cursor ");
		printf("txFlags ");
		printf("txprep-Cursor ");
		printf("txsent-Cursor ");
		printf(" txfin-Cursor ");
	}

	if (show_lgr) {
		printf(" Role ");
		printf("    IB-device    ");
		printf("Port ");
		printf("Linkid ");
		printf("                 GID              ");
		printf("                Peer-GID          ");
	}

	printf("\n");
}

static const char *smc_state(unsigned char x)
{
	static char buf[16];

	switch (x) {
	case 1:		return "ACTIVE";
	case 2:		return "INIT";
	case 7:		return "CLOSED";
	case 10:	return "LISTEN";
	case 20:	return "PEERCLOSEWAIT1";
	case 21:	return "PEERCLOSEWAIT2";
	case 22:	return "APPCLOSEWAIT1";
	case 23:	return "APPCLOSEWAIT2";
	case 24:	return "APPFINCLOSEWAIT1";
	case 25:	return "PEERFINCLOSEWAIT";
	case 26:	return "PEERABORTWAIT";
	case 27:	return "PROCESSABORT";
	default:	sprintf(buf, "%#x?", x); return buf;
	}
}

static int parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta,
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
	return 0;
}

static void show_one_smc_sock(struct nlmsghdr *nlh)
{
	struct rtattr *tb[SMC_DIAG_MAX + 1];
	struct smc_diag_msg *r = NLMSG_DATA(nlh);
	struct in_addr in;
	unsigned long long inode;

	parse_rtattr(tb, SMC_DIAG_MAX, (struct rtattr *)(r+1),
		     nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*r)));

	if (listening) {
		if ( r->diag_state != 10)
			return;
	} else {
		if (!all && (r->diag_state == 10 || r->diag_state == 2))
			return;
	}

	printf("%14s ", smc_state(r->diag_state));
	printf("%5d ", r->diag_uid);
	inode = r->diag_inode;
	printf("%6llu ", inode);
	if (r->diag_state == 2)			/* INIT state */
		goto newline;

	in.s_addr = r->id.idiag_src[0];
	printf("%15s:%5d ", inet_ntoa(in), ntohs(r->id.idiag_sport));
	if (r->diag_state == 10)		/* LISTEN state */
		goto newline;

	in.s_addr = r->id.idiag_dst[0];
	printf("%15s:%5d ", inet_ntoa(in), ntohs(r->id.idiag_dport));
	printf("%04x ", r->id.idiag_if);
	printf("%4s ", r->diag_fallback ? "TCP " : "RDMA");

	if (r->diag_fallback)
		goto newline;

	if (show_details) {
		if (tb[SMC_DIAG_SHUTDOWN]) {
			unsigned char mask;

			mask = *(__u8 *)RTA_DATA(tb[SMC_DIAG_SHUTDOWN]);
			printf(" %c-%c  ", mask & 1 ? 'R' : '<', mask & 2 ? 'W' : '>');
		}

		if (tb[SMC_DIAG_CONNINFO]) {
			struct smc_diag_conninfo cinfo;

			cinfo = *(struct smc_diag_conninfo *)RTA_DATA(tb[SMC_DIAG_CONNINFO]);
			printf("%08x ", cinfo.token);
			printf("%08x ", cinfo.sndbuf_size);
			printf("%08x ", cinfo.rmbe_size);
			printf("%08x ", cinfo.peer_rmbe_size);

			printf("%04x:%08x ", cinfo.rx_prod.wrap, cinfo.rx_prod.count);
			printf("%04x:%08x ", cinfo.rx_cons.wrap, cinfo.rx_cons.count);
			printf("%02x:%02x ", cinfo.rx_prod_flags, cinfo.rx_conn_state_flags);
			printf("%04x:%08x ", cinfo.tx_prod.wrap, cinfo.tx_prod.count);
			printf("%04x:%08x ", cinfo.tx_cons.wrap, cinfo.tx_cons.count);
			printf("%02x:%02x ", cinfo.tx_prod_flags, cinfo.tx_conn_state_flags);
			printf("%04x:%08x ", cinfo.tx_prep.wrap, cinfo.tx_prep.count);
			printf("%04x:%08x ", cinfo.tx_sent.wrap, cinfo.tx_sent.count);
			printf("%04x:%08x ", cinfo.tx_fin.wrap, cinfo.tx_fin.count);
		}
	}

	if (show_lgr) {
		if (tb[SMC_DIAG_LGRINFO]) {
			struct smc_diag_lgrinfo linfo;

			linfo = *(struct smc_diag_lgrinfo *)RTA_DATA(tb[SMC_DIAG_LGRINFO]);
			printf("%4s ", linfo.role ? "SERV" : "CLNT");
			printf("%15s ", linfo.lnk[0].ibname);
			printf("%02x ", linfo.lnk[0].ibport);
			printf("%02x ", linfo.lnk[0].link_id);
			printf("%s ", linfo.lnk[0].gid);
			printf("%s ", linfo.lnk[0].peer_gid);
		}
	}

newline:
	printf("\n");
	return;
}

static int rtnl_dump(struct rtnl_handle *rth)
{
		struct sockaddr_nl nladdr;
	struct iovec iov;
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	char buf[32768];
	int msglen;
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
			break; /* process next */
		}
		if (h->nlmsg_type == NLMSG_ERROR) {
			if (h->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
				fprintf(stderr, "ERROR truncated\n");
			} else {
				perror("RTNETLINK answers");
			}
			return EXIT_FAILURE;
		}
		show_one_smc_sock(h);
		h = NLMSG_NEXT(h, msglen);
	}
	if (msg.msg_flags & MSG_TRUNC) {
		fprintf(stderr, "Message truncated\n");
		goto again;
	}
	return EXIT_SUCCESS;
}

static int smc_show_netlink()
{
	struct rtnl_handle rth;
	int rc = 0;

	if ((rc = rtnl_open(&rth)))
		return EXIT_FAILURE;

	rth.dump = MAGIC_SEQ;

	if ((rc = sockdiag_send(rth.fd)))
		goto exit;

	print_header();

	rc = rtnl_dump(&rth);

exit:
	rtnl_close(&rth);
	return rc;
}

static const struct option long_opts[] = {
	{ "all", 0, 0, 'a' },
	{ "extended", 0, 0, 'e' },
	{ "listening", 0, 0, 'l' },
	{ "linkgroup", 0, 0, 'L' },
	{ "version", 0, 0, 'V' },
	{ "help", 0, 0, 'h' },
	{ NULL, 0, NULL, 0}
};

static void _usage(FILE *dest)
{
	fprintf(dest,
"Usage: %s [ OPTIONS ]\n"
"\t-h, --help          this message\n"
"\t-V, --version       show version information\n"
"\t-a, --all           show all sockets\n"
"\t-l, --listening     show listening sockets\n"
"\t-e, --extended      show detailed socket information\n"
"\t-L, --linkgroup     show linkgroups\n"
"\tno OPTIONS          show all connected sockets\n",
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

int main(int argc, char *argv[])
{
	char *slash;
	int rc = 0;
	int ch;

	progname = (slash = strrchr(argv[0], '/')) ? slash + 1 : argv[0];

	while ((ch = getopt_long(argc, argv, "aleLhvV", long_opts, NULL)) != EOF) {
		switch (ch) {
		case 'a':
			all = 1;
			listening = 0;
			break;
		case 'l':
			if (!all)
				listening = 1;
			break;
		case 'e':
			show_details++;
			break;
		case 'L':
			show_lgr++;
			break;
		case 'v':
		case 'V':
			printf("smcss utility, smc-tools-%s\n", RELEASE_STRING);
			exit(0);
		case 'h':
			help();
		case '?':
		default:
			usage();
		}
	}

	rc = smc_show_netlink();
	return rc;
}
