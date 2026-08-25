#!/bin/sh
# Mad Pascal programs (pascal/README.md), as committed in fs/PRG: they run
# from the ROM's RUN, print through JIM, and hand the shell back.
cd "$(dirname "$0")/.."
out=$(./test/headless rom/kernal.bin "RUN HELLO one two
~~~~" 600 2>&1) || { echo "$out"; echo "pastest: FAILED: hello"; exit 1; }
echo "$out" | grep -q "Hello from Mad Pascal" || { echo "$out"; echo "pastest: FAILED: hello did not print"; exit 1; }
echo "$out" | grep -q "You said: one two" || { echo "$out"; echo "pastest: FAILED: ParamStr"; exit 1; }
out=$(./test/headless rom/kernal.bin "PSIEVE
~~~~" 900 2>&1) || { echo "$out"; echo "pastest: FAILED: psieve"; exit 1; }
echo "$out" | grep -q "1899 primes" || { echo "$out"; echo "pastest: FAILED: the sieve's count"; exit 1; }
echo "pastest: OK (hello + ParamStr, sieve 1899 primes, both back to the shell)"
