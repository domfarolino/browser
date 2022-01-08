load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
  name = "mage_parent",
  srcs = [
    "mage_parent.cc",
  ],
  deps = [
    "//mage:mage",
    "//examples/mage/magen:include",
  ],
  visibility = ["//visibility:public"],
)
cc_binary(
  name = "mage_child",
  srcs = [
    "mage_child.cc",
  ],
  deps = [
    "//mage:mage",
    "//examples/mage/magen:include",
  ],
  visibility = ["//visibility:public"],
)
