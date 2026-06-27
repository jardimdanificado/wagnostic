CXX = g++
SM_CXXFLAGS = $(shell pkg-config --cflags mozjs-140)
SM_LDFLAGS = $(shell pkg-config --libs mozjs-140)

HOST = ../../wagnostic-sm

.PHONY: all host clean

all: host

host: $(HOST)

$(HOST): host_sm.cpp
	$(CXX) -std=c++17 $(SM_CXXFLAGS) $< -o $@ $(SM_LDFLAGS) -O2 -lSDL2 -lGL

clean:
	rm -f $(HOST)
