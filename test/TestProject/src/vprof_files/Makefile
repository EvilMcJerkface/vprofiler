CXX = g++
CXX_FLAGS = -std=c++11

all: trace_tool.o VProfEventWrappers.o

clean:
	rm -rf *.o

trace_tool.o: trace_tool.cc
	$(CXX) -c trace_tool.cc $(CXX_FLAGS) -o trace_tool.o

VProfEventWrappers.o: VProfEventWrappers.cc
	$(CXX) -c VProfEventWrappers.cc $(CXX_FLAGS) -o VProfEventWrappers.o
