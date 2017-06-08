CXX = g++
INCLUDE_DIR = -I../vprof_files
CXX_FLAGS = --std=c++11 $(INCLUDE_DIR)

all: test_main.o test_src.o test_src_b.o

clean:
	rm -rf *.o test_main

test_main.o: test_main.cc
	$(CXX) -c test_main.cc $(CXX_FLAGS) -o test_main.o

test_src.o: test_src.cc
	$(CXX) -c test_src.cc $(CXX_FLAGS) -o test_src.o

test_src_b.o: test_src_b.cc
	$(CXX) -c test_src_b.cc $(CXX_FLAGS) -o test_src_b.o
