// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "tests.h"

#include <stdarg.h>
#include <stdio.h>
#include <sys/mman.h>

#include "include/capmap.h"

using capmap::tests::Options;
using capmap::tests::Test;
using capmap::tests::TestRun;

std::vector<Test *> &Test::list() {
  static std::vector<Test *> singleton;
  return singleton;
}

int TestRun::printf(char const *fmt, ...) {
  if (!options().verbose()) return 0;
  if (!printed_) notify_print();

  va_list args;
  va_start(args, fmt);
  int result = vprintf(fmt, args);
  va_end(args);
  return result;
}

bool Options::parse_arg(char const *arg) {
  if (arg[0] == '-') {
    if (arg[1] == '-') {
      // Long options.
      if (strcmp(arg, "--verbose") == 0) {
        verbosity_++;
        return true;
      } else {
        return false;
      }
    } else {
      // Short options.
      char const *c = &arg[1];
      do {
        switch (*c) {
          case 'v':
            verbosity_++;
            break;
          default:
            return false;
        }
      } while (*(++c) != '\0');
      return true;
    }
  } else {
    filter_needles_.push_back(arg);
    return true;
  }
}

bool Options::should_run(char const *name) const {
  if (filter_needles_.empty()) return true;
  for (char const *needle : filter_needles_) {
    if (strstr(name, needle)) return true;
  }
  return false;
}

int main(int argc, char const *argv[]) {
  int pass = 0;
  int fail = 0;
  Options options;
  for (int i = 1; i < argc; i++) {
    if (!options.parse_arg(argv[i])) {
      fprintf(stderr, "Bad argument: %s\n", argv[i]);
      exit(1);
    }
  }
  for (Test *test : Test::list()) {
    if (!options.should_run(test->name())) continue;
    printf("%s... ", test->name());
    fflush(stdout);
    auto run = test->run(options);
    if (run.printed()) {
      printf("  ");
    }
    if (run.passed()) {
      printf("\033[1;32mPASS\033[m\n");
      pass++;
    } else {
      printf("\033[1;31mFAIL\033[m\n");
      fail++;
    }
  }
  printf("Tests complete: %d passed, %d failed\n", pass, fail);
  return 0;
}
