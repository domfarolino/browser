load("@rules_cc//cc:defs.bzl", "cc_library")

# This library target doesn't actually produce anything. It is only to export
# base header files to other targets and packages, until implementation files
# are written in this directory. See
# https://groups.google.com/forum/#!topic/bazel-discuss/x7c8HJ6dke8.
cc_library(
  name = "base",
  hdrs = [
    "check.h",
    "helper.h",
    "scheduling/task_loop.h",
    "scheduling/task_loop_for_worker.h",
    "scheduling/task_runner.h",
    "synchronization/condition_variable.h",
    "synchronization/mutex.h",
    "synchronization/synchronization_helpers.h",
    "threading/thread.h",
    "threading/simple_thread.h",
  ],
  srcs = [
    "scheduling/task_loop.cc",
    "scheduling/task_loop_for_worker.cc",
    "threading/thread.cc",
  ],
  deps = [],
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
