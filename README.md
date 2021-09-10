# browser

Simple browser components. In reality the current goal of this project is to
house primitives useful for building any kind of complex multi-process
event-loop driven application. Since the primary author is a Web Platform
engineer, this project has a slight _browser_ bias.

## Compatibility

Right now things have only been built and tested on macOS Catalina, but should also work on Linux.

## Building

1. Install [bazel](https://docs.bazel.build/versions/master/install.html)
1. Build
   - `bazel build base/base_tests`
1. Run
   - `./bazel-bin/base/base_tests`

Alternatively you can build any of the examples in
[`//examples`](https://github.com/domfarolino/browser/tree/master/examples) with
`bazel build examples/<example_name_here>`.
