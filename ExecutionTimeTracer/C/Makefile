CC = $(shell which gcc)
CXX = $(shell which g++)

C_FLAGS = -O3
CXX_FLAGS = -o3 --std=c++11
LINK_FLAG = -lpthread -lrt -lboost_thread -lboost_system -lboost_filesystem

C_SRCS = $(wildcard *_test.c)
CXX_SRCS = $(wildcard *.cc) $(wildcard *.cpp)
C_OBJS = $(patsubst %.c, %.o, $(C_SRCS))
CXX_OBJS = $(filter %.o,$(patsubst %.cc, %.o, $(CXX_SRCS)) $(patsubst %.cpp, %.o, $(CXX_SRCS)))

all: fifo_test pipe_test msg_test fifo_test_vprof pipe_test_vprof msg_test_vprof

#$(C_OBJS): $(C_SRCS)
#	$(CC) -c $(C_FLAGS) $< -o $@
#
#$(CXX_OBJS): $(CXX_SRCS)
#	$(CXX) -c $(CXX_FLAGS) $< -o $@

fifo_test.o: fifo_test.c
	$(CC) -c $(C_FLAGS) $< -o $@

fifo_test_vprof.o: fifo_test_vprof.c
	$(CC) -c $(C_FLAGS) $< -o $@

pipe_test.o: pipe_test.c
	$(CC) -c $(C_FLAGS) $< -o $@

pipe_test_vprof.o: pipe_test_vprof.c
	$(CC) -c $(C_FLAGS) $< -o $@

msg_test.o: msg_test.c
	$(CC) -c $(C_FLAGS) $< -o $@

msg_test_vprof.o: msg_test_vprof.c
	$(CC) -c $(C_FLAGS) $< -o $@

trace_tool.o: trace_tool.cc
	$(CXX) -c $(CXX_FLAGS) $< -o $@

VProfEventWrappers.o: VProfEventWrappers.cpp
	$(CXX) -c $(CXX_FLAGS) $< -o $@

fifo_test_vprof: fifo_test_vprof.o VProfEventWrappers.o trace_tool.o
	$(CXX) $^ $(LINK_FLAG) -o $@

fifo_test: fifo_test.o
	$(CC) fifo_test.o $(LINK_FLAG) -o fifo_test

pipe_test_vprof: pipe_test_vprof.o VProfEventWrappers.o trace_tool.o
	$(CXX) $^ $(LINK_FLAG) -o $@

pipe_test: pipe_test.o
	$(CC) pipe_test.o $(LINK_FLAG) -o pipe_test

msg_test_vprof: msg_test_vprof.o VProfEventWrappers.o trace_tool.o
	$(CXX) $^ $(LINK_FLAG) -o $@

msg_test: msg_test.o
	$(CC) msg_test.o $(LINK_FLAG) -o msg_test

print-%: ; @echo $* = $($*)

.PHONY: clean

clean: $(C_OBJS) $(CXX_OBJS)
	rm -rf $(C_OBJS) $(CXX_OBJS)
