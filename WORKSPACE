load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

http_archive(
  name = "gtest",
  url = "https://github.com/google/googletest/archive/release-1.11.0.zip",
  sha256 = "353571c2440176ded91c2de6d6cd88ddd41401d14692ec1f99e35d013feda55a",
  strip_prefix = "googletest-release-1.11.0",
)

git_repository(
  name = "browser",
  remote = "https://github.com/domfarolino/browser.git",
  commit = "77021f6caf785717024ca8d438455092c8d188d2",
)

git_repository(
  name = "mage",
  remote = "https://github.com/domfarolino/mage.git",
  commit = "930f3097ea235fba9c4773eb1a1757a341a5d62d",
  repo_mapping = {
    "@gtest": "@gtest",
    "@base": "@browser",
  },
)
