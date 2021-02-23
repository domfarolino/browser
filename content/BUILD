load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
  name = "main",
  srcs = ["main.cc"],
  copts = ["-Iexternal/gtest/include"],
  deps = [
    "//base:base",
    "@gtest//:gtest",
    "@gtest//:gtest_main",
  ],
)

# TODO: Turn the example binaries below into actual gtests.
cc_binary(
  name = "task_posting",
  srcs = ["task_posting.cc"],
  deps = ["//base:base"],
)
cc_binary(
  name = "bind_task_loop_to_current_thread",
  srcs = ["bind_task_loop_to_current_thread.cc"],
  deps = ["//base:base"],
)
cc_binary(
  name = "io_thread_example",
  srcs = ["io_thread_example.cc"],
  deps = ["//base:base"],
)
cc_binary(
  name = "io_thread_file_desc_example",
  srcs = ["io_thread_file_desc_example.cc"],
  deps = ["//base:base"],
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
