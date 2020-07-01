load("@rules_cc//cc:defs.bzl", "cc_library")

# This library target doesn't actually produce anything. It is only to export
# base header files to other targets and packages, until implementation files
# are written in this directory. See
# https://groups.google.com/forum/#!topic/bazel-discuss/x7c8HJ6dke8.
cc_library(
  name = "tests",
  srcs = [
    "process_test1.cc",
  ],
  deps = [],
  visibility = ["//visibility:public"],
)

# Workaround for debugging symbols. See
# https://github.com/bazelbuild/bazel/issues/2537#issuecomment-281497449.
genrule(
  name = "tests_dsym",
  srcs = [":tests"],
  outs = ["tests.dSYM"],
  output_to_bindir = True,
  cmd = "dsymutil $(location :tests) -o $@"
)
