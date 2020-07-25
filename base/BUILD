load("@rules_cc//cc:defs.bzl", "cc_library")

config_setting(
  name = "mac",
  constraint_values = ["@bazel_tools//platforms:osx"],
)

config_setting(
  name = "linux",
  constraint_values = ["@bazel_tools//platforms:linux"],
)

# This library target doesn't actually produce anything. It is only to export
# base header files to other targets and packages, until implementation files
# are written in this directory. See
# https://groups.google.com/forum/#!topic/bazel-discuss/x7c8HJ6dke8.
cc_library(
  name = "base",
  hdrs = [
    "build_config.h",
    "check.h",
    "helper.h",
    "synchronization/condition_variable.h",
    "synchronization/mutex.h",
    "synchronization/synchronization_helpers.h",
    "scheduling/task_loop.h",
    "threading/thread.h",
    "threading/simple_thread.h",
    "window/window.h",
    "window/window_impl.h",
  ] +
  select({
    ":mac": ["window/window_macos.h"],
    ":linux": ["window/window_linux.h"],
  }),
  srcs = [
    "threading/thread.cc",
    "window/window_impl.cc",
  ] +
  select({
    ":mac": [],
    ":linux": ["window/window_linux.cc"],
  }),
  deps = [],
  linkopts = select({
    ":mac": [],
    ":linux": ["-lX11"],
  }),
  visibility = ["//visibility:public"],
)

# Workaround for debugging symbols. See
# https://github.com/bazelbuild/bazel/issues/2537#issuecomment-281497449.
genrule(
  name = "base_dsym",
  srcs = [":base"],
  outs = ["base.dSYM"],
  output_to_bindir = True,
  cmd = "dsymutil $(location :base) -o $@"
)
