load("@rules_cc//cc:defs.bzl", "cc_binary")

config_setting(
  name = "linux",
  constraint_values = ["@bazel_tools//platforms:linux"],
)

config_setting(
  name = "darwin",
  constraint_values = ["@bazel_tools//platforms:osx"],
)

cc_binary(
  name = "main",
  srcs = ["main.cc"],
  copts = ["-Iexternal/gtest/include"],
  deps = [
    "//base:base",
    "@gtest//:gtest",
    "@gtest//:gtest_main",
  ],
  linkopts = select({
    ":darwin": [],
    ":linux": ["-lX11"],
  })
)

cc_binary(
  name = "task_posting",
  srcs = ["task_posting.cc"],
  deps = [
    "//base:base",
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
