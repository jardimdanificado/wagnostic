# js140 runner — shell script runner, creates a wrapper
SCRIPT = host_benchmark_js140.js
WRAPPER = ../../wagnostic-bench-js140

.PHONY: all bench clean

all: bench

bench: $(WRAPPER)

$(WRAPPER): $(SCRIPT)
	printf '#!/usr/bin/env bash\nexec js140 "$$(dirname "$$0")/runners/native-js140/host_benchmark_js140.js" "$$@"\n' > $@
	chmod +x $@

clean:
	rm -f $(WRAPPER)
