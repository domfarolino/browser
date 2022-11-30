load("@rules_cc//cc:defs.bzl", "cc_library")
load("//mage/parser:magen_build.bzl", "magen_build")

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "include",
  hdrs = [":gen"]
)

magen_build(
  name = "gen",
  srcs = [
    "child_process.magen",
    "child_process_2.magen",
    "parent_process.magen",
  ],
)
