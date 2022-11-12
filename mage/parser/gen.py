from jinja2 import Template

import sys

######################################################################## HELPERS
class Method:
  def __init__(self, name, arguments):
    self.name = name
    self.arguments = arguments

def GetInterfaceName(interface_descriptor_string):
  return interface_descriptor_string.split()[1]

def GetListOfMethods(method_descriptors):
  return_list = []
  for method_descriptor in method_descriptors:
    method_descriptor = method_descriptor.replace(',', '')
    # Pull the method name
    split = method_descriptor.split()
    method_name = split[0]
    split = split[1:]

    # Pull the types and names
    types = split[::2]
    names = split[1::2]
    print(types, names)
    # See https://stackoverflow.com/questions/13651965/ for why we have to wrap
    # list(zip(...)) below.
    return_list.append(Method(method_name, list(zip(types, names))));
  return return_list
#################################################################### END HELPERS

print(sys.argv[1:]) # For debugging

################################################################## START PARSING
# Parse the source.
source_location = sys.argv[1:][0]
source = open(source_location, "r")

source_text = source.read()
source_text = source_text.replace("(", " ")
source_text = source_text.replace(")", " ")

separate_interface_from_methods = source_text.split("{")
assert len(separate_interface_from_methods) == 2

interface_descriptor = [separate_interface_from_methods[0]] + separate_interface_from_methods[1].split(";")
for i in range(len(interface_descriptor)):
  interface_descriptor[i] = interface_descriptor[i].strip(' \n\t')

assert interface_descriptor[-1] == '}'
interface_descriptor.pop()
print(interface_descriptor)
interface_name = GetInterfaceName(interface_descriptor[0])
list_of_methods = GetListOfMethods(interface_descriptor[1:])
print(list_of_methods)
#################################################################### END PARSING

source.close()

##################################################### HELPERS USED IN TEMPLATING
def GetNativeType(magen_type):
  if magen_type == "string":
    return "std::string"
  elif magen_type == "MageHandle":
    return "mage::MageHandle"
  return magen_type

def GetMagenParamsType(magen_type):
  if magen_type == "string":
    return "mage::Pointer<mage::ArrayHeader<char>>"
  elif magen_type == "MageHandle":
    return "mage::EndpointDescriptor"
  return magen_type

def IsArrayType(magen_type):
  if magen_type == "string":
    return True
  return False

def IsHandleType(magen_type):
  if magen_type == "MageHandle":
    return True
  return False
################################################# END HELPERS USED IN TEMPLATING

# Generate a C++ header from the parsed source parameters.

with open('classes.tmpl-created') as f:
  generated_magen_template = Template(f.read())

generated_magen_template = generated_magen_template.render(
                             Interface=interface_name,
                             Methods=list_of_methods,
                             GetNativeType=GetNativeType,
                             GetMagenParamsType=GetMagenParamsType,
                             IsArrayType=IsArrayType,
                             IsHandleType=IsHandleType,
                           )

destination = sys.argv[1:][1]
text_file = open(destination, "w")
text_file.writelines(generated_magen_template)
text_file.writelines('\n')
text_file.close()

print("OK, the header has been generated")
