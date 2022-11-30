load("@rules_cc//cc:defs.bzl", "cc_library")
load("//mage/parser:magen_build.bzl", "magen_build")

package(default_visibility = ["//visibility:public"])

magen_build(
  name = "gen",
  srcs = [
    "demo.magen",
  ],
)

cc_library(
    name = "include",
    hdrs = [":gen"]
)
