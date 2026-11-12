# Host tests (no board needed): every test/run_*_tests.sh builds and runs one test with c++.
# A failing test prints the end of its output, so a CI log shows why.
.PHONY: test
test:
	@fail=0; for t in test/run_*_tests.sh; do out=$$(bash $$t 2>&1) && echo "PASS $$t" || { echo "FAIL $$t"; echo "$$out" | tail -25; fail=1; }; done; exit $$fail
