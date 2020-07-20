load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application")
load("@build_bazel_rules_apple//apple:versioning.bzl", "apple_bundle_version")

apple_bundle_version(
    name = "myAppVersion",
    build_version = "1.0",
)

objc_library(
    name = "Sources",
    srcs = [
        "main.m",
    ]
)

macos_application(
  name = "myApp",
  bundle_id = "myID",
  minimum_os_version = "10.11",
  infoplists = [":info.plist"],
  deps = ["Sources"],
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
