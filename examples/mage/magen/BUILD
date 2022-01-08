load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

genrule(
  name = "gen",
  tools = [
    "//mage/parser:gen",
    "child_process.magen",
  ],
  outs = [
    "child_process.magen.h",
  ],
  cmd = " && ".join([
    # Follow the pattern below:
    # python3 $(location //generated:gen) <input *.mage file> <output *.magen.h file>
    "python3 $(location //mage/parser:gen) $(location child_process.magen) $(location child_process.magen.h)",
  ]),
)

cc_library(
  name = "include",
  hdrs = [":gen"]
)
