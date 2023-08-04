// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <stdio.h>

#include "include/capmap.h"

int main() {
  printf("Example: default process capability map.\n");
  capmap::simple_scan_and_print_json(stdout);
}
