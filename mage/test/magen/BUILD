load("@rules_cc//cc:defs.bzl", "cc_library")
load("//mage/parser:magen_build.bzl", "magen_build")

package(default_visibility = ["//visibility:public"])

magen_build(
  name = "gen",
  srcs = [
    "callback_interface.magen",
    "first_interface.magen",
    "fourth_interface.magen",
    "second_interface.magen",
    "test.magen",
    "third_interface.magen",
    "handle_accepter.magen",
  ],
)

cc_library(
    name = "include",
    hdrs = [":gen"]
)
