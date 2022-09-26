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

#define RELEASE_STRING		"1.8.2"

#define PF_SMC 43

#include <net/if.h>

/***********************************************************
 * Mimic definitions in kernel/include/uapi/linux/smc.h
 ***********************************************************/

#define SMC_MAX_PNETID_LEN	16 /* Max. length of PNET id */
#define SMC_LGR_ID_SIZE		4
#define SMC_MAX_IBNAME		8
#define SMC_MAX_HOSTNAME_LEN	32 /* Max length of hostname */
#define SMC_MAX_EID_LEN		32 /* Max length of eid */
#define SMC_MAX_UEID		8  /* Max number of eids */
#define SMC_MAX_PORTS		2  /* Max # of ports per ib device */
#define SMC_PCI_ID_STR_LEN	16 /* Max length of pci id string */

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
/* Use for accessing non-socket information like */
/* SMC links, linkgroups and devices */
#define SMC_GENL_FAMILY_NAME	"SMC_GEN_NETLINK"
#define SMC_GENL_FAMILY_VERSION	1

/* SMC_GENL_FAMILY commands */
enum {
	SMC_NETLINK_GET_SYS_INFO = 1,
	SMC_NETLINK_GET_LGR_SMCR,
	SMC_NETLINK_GET_LINK_SMCR,
	SMC_NETLINK_GET_LGR_SMCD,
	SMC_NETLINK_GET_DEV_SMCD,
	SMC_NETLINK_GET_DEV_SMCR,
	SMC_NETLINK_GET_STATS,
	SMC_NETLINK_GET_FBACK_STATS,
	SMC_NETLINK_DUMP_UEID,
	SMC_NETLINK_ADD_UEID,
	SMC_NETLINK_REMOVE_UEID,
	SMC_NETLINK_FLUSH_UEID,
	SMC_NETLINK_DUMP_SEID,
	SMC_NETLINK_ENABLE_SEID,
	SMC_NETLINK_DISABLE_SEID,
};

/* SMC_GENL_FAMILY top level attributes */
enum {
	SMC_GEN_UNSPEC,
	SMC_GEN_SYS_INFO,		/* nest */
	SMC_GEN_LGR_SMCR,		/* nest */
	SMC_GEN_LINK_SMCR,		/* nest */
	SMC_GEN_LGR_SMCD,		/* nest */
	SMC_GEN_DEV_SMCD,		/* nest */
	SMC_GEN_DEV_SMCR,		/* nest */
	SMC_GEN_STATS,			/* nest */
	SMC_GEN_FBACK_STATS,		/* nest */
	__SMC_GEN_MAX,
	SMC_GEN_MAX = __SMC_GEN_MAX - 1
};

/* SMC_GEN_SYS_INFO attributes */
enum {
	SMC_NLA_SYS_UNSPEC,
	SMC_NLA_SYS_VER,		/* u8 */
	SMC_NLA_SYS_REL,		/* u8 */
	SMC_NLA_SYS_IS_ISM_V2,		/* u8 */
	SMC_NLA_SYS_LOCAL_HOST,		/* string */
	SMC_NLA_SYS_SEID,		/* string */
	SMC_NLA_SYS_IS_SMCR_V2,		/* u8 */
	__SMC_NLA_SYS_MAX,
	SMC_NLA_SYS_MAX = __SMC_NLA_SYS_MAX - 1
};

/* SMC_NLA_LGR_D_V2_COMMON and SMC_NLA_LGR_R_V2_COMMON nested attributes */
enum {
	SMC_NLA_LGR_V2_VER,		/* u8 */
	SMC_NLA_LGR_V2_REL,		/* u8 */
	SMC_NLA_LGR_V2_OS,		/* u8 */
	SMC_NLA_LGR_V2_NEG_EID,		/* string */
	SMC_NLA_LGR_V2_PEER_HOST,	/* string */
	__SMC_NLA_LGR_V2_MAX,
	SMC_NLA_LGR_V2_MAX = __SMC_NLA_LGR_V2_MAX - 1
};

/* SMC_NLA_LGR_R_V2 nested attributes */
enum {
	SMC_NLA_LGR_R_V2_UNSPEC,
	SMC_NLA_LGR_R_V2_DIRECT,	/* u8 */
	__SMC_NLA_LGR_R_V2_MAX,
	SMC_NLA_LGR_R_V2_MAX = __SMC_NLA_LGR_R_V2_MAX - 1
};

/* SMC_GEN_LGR_SMCR attributes */
enum {
	SMC_NLA_LGR_R_UNSPEC,
	SMC_NLA_LGR_R_ID,		/* u32 */
	SMC_NLA_LGR_R_ROLE,		/* u8 */
	SMC_NLA_LGR_R_TYPE,		/* u8 */
	SMC_NLA_LGR_R_PNETID,		/* string */
	SMC_NLA_LGR_R_VLAN_ID,		/* u8 */
	SMC_NLA_LGR_R_CONNS_NUM,	/* u32 */
	SMC_NLA_LGR_R_V2_COMMON,	/* nest */
	SMC_NLA_LGR_R_V2,		/* nest */
	__SMC_NLA_LGR_R_MAX,
	SMC_NLA_LGR_R_MAX = __SMC_NLA_LGR_R_MAX - 1
};

/* SMC_GEN_LINK_SMCR attributes */
enum {
	SMC_NLA_LINK_UNSPEC,
	SMC_NLA_LINK_ID,		/* u8 */
	SMC_NLA_LINK_IB_DEV,		/* string */
	SMC_NLA_LINK_IB_PORT,		/* u8 */
	SMC_NLA_LINK_GID,		/* string */
	SMC_NLA_LINK_PEER_GID,		/* string */
	SMC_NLA_LINK_CONN_CNT,		/* u32 */
	SMC_NLA_LINK_NET_DEV,		/* string */
	SMC_NLA_LINK_UID,		/* u32 */
	SMC_NLA_LINK_PEER_UID,		/* u32 */
	SMC_NLA_LINK_STATE,		/* u32 */
	__SMC_NLA_LINK_MAX,
	SMC_NLA_LINK_MAX = __SMC_NLA_LINK_MAX - 1
};

/* SMC_GEN_LGR_SMCD attributes */
enum {
	SMC_NLA_LGR_D_UNSPEC,
	SMC_NLA_LGR_D_ID,		/* u32 */
	SMC_NLA_LGR_D_GID,		/* u64 */
	SMC_NLA_LGR_D_PEER_GID,		/* u64 */
	SMC_NLA_LGR_D_VLAN_ID,		/* u8 */
	SMC_NLA_LGR_D_CONNS_NUM,	/* u32 */
	SMC_NLA_LGR_D_PNETID,		/* string */
	SMC_NLA_LGR_D_CHID,		/* u16 */
	SMC_NLA_LGR_D_PAD,		/* flag */
	SMC_NLA_LGR_D_V2_COMMON,	/* nest */
	__SMC_NLA_LGR_D_MAX,
	SMC_NLA_LGR_D_MAX = __SMC_NLA_LGR_D_MAX - 1
};

/* SMC_NLA_DEV_PORT nested attributes */
enum {
	SMC_NLA_DEV_PORT_UNSPEC,
	SMC_NLA_DEV_PORT_PNET_USR,	/* u8 */
	SMC_NLA_DEV_PORT_PNETID,	/* string */
	SMC_NLA_DEV_PORT_NETDEV,	/* string */
	SMC_NLA_DEV_PORT_STATE,		/* u8 */
	SMC_NLA_DEV_PORT_VALID,		/* u8 */
	SMC_NLA_DEV_PORT_LNK_CNT,	/* u32 */
	__SMC_NLA_DEV_PORT_MAX,
	SMC_NLA_DEV_PORT_MAX = __SMC_NLA_DEV_PORT_MAX - 1
};

/* SMC_GEN_DEV_SMCD and SMC_GEN_DEV_SMCR attributes */
enum {
	SMC_NLA_DEV_UNSPEC,
	SMC_NLA_DEV_USE_CNT,		/* u32 */
	SMC_NLA_DEV_IS_CRIT,		/* u8 */
	SMC_NLA_DEV_PCI_FID,		/* u32 */
	SMC_NLA_DEV_PCI_CHID,		/* u16 */
	SMC_NLA_DEV_PCI_VENDOR,		/* u16 */
	SMC_NLA_DEV_PCI_DEVICE,		/* u16 */
	SMC_NLA_DEV_PCI_ID,		/* string */
	SMC_NLA_DEV_PORT,		/* nest */
	SMC_NLA_DEV_PORT2,		/* nest */
	SMC_NLA_DEV_IB_NAME,		/* string */
	__SMC_NLA_DEV_MAX,
	SMC_NLA_DEV_MAX = __SMC_NLA_DEV_MAX - 1
};

/* SMC_NLA_STATS_T_TX(RX)_RMB_SIZE nested attributes */
/* SMC_NLA_STATS_TX(RX)PLOAD_SIZE nested attributes */
enum {
	SMC_NLA_STATS_PLOAD_PAD,
	SMC_NLA_STATS_PLOAD_8K,		/* u64 */
	SMC_NLA_STATS_PLOAD_16K,	/* u64 */
	SMC_NLA_STATS_PLOAD_32K,	/* u64 */
	SMC_NLA_STATS_PLOAD_64K,	/* u64 */
	SMC_NLA_STATS_PLOAD_128K,	/* u64 */
	SMC_NLA_STATS_PLOAD_256K,	/* u64 */
	SMC_NLA_STATS_PLOAD_512K,	/* u64 */
	SMC_NLA_STATS_PLOAD_1024K,	/* u64 */
	SMC_NLA_STATS_PLOAD_G_1024K,	/* u64 */
	__SMC_NLA_STATS_PLOAD_MAX,
	SMC_NLA_STATS_PLOAD_MAX = __SMC_NLA_STATS_PLOAD_MAX - 1
};

/* SMC_NLA_STATS_T_TX(RX)_RMB_STATS nested attributes */
enum {
	SMC_NLA_STATS_RMB_PAD,
	SMC_NLA_STATS_RMB_SIZE_SM_PEER_CNT,	/* u64 */
	SMC_NLA_STATS_RMB_SIZE_SM_CNT,		/* u64 */
	SMC_NLA_STATS_RMB_FULL_PEER_CNT,	/* u64 */
	SMC_NLA_STATS_RMB_FULL_CNT,		/* u64 */
	SMC_NLA_STATS_RMB_REUSE_CNT,		/* u64 */
	SMC_NLA_STATS_RMB_ALLOC_CNT,		/* u64 */
	SMC_NLA_STATS_RMB_DGRADE_CNT,		/* u64 */
	__SMC_NLA_STATS_RMB_MAX,
	SMC_NLA_STATS_RMB_MAX = __SMC_NLA_STATS_RMB_MAX - 1
};

/* SMC_NLA_STATS_SMCD_TECH and _SMCR_TECH nested attributes */
enum {
	SMC_NLA_STATS_T_PAD,
	SMC_NLA_STATS_T_TX_RMB_SIZE,	/* nest */
	SMC_NLA_STATS_T_RX_RMB_SIZE,	/* nest */
	SMC_NLA_STATS_T_TXPLOAD_SIZE,	/* nest */
	SMC_NLA_STATS_T_RXPLOAD_SIZE,	/* nest */
	SMC_NLA_STATS_T_TX_RMB_STATS,	/* nest */
	SMC_NLA_STATS_T_RX_RMB_STATS,	/* nest */
	SMC_NLA_STATS_T_CLNT_V1_SUCC,	/* u64 */
	SMC_NLA_STATS_T_CLNT_V2_SUCC,	/* u64 */
	SMC_NLA_STATS_T_SRV_V1_SUCC,	/* u64 */
	SMC_NLA_STATS_T_SRV_V2_SUCC,	/* u64 */
	SMC_NLA_STATS_T_SENDPAGE_CNT,	/* u64 */
	SMC_NLA_STATS_T_SPLICE_CNT,	/* u64 */
	SMC_NLA_STATS_T_CORK_CNT,	/* u64 */
	SMC_NLA_STATS_T_NDLY_CNT,	/* u64 */
	SMC_NLA_STATS_T_URG_DATA_CNT,	/* u64 */
	SMC_NLA_STATS_T_RX_BYTES,	/* u64 */
	SMC_NLA_STATS_T_TX_BYTES,	/* u64 */
	SMC_NLA_STATS_T_RX_CNT,		/* u64 */
	SMC_NLA_STATS_T_TX_CNT,		/* u64 */
	__SMC_NLA_STATS_T_MAX,
	SMC_NLA_STATS_T_MAX = __SMC_NLA_STATS_T_MAX - 1
};

/* SMC_GEN_STATS attributes */
enum {
	SMC_NLA_STATS_PAD,
	SMC_NLA_STATS_SMCD_TECH,	/* nest */
	SMC_NLA_STATS_SMCR_TECH,	/* nest */
	SMC_NLA_STATS_CLNT_HS_ERR_CNT,	/* u64 */
	SMC_NLA_STATS_SRV_HS_ERR_CNT,	/* u64 */
	__SMC_NLA_STATS_MAX,
	SMC_NLA_STATS_MAX = __SMC_NLA_STATS_MAX - 1
};

/* SMC_GEN_FBACK_STATS attributes */
enum {
	SMC_NLA_FBACK_STATS_PAD,
	SMC_NLA_FBACK_STATS_TYPE,	/* u8 */
	SMC_NLA_FBACK_STATS_SRV_CNT,	/* u64 */
	SMC_NLA_FBACK_STATS_CLNT_CNT,	/* u64 */
	SMC_NLA_FBACK_STATS_RSN_CODE,	/* u32 */
	SMC_NLA_FBACK_STATS_RSN_CNT,	/* u16 */
	__SMC_NLA_FBACK_STATS_MAX,
	SMC_NLA_FBACK_STATS_MAX = __SMC_NLA_FBACK_STATS_MAX - 1
};

/* SMC_NETLINK_UEID attributes */
enum {
	SMC_NLA_EID_TABLE_UNSPEC,
	SMC_NLA_EID_TABLE_ENTRY,	/* string */
	__SMC_NLA_EID_TABLE_MAX,
	SMC_NLA_EID_TABLE_MAX = __SMC_NLA_EID_TABLE_MAX - 1
};

/* SMC_NETLINK_SEID attributes */
enum {
	SMC_NLA_SEID_UNSPEC,
	SMC_NLA_SEID_ENTRY,	/* string */
	SMC_NLA_SEID_ENABLED,	/* u8 */
	__SMC_NLA_SEID_TABLE_MAX,
	SMC_NLA_SEID_TABLE_MAX = __SMC_NLA_SEID_TABLE_MAX - 1
};

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
	SMC_DIAG_GET_SYS_INFO,
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

/* SMC_DIAG_GET_SYS_INFO command extensions */
enum {
	SMC_DIAG_SYS_INFO = 1,
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

struct smc_v2_lgr_info {
	__u8		v2_lgr_info_received;
	__u8		smc_version;
	__u8		peer_smc_release;
	__u8		peer_os;	/* peer operating system */
	__u8		negotiated_eid[SMC_MAX_EID_LEN + 1];
	__u8		peer_hostname[SMC_MAX_HOSTNAME_LEN + 1];
	/* SMC-R v2 specific */
	__u8		smcr_direct;
};

/* unused
struct smc_system_info {
	__u8		smc_version;
	__u8		smc_release;
	__u8		ueid_count;
	__u8		smc_ism_is_v2;
	__u32		reserved;
	__u8		local_hostname[SMC_MAX_HOSTNAME_LEN];
	__u8		seid[SMC_MAX_EID_LEN];
	__u8		ueid[SMC_MAX_UEID][SMC_MAX_EID_LEN];
};
*/

/* SMC_DIAG_LINKINFO */

struct smc_diag_linkinfo {
	__u8 link_id;			/* link identifier */
	__u8 ibname[IB_DEVICE_NAME_MAX]; /* name of the RDMA device */
	__u8 ibport;			/* RDMA device port number */
	__u8 gid[40];			/* local GID */
	__u8 peer_gid[40];		/* peer GID */
};

struct smc_diag_linkinfo_v2 {
	struct smc_diag_linkinfo v1;
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
};

struct smcd_diag_dmbinfo_v2 {
	struct smcd_diag_dmbinfo v1;
	__u8		pnet_id[SMC_MAX_PNETID_LEN];
	__u32		conns_num;
	__u16		chid;
	__u8		vlan_id;
	struct smc_v2_lgr_info v2_lgr_info;
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
	__u32		lnk_cnt_by_port[SMC_MAX_PORTS]; /* # lnks per port */
};


struct smc_diag_lgr {
	__u8		lgr_id[SMC_LGR_ID_SIZE];
	__u8		lgr_role;
	__u8		lgr_type;
	__u8		pnet_id[SMC_MAX_PNETID_LEN];
	__u8		vlan_id;
	__u32		conns_num;
	struct smc_v2_lgr_info v2_lgr_info;
};
#endif /* SMCTOOLS_COMMON_H */
