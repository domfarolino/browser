# browser
Simple browser components

## Compatibility

Right now things have only been build and tested on macOS Catalina.

## Building

1. Install [bazel](https://docs.bazel.build/versions/master/install.html)
1. Build
   - `bazel build content/main`
1. Run
   - `./bazel-bin/content/main`

Optionally, you can build with debugging symbols:

```bazel build --copt="-g" --strip=never --spawn_strategy=standalone content/main```
