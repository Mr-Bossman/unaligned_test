CC=$(CROSS_COMPILE)gcc

all: unaligned_c unaligned

unaligned_c: unaligned.c
	$(CC) $^ -O0 -march=rv64imafdc -g -o $@

unaligned: unaligned.c
	$(CC) $^ -O0 -march=rv64imafd -g -o $@
