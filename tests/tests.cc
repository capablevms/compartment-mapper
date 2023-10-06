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

extern "C" void *__capability map_load_only_impl(TestRun *self) {
  size_t length = 42 * 4096;
  void *ptr = mmap(nullptr, length, PROT_READ, MAP_ANONYMOUS, -1, 0);
#ifdef __CHERI_PURE_CAPABILITY__
  void *__capability cap = cheri_perms_and(ptr, CHERI_PERM_LOAD);
#else
  auto addr = reinterpret_cast<ptraddr_t>(ptr);
  void *__capability cap = cheri_address_set(cheri_ddc_get(), addr);
  cap = cheri_bounds_set_exact(cap, length);
  cap = cheri_perms_and(cap, CHERI_PERM_LOAD);
#endif
  if (self->options().verbose()) {
    self->printf("map_load_only() -> %#lp\n", ptr);
  }
  return cap;
}

// Get a capability with load, but not load-capability permissions, and make
// some effort to clean up temporaries, etc.
//
// This is not expected to be reliable enough for security purposes, but is good
// enough for tests.
__attribute((naked)) void *__capability TestRun::map_load_only() {
  // AAPCS64(-cap):
  //    c0/x0: `this`
  // The result is returned in c0.
  // clang-format off
  asm(
#ifdef __CHERI_PURE_CAPABILITY__
      "str  clr, [csp, #-16]!\n"
#else
      "stp  lr, xzr, [sp, #-16]!\n"
#endif
      "bl   map_load_only_impl\n"
      // Scrub all caller-saved capability registers except:
      //  - c0, which holds the result,
      //  - c17, which we'll use to scrub the stack,
      //  - and clr, which we're going to restore anyway.
      "mov  x1, #0\n"
      "mov  x2, #0\n"
      "mov  x3, #0\n"
      "mov  x4, #0\n"
      "mov  x5, #0\n"
      "mov  x6, #0\n"
      "mov  x7, #0\n"
      "mov  x8, #0\n"
      "mov  x9, #0\n"
      "mov  x10, #0\n"
      "mov  x11, #0\n"
      "mov  x12, #0\n"
      "mov  x13, #0\n"
      "mov  x14, #0\n"
      "mov  x15, #0\n"
      "mov  x16, #0\n"
      // Scrub a section of stack. For our purposes, we just hope that this is
      // enough, and the worst that will happen is that a test might fail. For
      // security-sensitive applications, the actual stack used should be
      // measured or restricted somehow.
#ifdef __CHERI_PURE_CAPABILITY__
      // Assume that purecap implies C64.
      "sub  c17, csp, #(32 * 100)\n"
      "1:\n"
      "stp  czr, czr, [c17], #32\n"
      "cmp  sp, x17\n"
      "b.hi 1b\n"

      "ldr  clr, [csp], #16\n"
      "ret  clr\n"
#else
      // Assume that hybrid implies A64.
      "sub  x17, sp, #(32 * 100)\n"
      "1:\n"
      "stp  xzr, xzr, [x17], #16\n"
      "cmp  sp, x17\n"
      "b.hi 1b\n"

      "ldp  lr, xzr, [sp], #16\n"
      "ret  lr\n"
#endif
      );
  // clang-format on
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
