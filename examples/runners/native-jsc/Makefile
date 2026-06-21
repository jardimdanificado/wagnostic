CXX = g++
CXXFLAGS = $(shell pkg-config --cflags javascriptcoregtk-4.1)
LDFLAGS = $(shell pkg-config --libs javascriptcoregtk-4.1)

BENCH = ../../wagnostic-bench-jsc

.PHONY: all bench clean

all: bench

bench: $(BENCH)

$(BENCH): host_benchmark_jsc.cpp
	$(CXX) -std=c++17 $(CXXFLAGS) $< -o $@ $(LDFLAGS) -O2

clean:
	rm -f $(BENCH)
