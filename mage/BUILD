load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
  name = "mage",
  hdrs = [
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
    "core/message.cc",
    "core/node.cc",
    "core/util.cc",
  ],
  deps = ["//base:base"],
  visibility = ["//visibility:public"],
)
