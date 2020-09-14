/*
 * smc-tools/smctools_common.h
 *
 * Copyright IBM Corp. 2017
 *
 * Author(s): Ursula Braun (ubraun@linux.ibm.com)
 *
 * Copyright IBM Corp. 2017
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef SMCTOOLS_COMMON_H
#define SMCTOOLS_COMMON_H

#define STRINGIFY_1(x)		#x
#define STRINGIFY(x)		STRINGIFY_1(x)

#define RELEASE_STRING	STRINGIFY (SMC_TOOLS_RELEASE)
#define RELEASE_LEVEL   "3d7eead"

#define PF_SMC 43

#include <net/if.h>

#define SMC_MAX_PNETID_LEN 16 /* Max. length of PNET id */
#define SMC_LGR_ID_SIZE 4
#define SMC_MAX_PORTS 2 /* Max # of ports per ib device */
#define SMC_PCI_ID_STR_LEN 16

/***********************************************************
 * Mimic definitions in kernel/include/uapi/linux/smc.h
 ***********************************************************/

/* Netlink SMC_PNETID attributes */
enum {
	SMC_PNETID_UNSPEC,
	SMC_PNETID_NAME,
	SMC_PNETID_ETHNAME,
	SMC_PNETID_IBNAME,
	SMC_PNETID_IBPORT,
	__SMC_PNETID_MAX,
	SMC_PNETID_MAX = __SMC_PNETID_MAX - 1
};

enum {				/* SMC PNET Table commands */
	SMC_PNETID_GET = 1,
	SMC_PNETID_ADD,
	SMC_PNETID_DEL,
	SMC_PNETID_FLUSH
};

#define SMCR_GENL_FAMILY_NAME		"SMC_PNETID"
#define SMCR_GENL_FAMILY_VERSION	1

/***********************************************************
 * Mimic definitions in kernel/include/uapi/linux/smc_diag.h
 ***********************************************************/
#include <linux/types.h>
#include <linux/inet_diag.h>
#include <rdma/ib_user_verbs.h>

#define SMC_DIAG_EXTS_PER_CMD 16
/* Sequence numbers */
enum {
	MAGIC_SEQ = 123456,
	MAGIC_SEQ_V2,
	MAGIC_SEQ_V2_ACK,
};

/* Request structure */
struct smc_diag_req {
	__u8	diag_family;
	__u8	pad[2];
	__u8	diag_ext;		/* Query extended information */
	struct inet_diag_sockid	id;
};

/* Request structure v2 */
struct smc_diag_req_v2 {
	__u8	diag_family;
	__u8	pad[2];
	__u8	diag_ext;		/* Query extended information */
	struct inet_diag_sockid	id;
	__u32	cmd;
	__u32	cmd_ext;
	__u8	cmd_val[8];
};

/* Base info structure. It contains socket identity (addrs/ports/cookie) based
 * on the internal clcsock, and more SMC-related socket data
 */
struct smc_diag_msg {
	__u8	diag_family;
	__u8	diag_state;
	__u8	diag_mode;
	__u8	diag_shutdown;
	struct inet_diag_sockid id;

	__u32	diag_uid;
	__u64	diag_inode;
};

/* Mode of a connection */
enum {
	SMC_DIAG_MODE_SMCR,
	SMC_DIAG_MODE_FALLBACK_TCP,
	SMC_DIAG_MODE_SMCD,
};

/* GET_SOCK_DIAG command extensions */

enum {
	SMC_DIAG_NONE,
	SMC_DIAG_CONNINFO,
	SMC_DIAG_LGRINFO,
	SMC_DIAG_SHUTDOWN,
	SMC_DIAG_DMBINFO,
	SMC_DIAG_FALLBACK,
	__SMC_DIAG_MAX,
};

/* V2 Commands */
enum {
	SMC_DIAG_GET_LGR_INFO = SMC_DIAG_EXTS_PER_CMD,
	SMC_DIAG_GET_DEV_INFO,
	__SMC_DIAG_EXT_MAX,
};

/* SMC_DIAG_GET_LGR_INFO command extensions */
enum {
	SMC_DIAG_LGR_INFO_SMCR = 1,
	SMC_DIAG_LGR_INFO_SMCR_LINK,
	SMC_DIAG_LGR_INFO_SMCD,
};

/* SMC_DIAG_GET_DEV_INFO command extensions */
enum {
	SMC_DIAG_DEV_INFO_SMCD = 1,
	SMC_DIAG_DEV_INFO_SMCR,
};

#define SMC_DIAG_MAX (__SMC_DIAG_MAX - 1)
#define SMC_DIAG_EXT_MAX (__SMC_DIAG_EXT_MAX - 1)

/* SMC_DIAG_CONNINFO */
#define IB_DEVICE_NAME_MAX	64

struct smc_diag_cursor {
	__u16	reserved;
	__u16	wrap;
	__u32	count;
};

struct smc_diag_conninfo {
	__u32			token;		/* unique connection id */
	__u32			sndbuf_size;	/* size of send buffer */
	__u32			rmbe_size;	/* size of RMB element */
	__u32			peer_rmbe_size;	/* size of peer RMB element */
	/* local RMB element cursors */
	struct smc_diag_cursor	rx_prod;	/* received producer cursor */
	struct smc_diag_cursor	rx_cons;	/* received consumer cursor */
	/* peer RMB element cursors */
	struct smc_diag_cursor	tx_prod;	/* sent producer cursor */
	struct smc_diag_cursor	tx_cons;	/* sent consumer cursor */
	__u8			rx_prod_flags;	/* received producer flags */
	__u8			rx_conn_state_flags; /* recvd connection flags*/
	__u8			tx_prod_flags;	/* sent producer flags */
	__u8			tx_conn_state_flags; /* sent connection flags*/
	/* send buffer cursors */
	struct smc_diag_cursor	tx_prep;	/* prepared to be sent cursor */
	struct smc_diag_cursor	tx_sent;	/* sent cursor */
	struct smc_diag_cursor	tx_fin;		/* confirmed sent cursor */
};

/* SMC_DIAG_LINKINFO */

struct smc_diag_linkinfo {
	__u8 link_id;			/* link identifier */
	__u8 ibname[IB_DEVICE_NAME_MAX]; /* name of the RDMA device */
	__u8 ibport;			/* RDMA device port number */
	__u8 gid[40];			/* local GID */
	__u8 peer_gid[40];		/* peer GID */
	__u32 conn_cnt;
	__u8 netdev[IFNAMSIZ];
	__u8 link_uid[4];
	__u8 peer_link_uid[4];
	__u32 link_state;
};

struct smc_diag_lgrinfo {
	struct smc_diag_linkinfo	lnk[1];
	__u8				role;
};

struct smc_diag_fallback {
	__u32 reason;
	__u32 peer_diagnosis;
};

struct smcd_diag_dmbinfo {		/* SMC-D Socket internals */
	__u32 linkid;			/* Link identifier */
	__u64 peer_gid;			/* Peer GID */
	__u64 my_gid;			/* My GID */
	__u64 token;			/* Token of DMB */
	__u64 peer_token;		/* Token of remote DMBE */
	__u8		pnet_id[SMC_MAX_PNETID_LEN];
	__u32		conns_num;
	__u8		vlan_id;
};

struct smc_diag_dev_info {
	__u8		pnet_id[SMC_MAX_PORTS][SMC_MAX_PNETID_LEN];
	__u8		pnetid_by_user[SMC_MAX_PORTS];
	__u32		use_cnt;
	__u8		is_critical;
	__u32		pci_fid;
	__u16		pci_pchid;
	__u16		pci_vendor;
	__u16		pci_device;
	__u8		pci_id[SMC_PCI_ID_STR_LEN];
	__u8		dev_name[IB_DEVICE_NAME_MAX];
	__u8		netdev[SMC_MAX_PORTS][IFNAMSIZ];
	__u8		port_state[SMC_MAX_PORTS];
	__u8		port_valid[SMC_MAX_PORTS];
};


struct smc_diag_lgr {
	__u8		lgr_id[SMC_LGR_ID_SIZE];
	__u8		lgr_role;
	__u8		lgr_type;
	__u8		pnet_id[SMC_MAX_PNETID_LEN];
	__u8		vlan_id;
	__u32		conns_num;
};
#endif /* SMCTOOLS_COMMON_H */
