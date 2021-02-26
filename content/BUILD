load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_test(
  name = "base",
  srcs = [
    "main.cc",
    "task_posting.cc",
    "bind_task_loop_to_current_thread.cc"
  ],
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
  name = "main_dsym",
  srcs = [":main"],
  outs = ["main.dSYM"],
  output_to_bindir = True,
  cmd = "dsymutil $(location :main) -o $@"
)
