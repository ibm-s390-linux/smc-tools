/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright (c) IBM Corp. 2017, 2018
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
#include <sys/param.h>

#include <linux/tcp.h>
#include <linux/sock_diag.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>

#include "smctools_common.h"
#include "smc_diag.h"

#define MAGIC_SEQ 123456
#define ADDR_LEN_SHORT	23

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
int show_smcr;
int show_smcd;
int show_wide;
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

	if (show_smcr)
		req.r.diag_ext |= (1<<(SMC_DIAG_LGRINFO-1));

	if (show_smcd)
		req.r.diag_ext |= (1<<(SMC_DIAG_DMBINFO-1));

	if (sendmsg(fd, &msg, 0) < 0) {
		close(fd);
		return EXIT_FAILURE;
	}

	return 0;
}

static void print_header(void)
{
	printf("State          ");
	printf("UID   ");
	printf("Inode   ");
	printf("Local Address           ");
	printf("Foreign Address         ");
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
		printf("txfin-Cursor  ");
	}

	if (show_smcr) {
		printf("Role ");
		printf("IB-device       ");
		printf("Port ");
		printf("Linkid ");
		printf("GID                                      ");
		printf("Peer-GID");
	}

	if (show_smcd) {
		printf("GID              ");
		printf("Token            ");
		printf("Peer-GID         ");
		printf("Peer-Token       ");
		printf("Linkid");
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

static void parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta,
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

/* format one sockaddr / port */
static void addr_format(char *buf, size_t buf_len, size_t short_len,
			int af, void *addr, int port)
{
	char *errmsg = "(inet_ntop error)"; /* very unlikely */
	char addr_buf[64], port_buf[16];
	int addr_len, port_len;

	if (!inet_ntop(af, addr, addr_buf, sizeof(addr_buf))) {
		strcpy(buf, errmsg);
		return;
	}
	sprintf(port_buf, "%d", port);
	addr_len = strlen(addr_buf);
	port_len = strlen(port_buf);
	if (!show_wide && (addr_len + 1 + port_len > short_len)) {
		/* truncate addr string */
		addr_len = short_len - 1 - port_len - 2;
		strncpy(buf, addr_buf, addr_len);
		buf[addr_len] = '\0';
		strcat(buf, ".."); /* indicate truncation */
		strcat(buf, ":");
		strncat(buf, port_buf, port_len);
	} else {
		snprintf(buf, buf_len, "%s:%s", addr_buf, port_buf);
	}
}

static void show_one_smc_sock(struct nlmsghdr *nlh)
{
	struct smc_diag_msg *r = NLMSG_DATA(nlh);
	struct rtattr *tb[SMC_DIAG_MAX + 1];
	unsigned long long inode;
	char txtbuf[128];

	parse_rtattr(tb, SMC_DIAG_MAX, (struct rtattr *)(r+1),
		     nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*r)));

	if (listening) {
		if ( r->diag_state != 10)
			return;
	} else {
		if (!all && (r->diag_state == 10 || r->diag_state == 2))
			return;
	}
	if (show_smcr && r->diag_mode != SMC_DIAG_MODE_SMCR)
		return;	/* show only SMC-R sockets */
	if (show_smcd && r->diag_mode != SMC_DIAG_MODE_SMCD)
		return;	/* show only SMC-D sockets */

	printf("%-14s ", smc_state(r->diag_state));
	printf("%05d ", r->diag_uid);
	inode = r->diag_inode;
	printf("%07llu ", inode);
	if (r->diag_state == 2)			/* INIT state */
		goto newline;

	addr_format(txtbuf, sizeof(txtbuf), ADDR_LEN_SHORT,
		    r->diag_family, r->id.idiag_src, ntohs(r->id.idiag_sport));
	printf("%-*s ", (int)MAX(ADDR_LEN_SHORT, strlen(txtbuf)), txtbuf);
	if (r->diag_state == 10)		/* LISTEN state */
		goto newline;

	addr_format(txtbuf, sizeof(txtbuf), ADDR_LEN_SHORT,
		    r->diag_family, r->id.idiag_dst, ntohs(r->id.idiag_dport));
	printf("%-*s ", (int)MAX(ADDR_LEN_SHORT, strlen(txtbuf)), txtbuf);
	printf("%04x ", r->id.idiag_if);
	if (r->diag_mode == SMC_DIAG_MODE_FALLBACK_TCP)
		printf("%4s ", "TCP ");
	else if (r->diag_mode == SMC_DIAG_MODE_SMCD)
		printf("%4s ", "SMCD");
	else
		printf("%4s ", "SMCR");

	if (r->diag_mode == SMC_DIAG_MODE_FALLBACK_TCP)
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
			printf("%02x:%02x   ", cinfo.rx_prod_flags, cinfo.rx_conn_state_flags);
			printf("%04x:%08x ", cinfo.tx_prod.wrap, cinfo.tx_prod.count);
			printf("%04x:%08x ", cinfo.tx_cons.wrap, cinfo.tx_cons.count);
			printf("%02x:%02x   ", cinfo.tx_prod_flags, cinfo.tx_conn_state_flags);
			printf("%04x:%08x ", cinfo.tx_prep.wrap, cinfo.tx_prep.count);
			printf("%04x:%08x ", cinfo.tx_sent.wrap, cinfo.tx_sent.count);
			printf("%04x:%08x ", cinfo.tx_fin.wrap, cinfo.tx_fin.count);
		}
	}

	if (show_smcr) {
		if (tb[SMC_DIAG_LGRINFO]) {
			struct smc_diag_lgrinfo linfo;

			linfo = *(struct smc_diag_lgrinfo *)RTA_DATA(tb[SMC_DIAG_LGRINFO]);
			printf("%4s ", linfo.role ? "SERV" : "CLNT");
			printf("%-15s ", linfo.lnk[0].ibname);
			printf("%02x   ", linfo.lnk[0].ibport);
			printf("%02x     ", linfo.lnk[0].link_id);
			printf("%-40s ", linfo.lnk[0].gid);
			printf("%s", linfo.lnk[0].peer_gid);
		}
	}

	if (show_smcd) {
		if (tb[SMC_DIAG_DMBINFO]) {
			struct smcd_diag_dmbinfo dinfo;

			dinfo = *(struct smcd_diag_dmbinfo *)RTA_DATA(tb[SMC_DIAG_DMBINFO]);
			printf("%016llx ", dinfo.my_gid);
			printf("%016llx ", dinfo.token);
			printf("%016llx ", dinfo.peer_gid);
			printf("%016llx ", dinfo.peer_token);
			printf("%08x ", dinfo.linkid);
		}
	}

newline:
	printf("\n");
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
		if (h->nlmsg_type == NLMSG_DONE)
			break; /* process next */
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
	{ "smcd", 0, 0, 'D' },
	{ "smcr", 0, 0, 'R' },
	{ "version", 0, 0, 'V' },
	{ "wide", 0, 0, 'W' },
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
"\t-W, --wide          do not truncate IP addresses\n"
"\t-D, --smcd          show detailed SMC-D information (shows only SMC-D sockets)\n"
"\t-R, --smcr          show detailed SMC-R information (shows only SMC-R sockets)\n"
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
	int ch;

	progname = (slash = strrchr(argv[0], '/')) ? slash + 1 : argv[0];

	while ((ch = getopt_long(argc, argv, "aleDRhvVW", long_opts, NULL)) != EOF) {
		switch (ch) {
		case 'a':
			all++;
			break;
		case 'l':
			listening++;
			break;
		case 'e':
			show_details++;
			break;
		case 'D':
			show_smcd++;
			break;
		case 'R':
			show_smcr++;
			break;
		case 'v':
		case 'V':
			printf("smcss utility, smc-tools-%s (%s)\n", RELEASE_STRING,
			       RELEASE_LEVEL);
			exit(0);
		case 'W':
			show_wide++;
			break;
		case 'h':
			help();
		case '?':
		default:
			usage();
		}
	}

	if (show_smcr && show_smcd) {
		fprintf(stderr, "--smcd together with --smcr is not supported\n");
		usage();
	}
	if (listening && show_details) {
		fprintf(stderr, "--listening together with --extended is not supported\n");
		usage();
	}
	if (listening && all) {
		fprintf(stderr, "--listening together with --all is not supported\n");
		usage();
	}
	if (listening && show_smcr) {
		fprintf(stderr, "--listening together with --smcr is not supported\n");
		usage();
	}
	if (listening && show_smcd) {
		fprintf(stderr, "--listening together with --smcd is not supported\n");
		usage();
	}
	return smc_show_netlink();
}
