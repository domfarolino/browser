def magen_build(name, srcs):
  outputs = []
  cmds = []
  for src in srcs:
    output = src + ".h"
    outputs.append(output)
    cmds.append("python3 $(location //mage/parser:gen) $(location %s) $(location %s)" % (src, output))

  native.genrule(
    name = name,
    outs = outputs,
    tools = ["//mage/parser:gen"] + srcs,
    cmd = " && ".join(cmds)
  )
