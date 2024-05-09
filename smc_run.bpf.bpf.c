// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 *  eBPF version of smc_run
 *
 *  Copyright (c) 2024, Alibaba Inc.
 *
 *  Author:  D. Wythe <alibuda@linux.alibaba.com>
 */

#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

#ifndef AF_INET
#define AF_INET 2
#endif

#ifndef AF_INET6
#define AF_INET6 10
#endif

#ifndef IPPROTO_SMC
#define IPPROTO_SMC 256
#endif

#define CORE_READ(dst, src) bpf_core_read(dst, sizeof(*(dst)), src)

struct smc_run_strategy {
	__u8	enable;
	__u8	inherit;
};

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 65536);
	__type(key, pid_t);
	__type(value, struct smc_run_strategy);
} smc_run_pid SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 65536);
	__type(key, int);
	__type(value, struct smc_run_strategy);
} smc_run_netns SEC(".maps");

SEC("fentry/proc_free_inum")
int BPF_PROG(smc_run_on_net_cleanup, int ino)
{
	bpf_map_delete_elem(&smc_run_netns, &ino);
	return 0;
}

SEC("raw_tracepoint/sched_process_exit")
int smc_run_on_process_exit(void *ctx)
{
	pid_t pid = bpf_get_current_pid_tgid() >> 32;
	bpf_map_delete_elem(&smc_run_pid, &pid);
	return 0;
}

SEC("raw_tracepoint/sched_process_fork")
int smc_run_on_process_fork(struct bpf_raw_tracepoint_args *ctx)
{
	struct smc_run_strategy *match, init;
	pid_t pid;

	struct task_struct *parent = (struct task_struct *)ctx->args[0];
	struct task_struct *child = (struct task_struct *)ctx->args[1];

	if (CORE_READ(&pid, &parent->pid))
		return 0;

	match = bpf_map_lookup_elem(&smc_run_pid, &pid);
	if (match && match->enable && match->inherit)
	{
		if (CORE_READ(&pid, &child->pid))
			return 0;
		init.enable = init.inherit = 1;
		bpf_map_update_elem(&smc_run_pid, &pid, &init, BPF_NOEXIST);
	}

	return 0;
}

SEC("fmod_ret/update_socket_protocol")
int BPF_PROG(smc_run, int family, int type, int protocol)
{
	struct smc_run_strategy *match, init;
	struct task_struct *task;
	int netns_ino;
	pid_t pid;

	if (family != AF_INET && family != AF_INET6)
		goto nop;

	if ((type & 0xf) != SOCK_STREAM)
		goto nop;

	if (protocol != 0 && protocol != IPPROTO_TCP)
		goto nop;

	pid = bpf_get_current_pid_tgid() >> 32;
	match = bpf_map_lookup_elem(&smc_run_pid, &pid);
	if (match)
		goto found;

	task = bpf_get_current_task_btf();
	if (!task)
		goto nop;

	netns_ino = task->nsproxy->net_ns->ns.inum;
	match = bpf_map_lookup_elem(&smc_run_netns, &netns_ino);

	if (match) {
		if (match->enable) {
			init.enable = 1;
			init.inherit = 0;
			/* speed up */
			bpf_map_update_elem(&smc_run_pid, &pid, &init, BPF_NOEXIST);
		}			
		goto found;
	}
nop:
	return protocol;
found:
	return match->enable ? IPPROTO_SMC : protocol; 
}
