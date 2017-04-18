CC = /opt/rh/devtoolset-2/root/usr/bin/gcc
CXX = /opt/rh/devtoolset-2/root/usr/bin/g++

C_FLAGS = -O3
CXX_FLAGS = -o3 --std=c++11
LINK_FLAG = -lpthread -lrt -L/home/jiamin/usr/lib -lboost_thread

C_SRCS = $(wildcard *_test.c)
CXX_SRCS = $(wildcard *.cc)
C_OBJS = $(patsubst %.c, %.o, $(C_SRCS))
CXX_OBJS =  $(patsubst %.cc, %.o, $(CXX_SRCS))

$(C_OBJS): $(C_SRCS)
	$(CC) -c $(C_FLAGS) $< -o $@

$(CXX_OBJS): $(CXX_SRCS)
	$(CXX) -c $(CXX_FLAGS) $< -o $@

fifo_test: fifo_test.o
	$(CC) fifo_test.o $(LINK_FLAG) -o fifo_test

pipe_test: pipe_test.o
	$(CC) pipe_test.o $(LINK_FLAG) -o pipe_test

msg_test: msg_test.o
	$(CC) msg_test.o $(LINK_FLAG) -o msg_test

all: fifo_test pipe_test msg_test

.PHONY: clean

clean: $(C_OBJS) $(CXX_OBJS)
	rm -rf $(C_OBJS) $(CXX_OBJS)
