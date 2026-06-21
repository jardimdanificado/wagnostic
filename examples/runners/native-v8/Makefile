CXX = g++
V8_DIR = ../lib/libv8
CXXFLAGS = -I$(V8_DIR)/include -std=c++20 -O2
LDFLAGS = -L$(V8_DIR)/lib -Wl,-rpath,'$$ORIGIN/../lib/libv8/lib' \
          -lv8 -lv8_libplatform -lv8_libbase \
          -lpthread -ldl -lm -lrt -lSDL2 -lGL

HOST = ../../wagnostic-v8
BENCH = ../../wagnostic-bench-v8

.PHONY: all host bench clean

all: host bench

host: $(HOST)

$(HOST): host_v8.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

bench: $(BENCH)

$(BENCH): host_benchmark_v8.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(filter-out -lSDL2 -lGL,$(LDFLAGS))

clean:
	rm -f $(HOST) $(BENCH)
