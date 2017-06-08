CXX = g++
LIBS = -lpthread -lboost_system -lboost_filesystem -lboost_thread
CXX_SRCS = $(wildcard deep_path/*.cc) $(wildcard vprof_files/*.cc)
C_SRCS = $(wildcard deep_path/*.c) $(wildcard vprof_files/*.c)
OBJS = $(CXX_SRCS:.cc=.o) $(C_SRCS:.c=.o)

all:
	make -C deep_path
	make -C vprof_files
	$(CXX) $(OBJS) $(LIBS) -o test_main

clean:
	make clean -C deep_path
	make clean -C vprof_files
