#!/usr/bin/env bash

set -euo pipefail

scope_tmp=$(mktemp -d)
trap 'rm -rf "$scope_tmp"' EXIT

# Current upstream uses fixed-seed hashing and no longer has --hash-seed.
# Check independent frontend runs, including randomized pointer allocation,
# and compare sorted output so backend container order is not the assertion.
for scope_mode in baseline randomized_1 randomized_2; do
  scope_options=()
  if [[ "${scope_mode}" != baseline ]]; then
    scope_options=(-M)
  fi
  "${YOSYS}" "${scope_options[@]}" -q -p \
    "read_verilog -sv scope_hash_determinism.sv; hierarchy -check -top top; proc; flatten; opt_clean; select -module top; sat -verify -prove ok 1; write_rtlil -sort ${scope_tmp}/${scope_mode}.il"
done

cmp "${scope_tmp}/baseline.il" "${scope_tmp}/randomized_1.il"
cmp "${scope_tmp}/baseline.il" "${scope_tmp}/randomized_2.il"
