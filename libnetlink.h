/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2020
 *
 * Author(s): Ursula Braun <ubraun@linux.ibm.com>
 *            Guvenc Gulce <guvenc@linux.ibm.com>
 *
 *
 * Userspace program for SMC Information display
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 */
#include <linux/sock_diag.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>

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

#define DIAG_REQUEST(_req, _r, _seq)						    \
	struct {							    \
		struct nlmsghdr nlh;					    \
		_r;							    \
	} _req = {							    \
		.nlh = {						    \
			.nlmsg_type = SOCK_DIAG_BY_FAMILY,		    \
			.nlmsg_flags = NLM_F_ROOT|NLM_F_REQUEST,	    \
			.nlmsg_seq = _seq,				    \
			.nlmsg_len = sizeof(_req),			    \
		},                                                          \
	}

int rtnl_open(struct rtnl_handle *rth);
void rtnl_close(struct rtnl_handle *rth);
int rtnl_dump(struct rtnl_handle *rth, void (*handler)(struct nlmsghdr *nlh));
void parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len);
int sockdiag_send(int fd, int cmd);
void set_extension(int ext);
