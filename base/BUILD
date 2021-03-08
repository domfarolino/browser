load("@rules_cc//cc:defs.bzl", "cc_library")

config_setting(
  name = "mac",
  constraint_values = ["@bazel_tools//platforms:osx"],
)

config_setting(
  name = "linux",
  constraint_values = ["@bazel_tools//platforms:linux"],
)

# This library target doesn't actually produce anything. It is only to export
# base header files to other targets and packages, until implementation files
# are written in this directory. See
# https://groups.google.com/forum/#!topic/bazel-discuss/x7c8HJ6dke8.
cc_library(
  name = "base",
  hdrs = [
    "build_config.h",
    "callback.h",
    "check.h",
    "scheduling/task_loop.h",
    "scheduling/task_loop_for_io.h",
    "scheduling/task_loop_for_worker.h",
    "scheduling/task_runner.h",
    "synchronization/condition_variable.h",
    "synchronization/mutex.h",
    "synchronization/synchronization_helpers.h",
    "threading/thread.h",
    "threading/simple_thread.h",
  ] +
  select({
    ":mac": ["scheduling/task_loop_for_io_mac.h"],
    ":linux": [],
  }),
  srcs = [
    "scheduling/task_loop.cc",
    "scheduling/task_loop_for_worker.cc",
    "threading/thread.cc",
  ] +
  select({
    ":mac": ["scheduling/task_loop_for_io_mac.cc"],
    ":linux": [],
  }),

  deps = [],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "base_tests",
  srcs = [
    "build_config_test.cc",
    "scheduling/task_loop_test.cc",
    "threading/simple_thread_test.cc",
  ] +
  select({
    ":mac": ["scheduling/task_loop_for_io_test.cc"],
    # TODO(domfarolino): Enable the task loop for IO tests on linux once an IO task loop is implemented there.
    ":linux": [],
  }),
  copts = ["-Iexternal/gtest/include"],
  deps = [
    "//base:base",
    "@gtest//:gtest",
    "@gtest//:gtest_main",
  ],
)

# Workaround for debugging symbols. See
# https://github.com/bazelbuild/bazel/issues/2537#issuecomment-281497449.
genrule(
  name = "base_dsym",
  srcs = [":base"],
  outs = ["base.dSYM"],
  output_to_bindir = True,
  cmd = "dsymutil $(location :base) -o $@"
)
