/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2021
 *
 * Author(s): Guvenc Gulce <guvenc@linux.ibm.com>
 *
 * Userspace program for SMC Information display
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef SMC_SYSTEM_H_
#define SMC_SYSTEM_H_

#define SMC_CLC_DECL_MEM	0x01010000  /* insufficient memory resources  */
#define SMC_CLC_DECL_TIMEOUT_CL	0x02010000  /* timeout w4 QP confirm link     */
#define SMC_CLC_DECL_TIMEOUT_AL	0x02020000  /* timeout w4 QP add link	      */
#define SMC_CLC_DECL_CNFERR	0x03000000  /* configuration error            */
#define SMC_CLC_DECL_PEERNOSMC	0x03010000  /* peer did not indicate SMC      */
#define SMC_CLC_DECL_IPSEC	0x03020000  /* IPsec usage		      */
#define SMC_CLC_DECL_NOSMCDEV	0x03030000  /* no SMC device found (R or D)   */
#define SMC_CLC_DECL_NOSMCDDEV	0x03030001  /* no SMC-D device found	      */
#define SMC_CLC_DECL_NOSMCRDEV	0x03030002  /* no SMC-R device found	      */
#define SMC_CLC_DECL_NOISM2SUPP	0x03030003  /* hardware has no ISMv2 support  */
#define SMC_CLC_DECL_NOV2EXT	0x03030004  /* peer sent no clc v2 extension  */
#define SMC_CLC_DECL_NOV2DEXT	0x03030005  /* peer sent no clc SMC-Dv2 ext.  */
#define SMC_CLC_DECL_NOSEID	0x03030006  /* peer sent no SEID	      */
#define SMC_CLC_DECL_NOSMCD2DEV	0x03030007  /* no SMC-Dv2 device found	      */
#define SMC_CLC_DECL_MODEUNSUPP	0x03040000  /* smc modes do not match (R or D)*/
#define SMC_CLC_DECL_RMBE_EC	0x03050000  /* peer has eyecatcher in RMBE    */
#define SMC_CLC_DECL_OPTUNSUPP	0x03060000  /* fastopen sockopt not supported */
#define SMC_CLC_DECL_DIFFPREFIX	0x03070000  /* IP prefix / subnet mismatch    */
#define SMC_CLC_DECL_GETVLANERR	0x03080000  /* err to get vlan id of ip device*/
#define SMC_CLC_DECL_ISMVLANERR	0x03090000  /* err to reg vlan id on ism dev  */
#define SMC_CLC_DECL_NOACTLINK	0x030a0000  /* no active smc-r link in lgr    */
#define SMC_CLC_DECL_NOSRVLINK	0x030b0000  /* SMC-R link from srv not found  */
#define SMC_CLC_DECL_VERSMISMAT	0x030c0000  /* SMC version mismatch	      */
#define SMC_CLC_DECL_MAX_DMB	0x030d0000  /* SMC-D DMB limit exceeded       */
#define SMC_CLC_DECL_SYNCERR	0x04000000  /* synchronization error          */
#define SMC_CLC_DECL_PEERDECL	0x05000000  /* peer declined during handshake */
#define SMC_CLC_DECL_INTERR	0x09990000  /* internal error		      */
#define SMC_CLC_DECL_ERR_RTOK	0x09990001  /*	 rtoken handling failed       */
#define SMC_CLC_DECL_ERR_RDYLNK	0x09990002  /*	 ib ready link failed	      */
#define SMC_CLC_DECL_ERR_REGRMB	0x09990003  /*	 reg rmb failed		      */

#define SMC_TYPE_R	0
#define SMC_TYPE_D	1
#define SMC_SERVER	1
#define SMC_CLIENT	0

#define SMC_MAX_FBACK_RSN_CNT 30

enum {
	SMC_BUF_8K,
	SMC_BUF_16K,
	SMC_BUF_32K,
	SMC_BUF_64K,
	SMC_BUF_128K,
	SMC_BUF_256K,
	SMC_BUF_512K,
	SMC_BUF_1024K,
	SMC_BUF_G_1024K,
	SMC_BUF_MAX,
};

struct smc_stats_fback {
	int	fback_code;
	int	count;
};

struct smc_stats_rsn {
	struct	smc_stats_fback srv[SMC_MAX_FBACK_RSN_CNT];
	struct	smc_stats_fback clnt[SMC_MAX_FBACK_RSN_CNT];
	__u64 srv_fback_cnt;
	__u64 clnt_fback_cnt;
};

struct smc_stats_rmbcnt {
	__u64	buf_size_small_peer_cnt;
	__u64	buf_size_small_cnt;
	__u64	buf_full_peer_cnt;
	__u64	buf_full_cnt;
	__u64	reuse_cnt;
	__u64	alloc_cnt;
	__u64	dgrade_cnt;
};

struct smc_stats_memsize {
	__u64	buf[SMC_BUF_MAX];
};

struct smc_stats_tech {
	struct smc_stats_memsize tx_rmbsize;
	struct smc_stats_memsize rx_rmbsize;
	struct smc_stats_memsize tx_pd;
	struct smc_stats_memsize rx_pd;
	struct smc_stats_rmbcnt rmb_tx;
	struct smc_stats_rmbcnt rmb_rx;
	__u64	clnt_v1_succ_cnt;
	__u64	clnt_v2_succ_cnt;
	__u64	srv_v1_succ_cnt;
	__u64	srv_v2_succ_cnt;
	__u64	sendpage_cnt;
	__u64	urg_data_cnt;
	__u64	splice_cnt;
	__u64	cork_cnt;
	__u64	ndly_cnt;
	__u64	rx_bytes;
	__u64	tx_bytes;
	__u64	rx_cnt;
	__u64	tx_cnt;
};

struct smc_stats {
	struct smc_stats_tech	smc[2];
	__u64	clnt_hshake_err_cnt;
	__u64	srv_hshake_err_cnt;
};

int invoke_stats(int argc, char **argv, int detail_level);

#endif /* SMC_SYSTEM_H_ */
