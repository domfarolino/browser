load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
  name = "invitee_as_remote",
  srcs = [
    "invitee_as_remote.cc",
  ],
  deps = [
    "//base:base",
    "//mage:mage",
    "//mage/test/magen:include",
  ],
  visibility = ["//visibility:public"],
)
cc_binary(
  name = "inviter_as_remote",
  srcs = [
    "inviter_as_remote.cc",
  ],
  deps = [
    "//base:base",
    "//mage:mage",
    "//mage/test/magen:include",
  ],
  visibility = ["//visibility:public"],
)
cc_binary(
  name = "inviter_as_remote_block_on_acceptance",
  srcs = [
    "inviter_as_remote_block_on_acceptance.cc",
  ],
  deps = [
    "//base:base",
    "//mage:mage",
    "//mage/test/magen:include",
  ],
  visibility = ["//visibility:public"],
)
