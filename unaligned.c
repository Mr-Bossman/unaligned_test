#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <float.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/prctl.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define stringify(s) __stringify(s)
#define __stringify(s) #s

#define VAL16	0x1234
#define VAL32	0xDEADBEEF
#define VAL64	0x45674321D00DF789

#define VAL_FLOAT	78951.234375
#define VAL_DOUBLE	567890.512396965789589290

struct misaligned_test_params {
	int size;
	int offset;
};

static uint8_t buf[16] __attribute__((aligned(16)));
static bool expect_sigbus = false;
static void *sigbus_ret_addr;
static const struct misaligned_test_params gp_params[] = {
	{2, 1},
	{4, 1},
	{4, 2},
	{4, 3},
	{8, 1},
	{8, 2},
	{8, 3},
	{8, 4},
	{8, 5},
	{8, 6},
	{8, 7},
};

void write_test_sigbus_exit(void);
void read_test_sigbus_exit(void);
void fpu_load_sigbus_exit(void);
void fpu_store_sigbus_exit(void);

#define test_fp_instr(__instr, __fp_reg, __offset, __addr) \
case __fp_reg: \
	asm volatile (__instr" f"stringify(__fp_reg)", 0(%[greg])"::[greg] "r" (__addr) :"memory");\
	break;

#define test_fp_instr_gen_c(__instr, __offset, __addr) \
	test_fp_instr(__instr, 8, __offset, __addr); \
	test_fp_instr(__instr, 9, __offset, __addr); \
	test_fp_instr(__instr, 10, __offset, __addr); \
	test_fp_instr(__instr, 11, __offset, __addr); \
	test_fp_instr(__instr, 12, __offset, __addr); \
	test_fp_instr(__instr, 13, __offset, __addr); \
	test_fp_instr(__instr, 14, __offset, __addr); \
	test_fp_instr(__instr, 15, __offset, __addr)

#define test_fp_instr_gen(__instr, __offset, __addr) \
	test_fp_instr(__instr, 0, __offset, __addr); \
	test_fp_instr(__instr, 1, __offset, __addr); \
	test_fp_instr(__instr, 2, __offset, __addr); \
	test_fp_instr(__instr, 3, __offset, __addr); \
	test_fp_instr(__instr, 4, __offset, __addr); \
	test_fp_instr(__instr, 5, __offset, __addr); \
	test_fp_instr(__instr, 6, __offset, __addr); \
	test_fp_instr(__instr, 7, __offset, __addr); \
	test_fp_instr_gen_c(__instr, __offset, __addr); \
	test_fp_instr(__instr, 16, __offset, __addr); \
	test_fp_instr(__instr, 17, __offset, __addr); \
	test_fp_instr(__instr, 18, __offset, __addr); \
	test_fp_instr(__instr, 19, __offset, __addr); \
	test_fp_instr(__instr, 20, __offset, __addr); \
	test_fp_instr(__instr, 21, __offset, __addr); \
	test_fp_instr(__instr, 22, __offset, __addr); \
	test_fp_instr(__instr, 23, __offset, __addr); \
	test_fp_instr(__instr, 24, __offset, __addr); \
	test_fp_instr(__instr, 25, __offset, __addr); \
	test_fp_instr(__instr, 26, __offset, __addr); \
	test_fp_instr(__instr, 27, __offset, __addr); \
	test_fp_instr(__instr, 28, __offset, __addr); \
	test_fp_instr(__instr, 29, __offset, __addr); \
	test_fp_instr(__instr, 30, __offset, __addr); \
	test_fp_instr(__instr, 31, __offset, __addr)


bool float_equal(float a, float b)
{
	float scaled_epsilon;
	float difference = fabsf(a - b);

	// Scale to the largest value.
	a = fabsf(a);
	b = fabsf(b);
	if (a > b)
		scaled_epsilon = FLT_EPSILON * a;
	else
		scaled_epsilon = FLT_EPSILON * b;

	return difference <= scaled_epsilon;
}

bool double_equal(double a, double b)
{
	double scaled_epsilon;
	double difference = fabsf(a - b);

	// Scale to the largest value.
	a = fabs(a);
	b = fabs(b);
	if (a > b)
		scaled_epsilon = DBL_EPSILON * a;
	else
		scaled_epsilon = DBL_EPSILON * b;

	return difference <= scaled_epsilon;
}


void test_fpu_store(int size, int fp_reg, int offset)
{

	sigbus_ret_addr = &fpu_store_sigbus_exit;
	register void *offset_reg asm ("s1") = (buf + offset);

	switch(size) {
		case 4:
			float f;
			switch (fp_reg) {
#ifdef __riscv_compressed
  #if __riscv_xlen == 32
				test_fp_instr_gen_c("c.fsw", offset, offset_reg);
  #endif
#else
				test_fp_instr_gen("fsw", offset, offset_reg);
#endif
			};
			memcpy(&f, offset_reg, sizeof(f));
			if (!float_equal(f, VAL_FLOAT)) {
				fprintf(stderr, "Read invalid float value from buf: %f instead of %f\n", f, VAL_FLOAT);
				exit(EXIT_FAILURE);
			}
		break;
		case 8:
			double d;
			switch (fp_reg) {
#ifdef __riscv_compressed
				test_fp_instr_gen_c("c.fsd", offset, offset_reg);
#else
				test_fp_instr_gen("fsd", offset, offset_reg);
#endif
			};
			memcpy(&d, offset_reg, sizeof(d));
			if (!double_equal(d, VAL_DOUBLE)) {
				fprintf(stderr, "Read invalid double value from buf: %f instead of %f\n", d, VAL_DOUBLE);
				exit(EXIT_FAILURE);
			}

		break;
	}

asm volatile("fpu_store_sigbus_exit: .global fpu_store_sigbus_exit":::"memory");
	return;
}


void test_fpu_load(int size, int fp_reg, int offset)
{

	sigbus_ret_addr = &fpu_load_sigbus_exit;
	register void *offset_reg asm ("s1") = (buf + offset);

	switch(size) {
		case 4:
			float f = VAL_FLOAT;
			memcpy(offset_reg, &f, sizeof(f));	
			switch (fp_reg) {
#ifdef __riscv_compressed
  #if __riscv_xlen == 32
				test_fp_instr_gen_c("c.flw", offset, offset_reg);
  #endif
#else
				test_fp_instr_gen("flw", offset, offset_reg);
#endif
			};
		break;
		case 8:
			double d = VAL_DOUBLE;
			memcpy(offset_reg, &d, sizeof(d));
			switch (fp_reg) {
#ifdef __riscv_compressed
				test_fp_instr_gen_c("c.fld", offset, offset_reg);
#else
				test_fp_instr_gen("fld", offset, offset_reg);
#endif
			};
		break;
	}

asm volatile("fpu_load_sigbus_exit: .global fpu_load_sigbus_exit":::"memory");
	return;
}

static void test_misaligned_write(int size, int offset)
{
	sigbus_ret_addr = &write_test_sigbus_exit;
	switch(size) {
		case 2:
			uint16_t *ptr16 = (uint16_t *) (buf + offset);
			*ptr16 = VAL16;
			break;
		case 4:
			uint32_t *ptr32 = (uint32_t *) (buf + offset);
			*ptr32 = VAL32;
			break;
		case 8:
			uint64_t *ptr64 = (uint64_t *) (buf + offset);
			*ptr64 = VAL64;
			break;
		default:
			fprintf(stderr, "Unsupported size %d\n", size);
			exit(EXIT_FAILURE);
			break;
	}

	if (expect_sigbus) {
		fprintf(stderr, "SIGBUS was not triggered\n");
		exit(EXIT_FAILURE);
	}

asm volatile("write_test_sigbus_exit: .global write_test_sigbus_exit"::: "memory");
	return;
}

static void test_misaligned_read(int size, int offset)
{
	sigbus_ret_addr = &read_test_sigbus_exit;
	switch(size) {
		case 2:
			uint16_t *ptr16 = (uint16_t *) (buf + offset);
			uint16_t val16 = *ptr16;
			if (val16 != VAL16) {
				fprintf(stderr, "Read invalid value from buffer %"PRIx16" instead of %"PRIx16"\n", val16, VAL16);
				exit(EXIT_FAILURE);
			}
			break;
		case 4:
			uint32_t *ptr32 = (uint32_t *) (buf + offset);
			uint32_t val32 = *ptr32;
			if (val32 != VAL32) {
				fprintf(stderr, "Read invalid value from buffer %"PRIx32" instead of %"PRIx32"\n", val32, VAL32);
				exit(EXIT_FAILURE);
			}
			break;
		case 8:
			uint64_t *ptr64 = (uint64_t *) (buf + offset);
			uint64_t val64 = *ptr64;
			if (val64 != VAL64) {
				fprintf(stderr, "Read invalid value from buffer %"PRIx64" instead of %"PRIx64"\n", val64, VAL64);
				exit(EXIT_FAILURE);
			}
			break;
		default:
			fprintf(stderr, "Unsupported size %d\n", size);
			exit(EXIT_FAILURE);
			break;
	}

	if (expect_sigbus) {
		fprintf(stderr, "SIGBUS was not triggered\n");
		exit(EXIT_FAILURE);
	}

asm volatile("read_test_sigbus_exit: .global read_test_sigbus_exit"::: "memory");
	return;
}

static void test_size(int size, int offset)
{
	int ret;
	int fp_reg;
	unsigned long val;

	printf("Testing emulated misaligned accesses, size %d, offset %d\n", size, offset);

	ret = prctl(PR_SET_UNALIGN, PR_UNALIGN_NOPRINT);
	if (ret == -1) {
		fprintf(stderr, "PR_SET_UNALIGN PR_UNALIGN_NOPRINT failed %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	ret = prctl(PR_GET_UNALIGN, &val);
	if (ret == -1 || val != PR_UNALIGN_NOPRINT) {
		fprintf(stderr, "PR_GET_UNALIGN invalid value returned, %d, val %lx\n", ret, val);
		exit(EXIT_FAILURE);
	}

	memset(buf, 0, sizeof(buf));
	expect_sigbus = false;
	test_misaligned_write(size, offset);
	test_misaligned_read(size, offset);
	if (size == 4 || size == 8) {
		for (fp_reg = 0; fp_reg < 32; fp_reg++) {
			test_fpu_load(size, fp_reg, offset);
			test_fpu_store(size, fp_reg, offset);
		}
	}

	printf("Testing non-emulated misaligned accesses, size %d, offset %d\n", size, offset);

	ret = prctl(PR_SET_UNALIGN, PR_UNALIGN_SIGBUS);
	if (ret == -1) {
		fprintf(stderr, "PR_SET_UNALIGN PR_UNALIGN_SIGBUS failed %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	ret = prctl(PR_GET_UNALIGN, &val);
	if (ret == -1 || val != PR_UNALIGN_SIGBUS) {
		fprintf(stderr, "PR_GET_UNALIGN invalid value returned, %d, val %lx\n", ret, val);
		exit(EXIT_FAILURE);
	}

	memset(buf, 0, sizeof(buf));
	expect_sigbus = true;
	test_misaligned_write(size, offset);
	test_misaligned_read(size, offset);
	if (size == 4 || size == 8) {
		for (fp_reg = 0; fp_reg < 32; fp_reg++) {
			test_fpu_load(size, fp_reg, offset);
			test_fpu_store(size, fp_reg, offset);
		}
	}
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
	int i;
	int ret;
	struct sigaction act = { 0 };
	const struct misaligned_test_params *test;

	printf("Buf base address: %p\n", buf);

	act.sa_sigaction = &sigbus_handler;

	ret = sigaction(SIGBUS, &act, NULL);
	if (ret) {
		fprintf(stderr, "Failed to install SIGBUS handler\n");
		return EXIT_FAILURE;
	}

	for (i = 0; i < ARRAY_SIZE(gp_params); i++) {
		test = &gp_params[i];
		test_size(test->size, test->offset);
	}

	return EXIT_SUCCESS;
}
