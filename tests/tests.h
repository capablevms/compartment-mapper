// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TESTS_H_
#define TESTS_H_

#include <stdio.h>

#include <vector>

#include "include/capmap.h"

namespace capmap {
namespace tests {

class Options {
 public:
  bool parse_arg(char const *arg);

  bool verbose() const { return verbosity_ > 0; }
  int verbosity() const { return verbosity_; }
  bool should_run(char const *name) const;

 private:
  int verbosity_ = 0;
  std::vector<char const *> filter_needles_;
};

class TestRun {
 public:
  bool passed() { return result_; }
  bool printed() const { return printed_; }

  int printf(char const *fmt, ...);
  Options const &options() { return options_; }

 protected:
  TestRun(Options const &options) : options_{options} {}

  void notify_print() {
    // We try to print "test... " then "PASS\n", so if the test prints something
    // we need to add a newline first.
    ::printf("\n");
    printed_ = true;
  }

  void print_range_dec(char const *prefix, const Range &range, char const *suffix) {
    printf("%s[%zu,%zu]%s", prefix, range.base(), range.last(), suffix);
  }

  void print_range_hex(char const *prefix, const Range &range, char const *suffix) {
    printf("%s[%#zx,%#zx]%s", prefix, range.base(), range.last(), suffix);
  }

  void print_sparse_range_dec(char const *prefix, const SparseRange &sr, char const *suffix) {
    const char *sep = "";
    printf("%s", prefix);
    for (auto const &range : sr.parts()) {
      print_range_dec(sep, range, "");
      sep = ", ";
    }
    printf("%s", suffix);
  }

  void print_sparse_range_bitmap(char const *prefix, const SparseRange &sr, char const *suffix,
                                 size_t bits, Range hl_range = Range()) {
    printf("%s", prefix);
    size_t count = 0;
    for (auto const &range : sr.parts()) {
      for (; (count < range.base()) && (count < bits); count++) {
        printf(hl_range.includes(count) ? "\033[31m-\033[m" : " ");
      }
      if (count >= bits) break;

      for (; (count <= range.last()) && (count < bits); count++) {
        printf(hl_range.includes(count) ? "\033[32m+\033[m" : "█");
      }
    }
    for (; (count < bits); count++) {
      printf(hl_range.includes(count) ? "\033[31m-\033[m" : " ");
    }
    printf("%s", suffix);
  }

  bool printed_ = false;
  bool result_ = true;

 private:
  Options const &options_;
};

class Test {
 public:
  virtual ~Test() {}
  virtual TestRun run(Options const &options) const = 0;
  char const *name() const { return name_; }

  static std::vector<Test *> &list();

 protected:
  Test(char const *name) : name_(name) { list().push_back(this); }
  char const *name_;
};

#define TEST(name)                                                                             \
  class TestRun_##name : public capmap::tests::TestRun {                                       \
   public:                                                                                     \
    TestRun_##name(capmap::tests::Options const &options) : capmap::tests::TestRun(options){}; \
    void run();                                                                                \
  };                                                                                           \
  class Test_##name : public capmap::tests::Test {                                             \
   public:                                                                                     \
    Test_##name(char const *name) : capmap::tests::Test(name) {}                               \
    virtual capmap::tests::TestRun run(                                                        \
        capmap::tests::Options const &options) const override final {                          \
      TestRun_##name runner(options);                                                          \
      runner.run();                                                                            \
      return runner;                                                                           \
    }                                                                                          \
  };                                                                                           \
  Test_##name test_##name(#name);                                                              \
  void TestRun_##name::run()

#define TRY(expr)                                               \
  do {                                                          \
    if (!(expr)) {                                              \
      printf("  `%s`\n", #expr);                                \
      /* This expects to be called from some TestRun::run(). */ \
      result_ = false;                                          \
      return;                                                   \
    }                                                           \
  } while (0)

}  // namespace tests
}  // namespace capmap

#endif
