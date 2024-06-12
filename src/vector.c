#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/syscall.h>

#ifdef SYS_riscv_hwprobe
#include <asm/hwprobe.h>
#endif

/* uClibc is lacking ucontext_t */
#ifndef ucontext_t
#include "local_ucontext.h"
#endif

#ifndef RISCV_HWPROBE_KEY_MISALIGNED_PERF
#define RISCV_HWPROBE_KEY_MISALIGNED_PERF	7
#endif

#ifndef RISCV_HWPROBE_KEY_VEC_MISALIGNED_PERF
#define RISCV_HWPROBE_KEY_VEC_MISALIGNED_PERF	8
#endif

typedef uint8_t u8;
typedef int64_t s64;
typedef uint64_t u64;

static uint8_t buf[16] __attribute__((aligned(16)));
static bool expect_sigbus = false;
static void *sigbus_ret_addr;

#ifdef SYS_riscv_hwprobe
static int __riscv_hwprobe(struct riscv_hwprobe *pairs,
			   size_t pair_count, size_t cpusetsize,
			   unsigned long *cpus_user,
			   unsigned int flags) {
	return syscall(SYS_riscv_hwprobe, pairs, pair_count, cpusetsize, cpus_user, flags);
}

static u64 riscv_probe_once(s64 key)
{
	struct riscv_hwprobe pairs;
	int ret;

	pairs.key = key;
	pairs.value = 0;

	ret = __riscv_hwprobe(&pairs, 1, 0, NULL, 0);
	if (ret < 0)
		return ret;

	return pairs.value;
}

static void sys_riscv_hwprobe(void)
{
	u64 val;

	val = riscv_probe_once(RISCV_HWPROBE_KEY_MISALIGNED_PERF);
	if (val < 0)
		fprintf(stderr, "Failed to probe misaligned access performance\n");
	else
		printf("Misaligned access performance: %lu\n", val);

	val = riscv_probe_once(RISCV_HWPROBE_KEY_VEC_MISALIGNED_PERF);
	if (val < 0)
		fprintf(stderr, "Failed to probe vector misaligned access performance\n");
	else
		printf("Vector misaligned access performance: %lu\n", val);
}
#else
static void sys_riscv_hwprobe(void)
{
	puts("No hwprobe syscall available\n");
}
#endif

static void do_unalined_vec(uint8_t *buf)
{
	__asm__ __volatile__ (
		".balign 4\n\t"
		".option push\n\t"
		".option arch, +v\n\t"
		"       vsetivli zero, 1, e16, m1, ta, ma\n\t"	// Vectors of 16b
		"       vle16.v v0, (%[ptr])\n\t"		// Load bytes
		".option pop\n\t"
		: : [ptr] "r" (buf + 1) : "v0");
}

static void sigbus_handler(int signal, siginfo_t *si, void *context)
{
	if (!expect_sigbus) {
		fprintf(stderr, "Received SIGBUS while not expecting it\n");
		_exit(EXIT_FAILURE);
	}

	/* Sketchy, but we don't have a lot of other options to modify return
	 * address "safely" within a signal handler.
	 */
	((ucontext_t*)context)->uc_mcontext.__gregs[REG_PC] = (unsigned long) sigbus_ret_addr;
}

int main(int argc, char **argv)
{
	int ret;
	struct sigaction act = { 0 };

	printf("Buf base address: %p\n", buf);

	act.sa_sigaction = &sigbus_handler;

	ret = sigaction(SIGBUS, &act, NULL);
	if (ret) {
		fprintf(stderr, "Failed to install SIGBUS handler\n");
		return EXIT_FAILURE;
	}

	sys_riscv_hwprobe();

	do_unalined_vec(buf);

	return EXIT_SUCCESS;
}
