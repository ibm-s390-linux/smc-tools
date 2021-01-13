/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2017, 2018
 *
 * Author(s):  Ursula Braun <ubraun@linux.ibm.com>
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

#include "smctools_common.h"
#include "libnetlink.h"

#define ADDR_LEN_SHORT	23

static char *progname;
int show_debug;
int show_smcr;
int show_smcd;
int show_wide;
int listening = 0;
int all = 0;

static void print_header(void)
{
	printf("State          ");
	printf("UID   ");
	printf("Inode   ");
	printf("Local Address           ");
	printf("Peer Address            ");
	printf("Intf ");
        printf("Mode ");

	if (show_debug) {
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

/* format one sockaddr / port */
static void addr_format(char *buf, size_t buf_len, size_t short_len,
			__be32 addr[4], int port)
{
	char addr_buf[INET6_ADDRSTRLEN + 1], port_buf[16];
	int addr_len, port_len;
	int af;

	/* There was an upstream discussion about the content of the
	 * diag_family field. Originally it was AF_SMC, but was changed with
	 * IPv6 support to indicate AF_INET or AF_INET6. Upstream complained
	 * later that there is no way to separate AF_INET from AF_SMC diag msgs.
	 * We now change back the value of the diag_family field to be always
	 * AF_SMC. We now 'parse' the IP address type.
	 * Note that smc_diag.c in kernel always clears the whole addr field
	 * before the ip address is copied into and we can rely on that here.
	 */
	if (addr[1] == 0 && addr[2] == 0 && addr[3] == 0)
		af = AF_INET;
	else
		af = AF_INET6;

	if (buf_len < 20)
		return; /* no space for errmsg */

	if (!inet_ntop(af, addr, addr_buf, sizeof(addr_buf))) {
		strcpy(buf, "(inet_ntop error)");
		return;
	}
	sprintf(port_buf, "%d", port);
	addr_len = strlen(addr_buf);
	port_len = strlen(port_buf);
	if (!show_wide && (addr_len + 1 + port_len > short_len)) {
		if (buf_len < short_len + 1) {
			strcpy(buf, "(buf to small)");
			return;
		}
		/* truncate addr string */
		addr_len = short_len - 1 - port_len - 2;
		strncpy(buf, addr_buf, addr_len);
		buf[addr_len] = '\0';
		strcat(buf, ".."); /* indicate truncation */
		strcat(buf, ":");
		strcat(buf, port_buf);
	} else {
		if (buf_len < addr_len + 1 + port_len + 1) {
			strcpy(buf, "(buf to small)");
			return;
		}
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
		    r->id.idiag_src, ntohs(r->id.idiag_sport));
	printf("%-*s ", (int)MAX(ADDR_LEN_SHORT, strlen(txtbuf)), txtbuf);
	if (r->diag_state == 10)		/* LISTEN state */
		goto newline;

	addr_format(txtbuf, sizeof(txtbuf), ADDR_LEN_SHORT,
		    r->id.idiag_dst, ntohs(r->id.idiag_dport));
	printf("%-*s ", (int)MAX(ADDR_LEN_SHORT, strlen(txtbuf)), txtbuf);
	printf("%04x ", r->id.idiag_if);
	if (r->diag_state == 7)			/* CLOSED state */
		goto newline;
	
	if (r->diag_mode == SMC_DIAG_MODE_FALLBACK_TCP) {
		printf("TCP ");
		/* when available print local and peer fallback reason code */
		if (tb[SMC_DIAG_FALLBACK] &&
		    tb[SMC_DIAG_FALLBACK]->rta_len >= sizeof(struct smc_diag_fallback))
		{
			struct smc_diag_fallback fallback;

			fallback = *(struct smc_diag_fallback *)RTA_DATA(tb[SMC_DIAG_FALLBACK]);
			printf("0x%08x", fallback.reason);
			if (fallback.peer_diagnosis)
				printf("/0x%08x", fallback.peer_diagnosis);
		}
		goto newline;

	} else if (r->diag_mode == SMC_DIAG_MODE_SMCD)
		printf("%4s ", "SMCD");
	else
		printf("%4s ", "SMCR");

	if (show_debug) {
		if (tb[SMC_DIAG_SHUTDOWN] &&
		    tb[SMC_DIAG_SHUTDOWN]->rta_len >= sizeof(__u8))
		{
			unsigned char mask;

			mask = *(__u8 *)RTA_DATA(tb[SMC_DIAG_SHUTDOWN]);
			printf(" %c-%c  ", mask & 1 ? 'R' : '<', mask & 2 ? 'W' : '>');
		}

		if (tb[SMC_DIAG_CONNINFO] &&
		    tb[SMC_DIAG_CONNINFO]->rta_len >= sizeof(struct smc_diag_conninfo))
		{
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
		if (tb[SMC_DIAG_LGRINFO] &&
		    tb[SMC_DIAG_LGRINFO]->rta_len >= sizeof(struct smc_diag_lgrinfo))
		{
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
		if (tb[SMC_DIAG_DMBINFO] &&
		    tb[SMC_DIAG_DMBINFO]->rta_len >= sizeof(struct smcd_diag_dmbinfo))
		{
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

static int smc_show_netlink()
{
	struct rtnl_handle rth;
	unsigned char cmd = 0;
	int rc = 0;

	if ((rc = rtnl_open(&rth)))
		return EXIT_FAILURE;

	rth.dump = MAGIC_SEQ;

	if (show_debug)
		cmd |= (1<<(SMC_DIAG_CONNINFO-1));

	if (show_smcr)
		cmd |= (1<<(SMC_DIAG_LGRINFO-1));

	if (show_smcd)
		cmd |= (1<<(SMC_DIAG_DMBINFO-1));

	if ((rc = sockdiag_send(rth.fd, cmd)))
		goto exit;

	print_header();

	rc = rtnl_dump(&rth, show_one_smc_sock);

exit:
	rtnl_close(&rth);
	return rc;
}

static const struct option long_opts[] = {
	{ "all", 0, 0, 'a' },
	{ "debug", 0, 0, 'd' },
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
"\t-v, --version       show version information\n"
"\t-a, --all           show all sockets\n"
"\t-l, --listening     show listening sockets\n"
"\t-d, --debug         show debug socket information\n"
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

	while ((ch = getopt_long(argc, argv, "aldDRhvW", long_opts, NULL)) != EOF) {
		switch (ch) {
		case 'a':
			all++;
			break;
		case 'l':
			listening++;
			break;
		case 'd':
			show_debug++;
			break;
		case 'D':
			show_smcd++;
			break;
		case 'R':
			show_smcr++;
			break;
		case 'v':
			printf("smcss utility, smc-tools-%s\n", RELEASE_STRING);
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
	if (listening && show_debug) {
		fprintf(stderr, "--listening together with --debug is not supported\n");
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
