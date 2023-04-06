#!/bin/bash

# SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: Apache-2.0 OR MIT

set -eu

SSHHOST="${SSHHOST:?Set SSHHOST to the hostname of a Morello machine (or model).}"
REMOTEDIR="${REMOTEDIR:-capmap}"

if [ -z "${NOFMT+nofmt}" ]; then
  make clang-format-check
else
  echo "Skipping clang-format check because NOFMT is set."
fi

make -j10
ssh -q "$SSHHOST" mkdir -p "$REMOTEDIR"
scp -q *.so test-* "$SSHHOST":"$REMOTEDIR/"
if [ -z "${NORUN+norun}" ]; then
  ssh "$SSHHOST" "set -eu;
                  cd $REMOTEDIR;
                  ./test-morello-purecap;
                  ./test-morello-hybrid;"
else
  echo "Skipping test run because NORUN is set."
fi
