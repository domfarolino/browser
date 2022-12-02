load("@rules_cc//cc:defs.bzl", "cc_library")
load("//mage/parser:magen_idl.bzl", "magen_idl")

package(default_visibility = ["//visibility:public"])

magen_idl(
  name = "include",
  srcs = [
    "child_process.magen",
    "child_process_2.magen",
    "parent_process.magen",
  ],
)
