CC = $(which gcc)
CXX = $(which g++)

C_FLAGS = -O3
CXX_FLAGS = -o3 --std=c++11
LINK_FLAG = -lpthread -lrt -L/home/jiamin/usr/lib -lboost_thread

SRCS = $(wildcard *.c)

PROGS = $(patsubst %.c,%,$(SRCS))

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<
clean: 
	rm -f $(PROGS)
