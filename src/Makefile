.PHONY: all clean

all: unaligned_c unaligned unaligned_vec

unaligned_c: unaligned.c
	$(CC) $(CFLAGS) -Og -march=rv64imafdc -g $^ -o $@

unaligned: unaligned.c
	$(CC) $(CFLAGS) -Og -march=rv64imafd  -g $^ -o $@

unaligned_vec: vector.c
	$(CC) $(CFLAGS) -Og -march=rv64imafdv -g $^ -o $@

clean:
	rm -f unaligned unaligned_c unaligned_vec
