CXX = g++
SM_CXXFLAGS = $(shell pkg-config --cflags mozjs-140)
SM_LDFLAGS = $(shell pkg-config --libs mozjs-140)

BENCH = ../../wagnostic-bench-sm

.PHONY: all bench clean

all: bench

bench: $(BENCH)

$(BENCH): host_benchmark_sm.cpp
	$(CXX) -std=c++17 $(SM_CXXFLAGS) $< -o $@ $(SM_LDFLAGS) -O2

clean:
	rm -f $(BENCH)
