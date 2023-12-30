#!/usr/bin/env bash

set -e
set -o pipefail

dd if=/dev/urandom of=input bs=1M count=5 status=none
dd if=/dev/urandom of=secret bs=1M count=5 status=none

sha512sum --tag secret > secret.sha512

./hide -c -i input -s secret -o out

sha512sum --quiet -c secret.sha512

./hide -e -i out -o sec

sha512sum --quiet -c secret.sha512
sha512sum secret sec | cut -d" " -f1 | sort -u | wc -l | grep -Pqe '^1$'

rm -f input secret secret.sha512 sec out
