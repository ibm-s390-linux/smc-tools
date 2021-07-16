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
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>

#include "smctools_common.h"
#include "util.h"
#include "libnetlink.h"
#include "stats.h"

#if defined(SMCD)
static int is_smcd = 1;
#else
static int is_smcd = 0;
#endif
static int d_level = 0;
static int is_abs = 0;

static int show_cmd = 0;
static int reset_cmd = 0;
static int json_cmd = 0;
static int cache_file_exists = 0;

struct smc_stats smc_stat;	/* kernel values, might contain merged values */
struct smc_stats smc_stat_c;	/* cache file values */
struct smc_stats smc_stat_org;	/* original kernel values */
struct smc_stats_rsn smc_rsn;
struct smc_stats_rsn smc_rsn_c;
struct smc_stats_rsn smc_rsn_org;
FILE *cache_fp = NULL;
char *cache_file_path;

static char* j_output[63] = {"SMC_INT_TX_BUF_8K", "SMC_INT_TX_BUF_16K", "SMC_INT_TX_BUF_32K", "SMC_INT_TX_BUF_64K", "SMC_INT_TX_BUF_128K",
			    "SMC_INT_TX_BUF_256K", "SMC_INT_TX_BUF_512K", "SMC_INT_TX_BUF_1024K", "SMC_INT_TX_BUF_G_1024K",
			    "SMC_INT_RX_BUF_8K", "SMC_INT_RX_BUF_16K", "SMC_INT_RX_BUF_32K", "SMC_INT_RX_BUF_64K", "SMC_INT_RX_BUF_128K",
			    "SMC_INT_RX_BUF_256K", "SMC_INT_RX_BUF_512K", "SMC_INT_RX_BUF_1024K", "SMC_INT_RX_BUF_G_1024K",
			    "SMC_USR_TX_BUF_8K", "SMC_USR_TX_BUF_16K", "SMC_USR_TX_BUF_32K", "SMC_USR_TX_BUF_64K", "SMC_USR_TX_BUF_128K",
			    "SMC_USR_TX_BUF_256K", "SMC_USR_TX_BUF_512K", "SMC_USR_TX_BUF_1024K", "SMC_USR_TX_BUF_G_1024K",
			    "SMC_USR_RX_BUF_8K", "SMC_USR_RX_BUF_16K", "SMC_USR_RX_BUF_32K", "SMC_USR_RX_BUF_64K", "SMC_USR_RX_BUF_128K",
			    "SMC_USR_RX_BUF_256K", "SMC_USR_RX_BUF_512K", "SMC_USR_RX_BUF_1024K", "SMC_USR_RX_BUF_G_1024K",
			    "SMC_INT_TX_BUF_SIZE_SM_PEER_CNT", "SMC_INT_TX_BUF_SIZE_SM_CNT", "SMC_INT_TX_BUF_FULL_PEER_CNT",
			    "SMC_INT_TX_BUF_FULL_CNT", "SMC_INT_TX_BUF_REUSE_CNT", "SMC_INT_TX_BUF_ALLOC_CNT", "SMC_INT_TX_BUF_DGRADE_CNT",
			    "SMC_INT_RX_BUF_SIZE_SM_PEER_CNT", "SMC_INT_RX_BUF_SIZE_SM_CNT", "SMC_INT_RX_BUF_FULL_PEER_CNT",
			    "SMC_INT_RX_BUF_FULL_CNT", "SMC_INT_RX_BUF_REUSE_CNT", "SMC_INT_RX_BUF_ALLOC_CNT", "SMC_INT_RX_BUF_DGRADE_CNT",
			    "SMC_CLNT_V1_SUCC_CNT", "SMC_CLNT_V2_SUCC_CNT", "SMC_SRV_V1_SUCC_CNT", "SMC_SRV_V2_SUCC_CNT",
			    "SMC_SENDPAGE_CNT", "SMC_URG_DATA_CNT", "SMC_SPLICE_CNT", "SMC_CORK_CNT", "SMC_NDLY_CNT",
			    "SMC_RX_BYTES", "SMC_TX_BYTES", "SMC_RX_CNT", "SMC_TX_CNT"
};

static struct nla_policy smc_gen_stats_policy[SMC_NLA_STATS_MAX + 1] = {
	[SMC_NLA_STATS_PAD]		= { .type = NLA_UNSPEC },
	[SMC_NLA_STATS_SMCD_TECH]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_SMCR_TECH]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_CLNT_HS_ERR_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_SRV_HS_ERR_CNT]	= { .type = NLA_U64 },
};

static struct nla_policy smc_gen_stats_fback_policy[SMC_NLA_FBACK_STATS_MAX + 1] = {
	[SMC_NLA_FBACK_STATS_PAD]	= { .type = NLA_UNSPEC },
	[SMC_NLA_FBACK_STATS_TYPE]	= { .type = NLA_U8 },
	[SMC_NLA_FBACK_STATS_SRV_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_FBACK_STATS_CLNT_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_FBACK_STATS_RSN_CODE]	= { .type = NLA_U32 },
	[SMC_NLA_FBACK_STATS_RSN_CNT]	= { .type = NLA_U16 },
};

static struct nla_policy smc_gen_stats_tech_policy[SMC_NLA_STATS_T_MAX + 1] = {
	[SMC_NLA_STATS_T_PAD]		= { .type = NLA_UNSPEC },
	[SMC_NLA_STATS_T_TX_RMB_SIZE]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_T_RX_RMB_SIZE]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_T_TXPLOAD_SIZE]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_T_RXPLOAD_SIZE]	= { .type = NLA_NESTED },
	[SMC_NLA_STATS_T_CLNT_V1_SUCC]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_CLNT_V2_SUCC]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_SRV_V1_SUCC]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_SRV_V2_SUCC]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_SENDPAGE_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_SPLICE_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_CORK_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_URG_DATA_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_NDLY_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_RX_BYTES]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_TX_BYTES]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_RX_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_T_TX_CNT]	= { .type = NLA_U64 },
};

static struct nla_policy smc_gen_stats_rmb_policy[SMC_NLA_STATS_RMB_MAX + 1] = {
	[SMC_NLA_STATS_RMB_PAD]			= { .type = NLA_UNSPEC },
	[SMC_NLA_STATS_RMB_SIZE_SM_PEER_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_SIZE_SM_CNT]		= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_FULL_PEER_CNT]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_FULL_CNT]		= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_REUSE_CNT]		= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_ALLOC_CNT]		= { .type = NLA_U64 },
	[SMC_NLA_STATS_RMB_DGRADE_CNT]		= { .type = NLA_U64 },
};

static struct nla_policy smc_gen_stats_pload_policy[SMC_NLA_STATS_PLOAD_MAX + 1] = {
	[SMC_NLA_STATS_PLOAD_PAD]	= { .type = NLA_UNSPEC },
	[SMC_NLA_STATS_PLOAD_8K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_16K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_32K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_64K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_128K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_256K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_512K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_1024K]	= { .type = NLA_U64 },
	[SMC_NLA_STATS_PLOAD_G_1024K]	= { .type = NLA_U64 },
};

static void usage(void)
{
	fprintf(stderr,
#if defined(SMCD)
		"Usage: smcd stats [show | reset]\n"
#elif defined(SMCR)
		"Usage: smcr stats [show | reset]\n"
#else
		"Usage: smc stats [show | reset]\n"
#endif
	);
	exit(-1);
}

static char* get_fbackstr(int code)
{
	char* str;

	switch (code) {
		case SMC_CLC_DECL_PEERNOSMC:
			str = "PEER_NO_SMC";
			break;
		case SMC_CLC_DECL_MEM:
			str = "MEMORY";
			break;
		case SMC_CLC_DECL_TIMEOUT_CL:
			str = "TIMEOUT_CL";
			break;
		case SMC_CLC_DECL_TIMEOUT_AL:
			str = "TIMEOUT_AL";
			break;
		case SMC_CLC_DECL_CNFERR:
			str = "CNF_ERR";
			break;
		case SMC_CLC_DECL_IPSEC:
			str = "IPSEC";
			break;
		case SMC_CLC_DECL_NOSMCDEV:
			str = "NOSMCDEV";
			break;
		case SMC_CLC_DECL_NOSMCDDEV:
			str = "NOSMCDDEV";
			break;
		case SMC_CLC_DECL_NOSMCRDEV:
			str = "NOSMCRDEV";
			break;
		case SMC_CLC_DECL_NOISM2SUPP:
			str = "NOISM2SUPP";
			break;
		case SMC_CLC_DECL_NOV2EXT:
			str = "NOV2EXT";
			break;
		case SMC_CLC_DECL_PEERDECL:
			str = "PEERDECL";
			break;
		case SMC_CLC_DECL_SYNCERR:
			str = "SYNCERR";
			break;
		case SMC_CLC_DECL_MAX_DMB:
			str = "MAX_DMB";
			break;
		case SMC_CLC_DECL_VERSMISMAT:
			str = "VERSMISMAT";
			break;
		case SMC_CLC_DECL_NOSRVLINK:
			str = "NOSRVLINK";
			break;
		case SMC_CLC_DECL_NOSEID:
			str = "NOSEID";
			break;
		case SMC_CLC_DECL_NOSMCD2DEV:
			str = "NOSMCD2DEV";
			break;
		case SMC_CLC_DECL_MODEUNSUPP:
			str = "MODEUNSUPP";
			break;
		case SMC_CLC_DECL_RMBE_EC:
			str = "RMBE_EC";
			break;
		case SMC_CLC_DECL_OPTUNSUPP:
			str = "OPTUNSUPP";
			break;
		case SMC_CLC_DECL_DIFFPREFIX:
			str = "DIFFPREFIX";
			break;
		case SMC_CLC_DECL_GETVLANERR:
			str = "GETVLANERR";
			break;
		case SMC_CLC_DECL_ISMVLANERR:
			str = "ISMVLANERR";
			break;
		case SMC_CLC_DECL_NOACTLINK:
			str = "NOACTLINK";
			break;
		case SMC_CLC_DECL_NOV2DEXT:
			str = "NOV2DEXT";
			break;
		default:
			str = "[unknown]";
			break;
	}
	return str;
}

static void bubble_sort(struct smc_stats_fback *fback)
{
	struct smc_stats_fback temp;
	int i, j;

	for (i = 0; i < SMC_MAX_FBACK_RSN_CNT; i++) {
		for (j = 0; j < SMC_MAX_FBACK_RSN_CNT - 1; j++) {
			if (fback[j + 1].count > fback[j].count) {
				temp = fback[j];
				fback[j] = fback[j + 1];
				fback[j + 1] = temp;
			}
		}
	}
}

static void print_fback_details(struct smc_stats_fback *fback, int is_srv)
{
	int caption_printed = 0;
	char *fback_str = NULL;
	int i, count = 0;

	bubble_sort(fback);
	for (i = 0; i < SMC_MAX_FBACK_RSN_CNT; i++) {
		if (fback[i].fback_code != 0) {
			if (!caption_printed) {
				caption_printed = 1;
				if (is_srv)
					printf("    Server\n");
				else
					printf("    Client\n");
			}
			fback_str = get_fbackstr(fback[i].fback_code);
			printf("     %-12s            %12d\n", fback_str, fback[i].count);
			count++;
			if (count == 3)
				break;
		}
	}
}

static void print_fbackstr()
{
	struct	smc_stats_fback *server, *client;

	server = smc_rsn.srv;
	client = smc_rsn.clnt;

	print_fback_details(server, 1);
	print_fback_details(client, 0);
}

static void fillbuffer(struct smc_stats_memsize *mem, char buf[][7])
{
	get_abbreviated(mem->buf[SMC_BUF_8K], 6, buf[SMC_BUF_8K]);
	get_abbreviated(mem->buf[SMC_BUF_16K], 6, buf[SMC_BUF_16K]);
	get_abbreviated(mem->buf[SMC_BUF_32K], 6, buf[SMC_BUF_32K]);
	get_abbreviated(mem->buf[SMC_BUF_64K], 6, buf[SMC_BUF_64K]);
	get_abbreviated(mem->buf[SMC_BUF_128K], 6, buf[SMC_BUF_128K]);
	get_abbreviated(mem->buf[SMC_BUF_256K], 6, buf[SMC_BUF_256K]);
	get_abbreviated(mem->buf[SMC_BUF_512K], 6, buf[SMC_BUF_512K]);
	get_abbreviated(mem->buf[SMC_BUF_G_1024K] + mem->buf[SMC_BUF_1024K], 6,
			buf[SMC_BUF_1024K]);
}

static void print_as_json()
{
	int size, i;
	__u64 *src;

	size = sizeof(struct smc_stats_tech) / sizeof(__u64);
	if (is_smcd) {
		src = (__u64 *)&smc_stat.smc[SMC_TYPE_D];
		printf("{\"SMCD\": {");
	} else {
		src = (__u64 *)&smc_stat.smc[SMC_TYPE_R];
		printf("{\"SMCR\": {");
	}
	for (i = 0; i < size; i++) {
		printf("\"%s\":%llu",j_output[i] ,*src);
		if (i != size - 1)
			printf(",");
		src++;
	}
	printf("}}\n");
}

static void print_as_text()
{
	__u64 smc_conn_cnt = 0, special_calls = 0, total_req_cn = 0;
	__u64 total_conn = 0, fback_count = 0, hshake_err_cnt = 0;
	float buf_small = 0, buf_small_r = 0, buf_rx_full = 0;
	__u64 smc_c_cnt_v1 = 0, smc_c_cnt_v2 = 0;
	float buf_full = 0, buf_full_r = 0;
	struct smc_stats_tech *tech;
	float avg_req_p_conn = 0;
	char buf[SMC_BUF_MAX][7];
	char temp_str[7];
	int tech_type;

	if (is_smcd) {
		printf("SMC-D Connections Summary\n");
		tech_type = SMC_TYPE_D;
	} else {
		printf("SMC-R Connections Summary\n");
		tech_type = SMC_TYPE_R;
	}
	tech = &smc_stat.smc[tech_type];

	smc_c_cnt_v1 = tech->clnt_v1_succ_cnt + tech->srv_v1_succ_cnt;
	smc_c_cnt_v2 = tech->clnt_v2_succ_cnt + tech->srv_v2_succ_cnt;
	smc_conn_cnt += tech->clnt_v1_succ_cnt;
	smc_conn_cnt += tech->clnt_v2_succ_cnt;
	smc_conn_cnt += tech->srv_v1_succ_cnt;
	smc_conn_cnt += tech->srv_v2_succ_cnt;
	total_conn += smc_conn_cnt;
	hshake_err_cnt += smc_stat.clnt_hshake_err_cnt;
	hshake_err_cnt += smc_stat.srv_hshake_err_cnt;
	total_conn += hshake_err_cnt;
	fback_count += smc_rsn.clnt_fback_cnt;
	fback_count += smc_rsn.srv_fback_cnt;
	total_conn += fback_count;
	total_req_cn = tech->rx_cnt + tech->tx_cnt;
	if (smc_conn_cnt)
		avg_req_p_conn = total_req_cn / (double)smc_conn_cnt;
	special_calls += tech->cork_cnt;
	special_calls += tech->ndly_cnt;
	special_calls += tech->sendpage_cnt;
	special_calls += tech->splice_cnt;
	special_calls += tech->urg_data_cnt;
	if (tech->tx_cnt) {
		buf_full = tech->rmb_tx.buf_full_cnt / (double)tech->tx_cnt * 100;
		buf_full_r = tech->rmb_tx.buf_full_peer_cnt / (double)tech->tx_cnt * 100;
		buf_small = tech->rmb_tx.buf_size_small_cnt / (double)tech->tx_cnt * 100;
		buf_small_r = tech->rmb_tx.buf_size_small_peer_cnt / (double)tech->tx_cnt * 100;
	}
	if (tech->rx_cnt)
		buf_rx_full = tech->rmb_rx.buf_full_cnt / (double)tech->rx_cnt * 100;

	printf("  Total connections handled  %12llu\n", total_conn);
	if (d_level) {
		printf("  SMC connections            %12llu (client %llu, server %llu)\n",
			smc_conn_cnt, tech->clnt_v1_succ_cnt + tech->clnt_v2_succ_cnt,
			tech->srv_v1_succ_cnt + tech->srv_v2_succ_cnt);
		printf("    v1                       %12llu\n", smc_c_cnt_v1);
		printf("    v2                       %12llu\n", smc_c_cnt_v2);
	} else {
		printf("  SMC connections            %12llu\n", smc_conn_cnt);
	}
	if (d_level) {
		printf("  Handshake errors           %12llu (client %llu, server %llu)\n",
			hshake_err_cnt, smc_stat.clnt_hshake_err_cnt, smc_stat.srv_hshake_err_cnt);
	} else {
		printf("  Handshake errors           %12llu\n", hshake_err_cnt);
	}
	printf("  Avg requests per SMC conn  %14.1f\n", avg_req_p_conn);
	if (d_level) {
		printf("  TCP fallback               %12llu (client %llu, server %llu)\n",
			fback_count, smc_rsn.clnt_fback_cnt, smc_rsn.srv_fback_cnt);
		print_fbackstr();
	} else {
		printf("  TCP fallback               %12llu\n", fback_count);
	}
	printf("\n");
	printf("RX Stats\n");
	get_abbreviated(smc_stat.smc[tech_type].rx_bytes, 6, temp_str);
	printf("  Data transmitted (Bytes) %14llu (%s)\n",
		smc_stat.smc[tech_type].rx_bytes, temp_str);
	printf("  Total requests             %12llu\n", tech->rx_cnt);
	printf("  Buffer full                %12llu (%.2f%%)\n", tech->rmb_rx.buf_full_cnt,
			buf_rx_full);
	if (d_level) {
		printf("  Buffer downgrades          %12llu\n", tech->rmb_rx.dgrade_cnt);
		printf("  Buffer reuses              %12llu\n", tech->rmb_rx.reuse_cnt);
	}
	fillbuffer(&tech->rx_rmbsize, buf);
	printf("            8KB    16KB    32KB    64KB   128KB   256KB   512KB  >512KB\n");
	printf("  Bufs   %6s  %6s  %6s  %6s  %6s  %6s  %6s  %6s\n",
		buf[SMC_BUF_8K], buf[SMC_BUF_16K], buf[SMC_BUF_32K], buf[SMC_BUF_64K],
		buf[SMC_BUF_128K], buf[SMC_BUF_256K], buf[SMC_BUF_512K], buf[SMC_BUF_1024K]);
	fillbuffer(&tech->rx_pd, buf);
	printf("  Reqs   %6s  %6s  %6s  %6s  %6s  %6s  %6s  %6s\n",
		buf[SMC_BUF_8K], buf[SMC_BUF_16K], buf[SMC_BUF_32K], buf[SMC_BUF_64K],
		buf[SMC_BUF_128K], buf[SMC_BUF_256K], buf[SMC_BUF_512K], buf[SMC_BUF_1024K]);
	printf("\n");
	printf("TX Stats\n");
	get_abbreviated(smc_stat.smc[tech_type].tx_bytes, 6, temp_str);
	printf("  Data transmitted (Bytes) %14llu (%s)\n",
		smc_stat.smc[tech_type].tx_bytes, temp_str);
	printf("  Total requests             %12llu\n", tech->tx_cnt);
	printf("  Buffer full                %12llu (%.2f%%)\n", tech->rmb_tx.buf_full_cnt,
			buf_full);
	printf("  Buffer full (remote)       %12llu (%.2f%%)\n", tech->rmb_tx.buf_full_peer_cnt,
			buf_full_r);
	printf("  Buffer too small           %12llu (%.2f%%)\n", tech->rmb_tx.buf_size_small_cnt,
			buf_small);
	printf("  Buffer too small (remote)  %12llu (%.2f%%)\n", tech->rmb_tx.buf_size_small_peer_cnt,
			buf_small_r);
	if (d_level) {
		printf("  Buffer downgrades          %12llu\n", tech->rmb_tx.dgrade_cnt);
		printf("  Buffer reuses              %12llu\n", tech->rmb_tx.reuse_cnt);
	}
	fillbuffer(&tech->tx_rmbsize, buf);
	printf("            8KB    16KB    32KB    64KB   128KB   256KB   512KB  >512KB\n");
	printf("  Bufs   %6s  %6s  %6s  %6s  %6s  %6s  %6s  %6s\n",
		buf[SMC_BUF_8K], buf[SMC_BUF_16K], buf[SMC_BUF_32K], buf[SMC_BUF_64K],
		buf[SMC_BUF_128K], buf[SMC_BUF_256K], buf[SMC_BUF_512K], buf[SMC_BUF_1024K]);
	fillbuffer(&tech->tx_pd, buf);
	printf("  Reqs   %6s  %6s  %6s  %6s  %6s  %6s  %6s  %6s\n",
		buf[SMC_BUF_8K], buf[SMC_BUF_16K], buf[SMC_BUF_32K], buf[SMC_BUF_64K],
		buf[SMC_BUF_128K], buf[SMC_BUF_256K], buf[SMC_BUF_512K], buf[SMC_BUF_1024K]);
	printf("\n");
	printf("Extras\n");
	printf("  Special socket calls       %12llu\n", special_calls);
	if (d_level) {
		printf("    cork                     %12llu\n", tech->cork_cnt);
		printf("    nodelay                  %12llu\n", tech->ndly_cnt);
		printf("    sendpage                 %12llu\n", tech->sendpage_cnt);
		printf("    splice                   %12llu\n", tech->splice_cnt);
		printf("    urgent data              %12llu\n", tech->urg_data_cnt);
	}
}

static int show_tech_pload_info(struct nlattr **attr, int type, int direction)
{
	struct nlattr *tech_pload_attrs[SMC_NLA_STATS_PLOAD_MAX + 1];
	struct smc_stats_memsize *tmp_memsize;
	uint64_t trgt = 0;
	int rc = NL_OK;
	int tech_type;

	if (type == SMC_NLA_STATS_SMCD_TECH)
		tech_type = SMC_TYPE_D;
	else
		tech_type = SMC_TYPE_R;

	if (direction == SMC_NLA_STATS_T_TXPLOAD_SIZE)
		tmp_memsize = &smc_stat.smc[tech_type].tx_pd;
	else if (direction == SMC_NLA_STATS_T_RXPLOAD_SIZE)
		tmp_memsize = &smc_stat.smc[tech_type].rx_pd;
	else if (direction == SMC_NLA_STATS_T_TX_RMB_SIZE)
		tmp_memsize = &smc_stat.smc[tech_type].tx_rmbsize;
	else if (direction == SMC_NLA_STATS_T_RX_RMB_SIZE)
		tmp_memsize = &smc_stat.smc[tech_type].rx_rmbsize;
	else
		return NL_STOP;

	if (nla_parse_nested(tech_pload_attrs, SMC_NLA_STATS_PLOAD_MAX,
			     attr[direction],
			     smc_gen_stats_pload_policy)) {
		fprintf(stderr, "Error: Failed to parse nested attributes: smc_gen_stats_pload_policy\n");
		return NL_STOP;
	}

	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_8K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_8K]);
		tmp_memsize->buf[SMC_BUF_8K] = trgt;
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_16K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_16K]);
		tmp_memsize->buf[SMC_BUF_16K] = trgt;
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_32K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_32K]);
		tmp_memsize->buf[SMC_BUF_32K] = trgt;
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_64K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_64K]);
		tmp_memsize->buf[SMC_BUF_64K] = trgt;
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_128K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_128K]);
		tmp_memsize->buf[SMC_BUF_128K] = trgt;
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_256K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_256K]);
		tmp_memsize->buf[SMC_BUF_256K] = trgt;
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_512K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_512K]);
		tmp_memsize->buf[SMC_BUF_512K] = trgt;
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_1024K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_1024K]);
		tmp_memsize->buf[SMC_BUF_1024K] = trgt;
	}
	if (tech_pload_attrs[SMC_NLA_STATS_PLOAD_G_1024K]) {
		trgt = nla_get_u64(tech_pload_attrs[SMC_NLA_STATS_PLOAD_G_1024K]);
		tmp_memsize->buf[SMC_BUF_G_1024K] = trgt;
	}
	return rc;
}

static int show_tech_rmb_info(struct nlattr **attr, int type, int direction)
{
	struct nlattr *tech_rmb_attrs[SMC_NLA_STATS_RMB_MAX + 1];
	struct smc_stats_rmbcnt *tmp_rmb_stats;
	uint64_t trgt = 0;
	int rc = NL_OK;
	int tech_type;

	if (type == SMC_NLA_STATS_SMCD_TECH)
		tech_type = SMC_TYPE_D;
	else
		tech_type = SMC_TYPE_R;

	if (direction == SMC_NLA_STATS_T_TX_RMB_STATS)
		tmp_rmb_stats = &smc_stat.smc[tech_type].rmb_tx;
	else
		tmp_rmb_stats = &smc_stat.smc[tech_type].rmb_rx;

	if (nla_parse_nested(tech_rmb_attrs, SMC_NLA_STATS_RMB_MAX,
			     attr[direction],
			     smc_gen_stats_rmb_policy)) {
		fprintf(stderr, "Error: Failed to parse nested attributes: smc_gen_stats_rmb_policy\n");
		return NL_STOP;
	}

	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_REUSE_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_REUSE_CNT]);
		tmp_rmb_stats->reuse_cnt = trgt;
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_ALLOC_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_ALLOC_CNT]);
		tmp_rmb_stats->alloc_cnt = trgt;
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_SIZE_SM_PEER_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_SIZE_SM_PEER_CNT]);
		tmp_rmb_stats->buf_size_small_peer_cnt = trgt;
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_SIZE_SM_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_SIZE_SM_CNT]);
		tmp_rmb_stats->buf_size_small_cnt = trgt;
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_FULL_PEER_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_FULL_PEER_CNT]);
		tmp_rmb_stats->buf_full_peer_cnt = trgt;
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_FULL_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_FULL_CNT]);
		tmp_rmb_stats->buf_full_cnt = trgt;
	}
	if (tech_rmb_attrs[SMC_NLA_STATS_RMB_DGRADE_CNT]) {
		trgt = nla_get_u64(tech_rmb_attrs[SMC_NLA_STATS_RMB_DGRADE_CNT]);
		tmp_rmb_stats->dgrade_cnt = trgt;
	}

	return rc;
}

static int fill_tech_info(struct nlattr **attr, int type)
{
	struct nlattr *tech_attrs[SMC_NLA_STATS_T_MAX + 1];
	uint64_t trgt = 0;
	int tech_type;

	if (type == SMC_NLA_STATS_SMCD_TECH)
		tech_type = SMC_TYPE_D;
	else
		tech_type = SMC_TYPE_R;

	if (nla_parse_nested(tech_attrs, SMC_NLA_STATS_T_MAX,
			     attr[type],
			     smc_gen_stats_tech_policy)) {
		fprintf(stderr, "Error: Failed to parse nested attributes: smc_gen_stats_fback_policy\n");
		return NL_STOP;
	}

	if (tech_attrs[SMC_NLA_STATS_T_SRV_V1_SUCC]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_SRV_V1_SUCC]);
		smc_stat.smc[tech_type].srv_v1_succ_cnt = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_SRV_V2_SUCC]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_SRV_V2_SUCC]);
		smc_stat.smc[tech_type].srv_v2_succ_cnt = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_CLNT_V1_SUCC]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_CLNT_V1_SUCC]);
		smc_stat.smc[tech_type].clnt_v1_succ_cnt = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_CLNT_V2_SUCC]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_CLNT_V2_SUCC]);
		smc_stat.smc[tech_type].clnt_v2_succ_cnt = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_RX_BYTES]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_RX_BYTES]);
		smc_stat.smc[tech_type].rx_bytes = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_TX_BYTES]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_TX_BYTES]);
		smc_stat.smc[tech_type].tx_bytes = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_RX_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_RX_CNT]);
		smc_stat.smc[tech_type].rx_cnt = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_TX_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_TX_CNT]);
		smc_stat.smc[tech_type].tx_cnt = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_SENDPAGE_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_SENDPAGE_CNT]);
		smc_stat.smc[tech_type].sendpage_cnt = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_SPLICE_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_SPLICE_CNT]);
		smc_stat.smc[tech_type].splice_cnt = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_CORK_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_CORK_CNT]);
		smc_stat.smc[tech_type].cork_cnt = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_NDLY_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_NDLY_CNT]);
		smc_stat.smc[tech_type].ndly_cnt = trgt;
	}
	if (tech_attrs[SMC_NLA_STATS_T_URG_DATA_CNT]) {
		trgt = nla_get_u64(tech_attrs[SMC_NLA_STATS_T_URG_DATA_CNT]);
		smc_stat.smc[tech_type].urg_data_cnt = trgt;
	}

	if (show_tech_rmb_info(tech_attrs, type, SMC_NLA_STATS_T_TX_RMB_STATS) != NL_OK)
		goto errout;
	if (show_tech_rmb_info(tech_attrs, type, SMC_NLA_STATS_T_RX_RMB_STATS) != NL_OK)
		goto errout;
	if (show_tech_pload_info(tech_attrs, type, SMC_NLA_STATS_T_TXPLOAD_SIZE) != NL_OK)
		goto errout;
	if (show_tech_pload_info(tech_attrs, type, SMC_NLA_STATS_T_RXPLOAD_SIZE) != NL_OK)
		goto errout;
	if (show_tech_pload_info(tech_attrs, type, SMC_NLA_STATS_T_TX_RMB_SIZE) != NL_OK)
		goto errout;
	if (show_tech_pload_info(tech_attrs, type, SMC_NLA_STATS_T_RX_RMB_SIZE) != NL_OK)
		goto errout;

	return NL_OK;
errout:
	return NL_STOP;
}

static int handle_gen_stats_reply(struct nl_msg *msg, void *arg)
{
	struct nlattr *stats_attrs[SMC_NLA_STATS_MAX + 1];
	struct nlattr *attrs[SMC_GEN_MAX + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);
	uint64_t trgt = 0;
	int rc = NL_OK;

	if (genlmsg_parse(hdr, 0, attrs, SMC_GEN_MAX,
			  (struct nla_policy *)smc_gen_net_policy) < 0) {
		fprintf(stderr, "%s: invalid data returned\n", "smc");
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}
	if (!attrs[SMC_GEN_STATS])
		return NL_STOP;

	if (nla_parse_nested(stats_attrs, SMC_NLA_STATS_MAX,
			     attrs[SMC_GEN_STATS],
			     smc_gen_stats_policy)) {
		fprintf(stderr, "failed to parse nested attributes!\n");
		return NL_STOP;
	}
	memset(&smc_stat, 0, sizeof(smc_stat));
	if (stats_attrs[SMC_NLA_STATS_CLNT_HS_ERR_CNT]) {
		trgt = nla_get_u64(stats_attrs[SMC_NLA_STATS_CLNT_HS_ERR_CNT]);
		smc_stat.clnt_hshake_err_cnt = trgt;
	}

	if (stats_attrs[SMC_NLA_STATS_SRV_HS_ERR_CNT]) {
		trgt = nla_get_u64(stats_attrs[SMC_NLA_STATS_SRV_HS_ERR_CNT]);
		smc_stat.srv_hshake_err_cnt = trgt;
	}

	if (stats_attrs[SMC_NLA_STATS_SMCR_TECH])
		rc = fill_tech_info(&stats_attrs[0], SMC_NLA_STATS_SMCR_TECH);
	if (stats_attrs[SMC_NLA_STATS_SMCD_TECH])
		rc = fill_tech_info(&stats_attrs[0], SMC_NLA_STATS_SMCD_TECH);
	return rc;
}

static int fback_array_last_pos(struct	smc_stats_fback *fback)
{
	int k;

	for (k = 0; k < SMC_MAX_FBACK_RSN_CNT; k++)
		if (fback[k].fback_code == 0)
			return k;

	return SMC_MAX_FBACK_RSN_CNT - 1;
}

static int handle_gen_fback_stats_reply(struct nl_msg *msg, void *arg)
{
	struct nlattr *stats_fback_attrs[SMC_NLA_FBACK_STATS_MAX + 1];
	struct nlattr *attrs[SMC_GEN_MAX + 1];
	struct nlmsghdr *hdr = nlmsg_hdr(msg);
	struct	smc_stats_fback *smc_fback;
	unsigned short type = 0, last_pos;
	uint64_t trgt64 = 0;
	int rc = NL_OK;
	int trgt = 0;

	if (genlmsg_parse(hdr, 0, attrs, SMC_GEN_MAX,
			  (struct nla_policy *)smc_gen_net_policy) < 0) {
		fprintf(stderr, "%s: invalid data returned\n", "smc");
		nl_msg_dump(msg, stderr);
		return NL_STOP;
	}
	if (!attrs[SMC_GEN_FBACK_STATS])
		return NL_STOP;

	if (nla_parse_nested(stats_fback_attrs, SMC_NLA_FBACK_STATS_MAX,
			     attrs[SMC_GEN_FBACK_STATS],
			     smc_gen_stats_fback_policy)) {
		fprintf(stderr, "failed to parse nested attributes!\n");
		return NL_STOP;
	}

	if (stats_fback_attrs[SMC_NLA_FBACK_STATS_SRV_CNT]) {
		trgt64 = nla_get_u64(stats_fback_attrs[SMC_NLA_FBACK_STATS_SRV_CNT]);
		smc_rsn.srv_fback_cnt = trgt64;
	}
	if (stats_fback_attrs[SMC_NLA_FBACK_STATS_SRV_CNT]) {
		trgt64 = nla_get_u64(stats_fback_attrs[SMC_NLA_FBACK_STATS_CLNT_CNT]);
		smc_rsn.clnt_fback_cnt = trgt64;
	}
	if (stats_fback_attrs[SMC_NLA_FBACK_STATS_TYPE]) {
		type = nla_get_u8(stats_fback_attrs[SMC_NLA_FBACK_STATS_TYPE]);
		if (type)
			smc_fback = smc_rsn.srv;
		else
			smc_fback = smc_rsn.clnt;

		last_pos = fback_array_last_pos(smc_fback);
		if (stats_fback_attrs[SMC_NLA_FBACK_STATS_RSN_CODE]) {
			trgt = nla_get_u32(stats_fback_attrs[SMC_NLA_FBACK_STATS_RSN_CODE]);
			smc_fback[last_pos].fback_code = trgt;
		}

		if (stats_fback_attrs[SMC_NLA_FBACK_STATS_RSN_CNT]) {
			trgt = nla_get_u16(stats_fback_attrs[SMC_NLA_FBACK_STATS_RSN_CNT]);
			smc_fback[last_pos].count = trgt;
		}
	}

	return rc;
}

static void handle_cmd_params(int argc, char **argv)
{

	memset(&smc_rsn, 0, sizeof(smc_rsn));
	if (argc == 0) {
		show_cmd = 1; /* no object given, so use the default "show" */
		return;
	}

	while (1) {
		if (contains(argv[0], "help") == 0) {
			usage();
		} else if (contains(argv[0], "show") == 0) {
			show_cmd = 1;
			break;
		} else if (contains(argv[0], "reset") == 0) {
			reset_cmd = 1;
			break;
		} else if (contains(argv[0], "json") == 0) {
			json_cmd = 1;
			break;
		}else {
			usage();
		}
		if (!NEXT_ARG_OK())
			break;
		NEXT_ARG();
	}
	/* Too many parameters or wrong sequence of parameters */
	if (NEXT_ARG_OK())
		usage();
}

static void read_cache_file(FILE *fp)
{
	int count = 0, idx = 0, rc, size_fback = 0;
	int size, val_err, val_cnt;
	unsigned long long val;
	__u64 *trgt, *fbck_cnt;
	char buf[4096];
	int *trgt_fbck;

	/* size without fallback reasons */
	size = sizeof(smc_stat_c) / sizeof(__u64);
	trgt = (__u64 *)&smc_stat_c;
	size_fback = size + 2*SMC_MAX_FBACK_RSN_CNT;
	trgt_fbck = (int *)&smc_rsn_c;
	fbck_cnt = (__u64 *)&smc_rsn_c.srv_fback_cnt;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (count < size) {
			rc = sscanf(buf, "%d%llu", &idx, &val);
			if (rc < 2) {
				perror("Error: parsing cache file(stats)");
				exit(-1);
			}
			if (idx != count) {
				perror("Error: unexpected value in cache file");
				exit(-1);
			}
			*trgt = val;
			trgt++;
		} else if (count < size_fback) {
			rc = sscanf(buf, "%d%d%d", &idx, &val_err, &val_cnt);
			if (rc < 3) {
				perror("Error: parsing cache file (fback stats)");
				exit(-1);
			}
			if (idx > SMC_MAX_FBACK_RSN_CNT * 2) {
				perror("Error: unexpected value in cache file (fback stats)");
				exit(-1);
			}
			*trgt_fbck = val_err;
			trgt_fbck++;
			*trgt_fbck = val_cnt;
			trgt_fbck++;
		} else if (count < size_fback + 2) {
			rc = sscanf(buf, "%llu", &val);
			if (rc < 1) {
				perror("Error: parsing cache file(fback counters)");
				exit(-1);
			}
			*fbck_cnt = val;
			fbck_cnt++;
		} else {
			perror("Error: cache file corrupt");
			exit(-1);
		}
		cache_file_exists = 1;
		count++;
	}
}

static int get_fback_err_cache_count(struct smc_stats_fback *fback, int trgt)
{
	int i;

	for (i = 0; i < SMC_MAX_FBACK_RSN_CNT; i++) {
		if (fback[i].fback_code == trgt)
			return fback[i].count;
	}

	return 0;
}

/* Check whether there were wrap arounds or really old data in the cache */
static int is_data_consistent ()
{
	int size, i, size_fback, val_err, val_cnt, cache_cnt;
	struct smc_stats_fback *kern_fbck;
	__u64 *kernel, *cache;

	size = sizeof(smc_stat) / sizeof(__u64);
	kernel = (__u64 *)&smc_stat;
	cache = (__u64 *)&smc_stat_c;
	for (i = 0; i < size; i++) {
		if (*kernel < *cache)
			return 0;
		kernel++;
		cache++;
	}

	size_fback = 2 * SMC_MAX_FBACK_RSN_CNT;
	kern_fbck = (struct smc_stats_fback *)&smc_rsn;
	for (i = 0; i < size_fback; i++) {
		val_err = kern_fbck->fback_code;
		if (i < SMC_MAX_FBACK_RSN_CNT)
			cache_cnt = get_fback_err_cache_count(smc_rsn_c.srv, val_err);
		else
			cache_cnt = get_fback_err_cache_count(smc_rsn_c.clnt, val_err);
		val_cnt = kern_fbck->count;
		kern_fbck++;
		if (val_cnt < cache_cnt)
			return 0;
	}

	if ((smc_rsn.srv_fback_cnt < smc_rsn_c.srv_fback_cnt) ||
	    (smc_rsn.clnt_fback_cnt < smc_rsn_c.clnt_fback_cnt))
		return 0;

	return 1;
}

static void merge_cache ()
{
	int size, i, size_fback, val_err, cache_cnt;
	struct smc_stats_fback *kern_fbck;
	__u64 *kernel, *cache;

	if (!is_data_consistent()) {
		unlink(cache_file_path);
		return;
	}

	size = sizeof(smc_stat) / sizeof(__u64);
	kernel = (__u64 *)&smc_stat;
	cache = (__u64 *)&smc_stat_c;
	for (i = 0; i < size; i++)
		*(kernel++) -=  *(cache++);

	size_fback = 2 * SMC_MAX_FBACK_RSN_CNT;
	kern_fbck = (struct smc_stats_fback *)&smc_rsn;
	for (i = 0; i < size_fback; i++) {
		val_err = kern_fbck->fback_code;
		if (i < SMC_MAX_FBACK_RSN_CNT)
			cache_cnt = get_fback_err_cache_count(smc_rsn_c.srv, val_err);
		else
			cache_cnt = get_fback_err_cache_count(smc_rsn_c.clnt, val_err);
		kern_fbck->count -= cache_cnt;
		kern_fbck++;
	}

	smc_rsn.srv_fback_cnt -= smc_rsn_c.srv_fback_cnt;
	smc_rsn.clnt_fback_cnt -= smc_rsn_c.clnt_fback_cnt;
}

static void open_cache_file()
{
	int fd;

	cache_file_path = malloc(128);
	sprintf(cache_file_path, "/tmp/.smcstats.u%d", getuid());

	fd = open(cache_file_path, O_RDWR|O_CREAT|O_NOFOLLOW, 0600);

	if (fd < 0) {
		perror("Error: open cache file");
		exit(-1);
	}

	if ((cache_fp = fdopen(fd, "r+")) == NULL) {
		perror("Error: cache file r+");
		exit(-1);
	}
	if (flock(fileno(cache_fp), LOCK_EX)) {
		perror("Error: cache file lock");
		exit(-1);
	}
}

static void init_cache_file()
{
	open_cache_file();
	read_cache_file(cache_fp);
}


static void fill_cache_file()
{
	int size, i, val_err, val_cnt;
	int *fback_src;
	__u64 *src;

	if (ftruncate(fileno(cache_fp), 0) < 0)
		perror("Error: ftruncate");

	size = sizeof(smc_stat) / sizeof(__u64);
	src = (__u64 *)&smc_stat_org;
	for (i = 0; i < size; i++) {
		fprintf(cache_fp, "%-12d%-16llu\n",i ,*src);
		src++;
	}

	fback_src = (int*)&smc_rsn_org;
	size = 2 * SMC_MAX_FBACK_RSN_CNT;
	for (i = 0; i < size; i++) {
		val_err = *(fback_src++);
		val_cnt = *(fback_src++);
		fprintf(cache_fp, "%-12d%-16d%-16d\n",i , val_err, val_cnt);
	}

	fprintf(cache_fp, "%16llu\n", smc_rsn.srv_fback_cnt);
	fprintf(cache_fp, "%16llu\n", smc_rsn.clnt_fback_cnt);

	fclose(cache_fp);
}

int invoke_stats(int argc, char **argv, int option_details)
{
	if (option_details == SMC_DETAIL_LEVEL_V || option_details == SMC_DETAIL_LEVEL_VV) {
		d_level = 1;
	} else if (option_details == SMC_OPTION_ABS) {
		is_abs = 1;
	} else if (option_details == SMC_OPTION_DETAIL_ABS) {
		is_abs = 1;
		d_level = 1;
	}

	handle_cmd_params(argc, argv);
	if (!is_abs)
		init_cache_file();
	if (gen_nl_handle_dump(SMC_NETLINK_GET_FBACK_STATS, handle_gen_fback_stats_reply, NULL))
		goto errout;
	if (gen_nl_handle_dump(SMC_NETLINK_GET_STATS, handle_gen_stats_reply, NULL))
		goto errout;
	memcpy(&smc_stat_org, &smc_stat, sizeof(smc_stat_org));
	memcpy(&smc_rsn_org, &smc_rsn, sizeof(smc_rsn_org));

	if (!is_abs && cache_file_exists)
		merge_cache();
	if (!json_cmd)
		print_as_text();
	else
		print_as_json();
	if (reset_cmd) {
		unlink(cache_file_path);
		open_cache_file();
		fill_cache_file();
	}
errout:
	free(cache_file_path);
	return 0;
}
