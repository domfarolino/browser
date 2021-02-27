load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

genrule(
    name = "gen",
    tools = [
      "//mage/parser:gen",
      "demo.magen",
    ],
    outs = [
      "demo.magen.h",
    ],
    cmd = " && ".join([
      # Follow the pattern below:
      # python $(location //generated:gen) <input *.mage file> <output *.magen.h file>
      "python $(location //mage/parser:gen) $(location demo.magen) $(location demo.magen.h)",
    ]),
)

cc_library(
    name = "include",
    hdrs = [":gen"]
)
