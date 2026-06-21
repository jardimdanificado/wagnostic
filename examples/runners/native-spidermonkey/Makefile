CXX = g++
SM_CXXFLAGS = $(shell pkg-config --cflags mozjs-140)
SM_LDFLAGS = $(shell pkg-config --libs mozjs-140)

HOST = ../../wagnostic-sm
BENCH = ../../wagnostic-bench-sm

.PHONY: all host bench clean

all: host bench

host: $(HOST)

$(HOST): host_sm.cpp
	$(CXX) -std=c++17 $(SM_CXXFLAGS) $< -o $@ $(SM_LDFLAGS) -O2 -lSDL2 -lGL

bench: $(BENCH)

$(BENCH): host_benchmark_sm.cpp
	$(CXX) -std=c++17 $(SM_CXXFLAGS) $< -o $@ $(SM_LDFLAGS) -O2

clean:
	rm -f $(HOST) $(BENCH)
