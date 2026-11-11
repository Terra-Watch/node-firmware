# Host tests (no board needed): every test/run_*_tests.sh builds and runs one test with g++.
.PHONY: test
test:
	@fail=0; for t in test/run_*_tests.sh; do bash $$t >/dev/null 2>&1 && echo "PASS $$t" || { echo "FAIL $$t"; fail=1; }; done; exit $$fail
