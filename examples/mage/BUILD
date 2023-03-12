load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
  name = "mage_parent",
  srcs = [
    "mage_parent.cc",
  ],
  deps = [
    "@base//base",
    "@mage//mage/public",
    "//examples/mage/magen:include",
    "@gtest//:gtest_prod",
  ],
  visibility = ["//visibility:public"],
)
cc_binary(
  name = "mage_child",
  srcs = [
    "mage_child.cc",
  ],
  deps = [
    "@base//base",
    "@mage//mage/public",
    "//examples/mage/magen:include",
    "@gtest//:gtest_prod",
  ],
  visibility = ["//visibility:public"],
)
