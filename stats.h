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
	__u16	count;
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
