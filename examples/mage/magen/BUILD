load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

genrule(
  name = "gen",
  tools = [
    "//mage/parser:gen",
    "child_process.magen",
    "child_process_2.magen",
    "parent_process.magen",
  ],
  outs = [
    "child_process.magen.h",
    "child_process_2.magen.h",
    "parent_process.magen.h",
  ],
  cmd = " && ".join([
    # Follow the pattern below:
    # python3 $(location //generated:gen) <input *.mage file> <output *.magen.h file>
    "python3 $(location //mage/parser:gen) $(location child_process.magen) $(location child_process.magen.h)",
    "python3 $(location //mage/parser:gen) $(location child_process_2.magen) $(location child_process_2.magen.h)",
    "python3 $(location //mage/parser:gen) $(location parent_process.magen) $(location parent_process.magen.h)",
  ]),
)

cc_library(
  name = "include",
  hdrs = [":gen"]
)
