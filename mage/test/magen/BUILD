load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

genrule(
    name = "gen",
    tools = [
      "//mage/parser:gen",
      "test.magen",
      "handle_accepter.magen",
    ],
    outs = [
      "test.magen.h",
      "handle_accepter.magen.h",
    ],
    cmd = " && ".join([
      # Follow the pattern below:
      # python3 $(location //generated:gen) <input *.mage file> <output *.magen.h file>
      "python3 $(location //mage/parser:gen) $(location test.magen) $(location test.magen.h)",
      "python3 $(location //mage/parser:gen) $(location handle_accepter.magen) $(location handle_accepter.magen.h)",
    ]),
)

cc_library(
    name = "include",
    hdrs = [":gen"]
)
