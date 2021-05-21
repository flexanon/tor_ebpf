SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)
CFLAGS=-I../.. -I ../../.. -I../../core/or -I../../ext -I../../ubpf/vm -I../../ubpf/vm/inc


all: $(SRC) $(OBJ)

$(OBJ): %.o

%.o: %.c
	clang-6.0 $(CFLAGS) -O2 -fno-gnu-inline-asm -emit-llvm -D PLUGIN_CLANG -c $< -o - | llc-6.0 -march=bpf -filetype=obj -o $@

clean:
	rm -rf *.o

.PHONY: %.o
