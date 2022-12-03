load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
  name = "public",
  deps = [
    ":include"
  ],
  # Don't leak all of the core internal headers to the public API.
  implementation_deps = [
    "//mage/core",
  ],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "include",
  hdrs = [
    "receiver.h",
    "remote.h",
    "message.h",
    "handles.h",
    "receiver_delegate.h",
    "core.h",
    "util.h",
  ],
  # Only make `:include` visible to `public` and things inside of //mage, since
  # some of the public headers are needed by core internal code.
  visibility = ["//mage:__subpackages__"],
)
