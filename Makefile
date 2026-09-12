# =============================================================================
#  Wagnostic 2.0 — Top-Level Makefile
# =============================================================================

.PHONY: all runner roms clean test test-native test-node

all: runner roms

runner:
	$(MAKE) -C runners/native

roms:
	$(MAKE) -C roms

clean:
	$(MAKE) -C runners/native clean
	$(MAKE) -C roms clean

test: test-native

test-native: runner roms
	$(MAKE) -C roms test-native

test-node: roms
	$(MAKE) -C roms test-node
