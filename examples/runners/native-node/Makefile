# Node.js runner — no compilation needed, creates a wrapper script
SCRIPT = host_benchmark_node.js
WRAPPER = ../../wagnostic-bench-node

.PHONY: all bench clean

all: bench

bench: $(WRAPPER)

$(WRAPPER): $(SCRIPT)
	printf '#!/usr/bin/env bash\nexec node "$$(dirname "$$0")/runners/native-node/host_benchmark_node.js" "$$@"\n' > $@
	chmod +x $@

clean:
	rm -f $(WRAPPER)
