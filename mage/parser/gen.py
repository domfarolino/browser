from jinja2 import Template

import sys
print(sys.argv[1:]) # For debugging

# Parse the source.
source_location = sys.argv[1:][0]
source = open(source_location, "r")

source_text = source.read()
source_text = source_text.replace("(", " ")
source_text = source_text.replace(")", "")
source_text = ''.join(e for e in source_text if (e.isalnum() or e.isspace()))
print(source_text)
source_text_split = source_text.split()
print(source_text_split)

source_interface_name = source_text_split[1]
source_interface_method_name = source_text_split[2]
arguments = source_text_split[3:]
arguments = zip(arguments[::2], arguments[1::2])
# it = iter(source_text_split[3:])
# arguments = zip(it, it)
#for argument_pair in arguments:
  # print(argument_pair)
print(arguments)

source.close()

def GetNativeType(magen_type):
  if magen_type == "string":
    return "std::string"
  return magen_type

def GetMagenParamsType(magen_type):
  if magen_type == "string":
    return "mage::Pointer<mage::ArrayHeader<char>>"
  return magen_type

def IsArrayType(magen_type):
  if magen_type == "string":
    return True
  return False

# Generate a C++ header out of the parsed source parameters.

generated_magen_template = Template("""
// This file is generated at build-time. Do not edit it.

#include "mage/core/message.h"

#include <string>
#include <vector>

namespace magen {

////////////////////////////////////////////////////////////////////////////////

/* For each method on the interface we want to generate the following kind of class*/
// These classes carry the arguments
class {{Interface}}_{{Method}}_Params {
 public:
  {{Interface}}_{{Method}}_Params() : bytes(sizeof(*this)) {}
  int bytes;
{%- for argument_pair in Arguments %}
  {{ GetMagenParamsType(argument_pair[0]) }} {{ argument_pair[1] }};
{%- endfor %}
};

////////////////////////////////////////////////////////////////////////////////

// Instances of this class are what the mage::Remote<T> calls into to serialize and send messages.
class {{Interface}}Proxy {
 public:
  {{Interface}}Proxy() {}
  void {{Method}}(
  {%- for argument_pair in Arguments %}
    {{ GetNativeType(argument_pair[0]) }} {{ argument_pair[1] }}{% if not loop.last %},{% endif %}
  {%- endfor %}
  ) {

    // Serialize the message data.
    mage::Message message(mage::MessageType::USER_MESSAGE);
    mage::MessageFragment<{{Interface}}_{{Method}}_Params> message_fragment(message);
    message_fragment.Allocate();
    {{Interface}}_{{Method}}_Params* params = message_fragment.data();

{%- for argument_pair in Arguments %}
  {% if not IsArrayType(argument_pair[0]) %}
    params->{{ argument_pair[1] }} = {{ argument_pair[1] }};
  {% else %}
    {
      // Create a new array message fragment.

      // TODO(domfarolino): Don't hardcode char. Introduce some "GetBaseType()"
      // method, or better yet, defer to C++ like mojo does.
      mage::MessageFragment<mage::ArrayHeader<char>> array(message);
      array.AllocateArray({{ argument_pair[1] }}.size());
      memcpy(array.data()->array_storage(), {{ argument_pair[1] }}.c_str(), {{ argument_pair[1] }}.size());
      params->{{ argument_pair[1] }}.Set(array.data());
    }
  {% endif %}
{%- endfor %}



    message.FinalizeSize();
  }
};

////////////////////////////////////////////////////////////////////////////////

// The class that user implementations of the interface will implement.
class {{Interface}} {
 public:
  // This is so that mage::Remotes can reference the proxy class.
  using Proxy = {{Interface}}Proxy;

  virtual void {{Method}}(
  {%- for argument_pair in Arguments %}
    {{ GetNativeType(argument_pair[0]) }} {{ argument_pair[1] }}{% if not loop.last %},{% endif %}
  {%- endfor %}
  ) = 0;
};



} // namespace magen.
""")
generated_magen_template = generated_magen_template.render(
                             Interface=source_interface_name,
                             Method=source_interface_method_name,
                             Arguments=list(arguments), # https://stackoverflow.com/questions/13651965/.
                             GetNativeType=GetNativeType,
                             GetMagenParamsType=GetMagenParamsType,
                             IsArrayType=IsArrayType,
                           )

destination = sys.argv[1:][1]
text_file = open(destination, "w")
text_file.writelines(generated_magen_template)
text_file.writelines('\n')
text_file.close()

print("OK, the header has been generated")
