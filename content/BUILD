load("@rules_cc//cc:defs.bzl", "cc_binary")

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

# TODO: Turn the example binaries below into actual gtests.
cc_binary(
  name = "task_posting",
  srcs = ["task_posting.cc"],
  deps = ["//base:base"],
)
cc_binary(
  name = "bind_task_loop_to_current_thread",
  srcs = ["bind_task_loop_to_current_thread.cc"],
  deps = ["//base:base"],
)
cc_binary(
  name = "mage_parent",
  srcs = ["mage_parent.cc"],
  deps = [
    "//base:base",
    "//mage:mage",
  ],
)
cc_binary(
  name = "mage_child",
  srcs = ["mage_child.cc"],
  deps = [
    "//base:base",
    "//mage:mage",
  ],
)
