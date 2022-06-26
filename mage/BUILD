load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
  name = "mage",
  hdrs = [
    "bindings/receiver.h",
    "bindings/remote.h",
    "core/channel.h",
    "core/core.h",
    "core/endpoint.h",
    "core/handles.h",
    "core/message.h",
    "core/node.h",
    "core/util.h",
  ],
  srcs = [
    "core/channel.cc",
    "core/core.cc",
    "core/endpoint.cc",
    "core/message.cc",
    "core/node.cc",
    "core/util.cc",
  ],
  deps = ["//base:base"],
  visibility = ["//visibility:public"],
)

# This filegroup target consists of misc binaries that //mage:mage_tests depend
# on being built.
filegroup(
  name = "mage_tests_misc",
  srcs = [
    "//mage/test:invitee_as_remote",
    "//mage/test:inviter_as_remote",
    "//mage/test:inviter_as_remote_block_on_acceptance",
    "//mage/test:child_as_receiver_and_callback",
  ],
)
cc_test(
  name = "mage_tests",
  srcs = [
    "core/mage_test.cc",
  ],
  copts = ["-Iexternal/gtest/include"],
  deps = [
    "//base:base",
    "//mage:mage",
    "//mage/test/magen:include",
    "@gtest//:gtest",
    "@gtest//:gtest_main",
  ],
  data = ["//mage:mage_tests_misc"],
)
