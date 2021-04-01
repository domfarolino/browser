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
  return magen_type

def GetMagenParamsType(magen_type):
  if magen_type == "string":
    return "mage::Pointer<mage::ArrayHeader<char>>"
  return magen_type

def IsArrayType(magen_type):
  if magen_type == "string":
    return True
  return False
################################################# END HELPERS USED IN TEMPLATING

# Generate a C++ header out of the parsed source parameters.

generated_magen_template = Template("""
// This file is generated at build-time. Do not edit it.

#include <string>
#include <vector>

#include "base/check.h"
#include "mage/core/endpoint.h"
#include "mage/core/handles.h"
#include "mage/core/message.h"

namespace magen {

class {{Interface}}Proxy;
class {{Interface}}ReceiverStub;

////////////////////////////////////////////////////////////////////////////////

// The class that user implementations of the interface will implement.
class {{Interface}} {
 public:
  virtual ~{{Interface}}() = default;
  // This is so that mage::Remotes can reference the proxy class.
  using Proxy = {{Interface}}Proxy;
  using ReceiverStub = {{Interface}}ReceiverStub;

  {%- for Method in Methods %}
    virtual void {{Method.name}}(
    {%- for argument_pair in Method.arguments %}
        {{ GetNativeType(argument_pair[0]) }} {{ argument_pair[1] }}{% if not loop.last %},{% endif %}
    {%- endfor %}
    ) = 0;
  {%- endfor %}
};

////////////////////////////////////////////////////////////////////////////////

// For each method on the interface we want to generate the following kind of
// class which is used when serializing and deserializing the message arguments.

{%- for Method in Methods %}
class {{Interface}}_{{Method.name}}_Params {
 public:
  {{Interface}}_{{Method.name}}_Params() : bytes(sizeof(*this)) {}
  int bytes;
{%- for argument_pair in Method.arguments %}
  {{ GetMagenParamsType(argument_pair[0]) }} {{ argument_pair[1] }};
{%- endfor %}
};
{%- endfor %}
////////////////////////////////////////////////////////////////////////////////

// Instances of this class are what the mage::Remote<T> calls into to serialize and send messages.
class {{Interface}}Proxy {
 public:
  void BindToHandle(mage::MageHandle local_handle) {
    CHECK(!bound_);
    bound_ = true;
    local_handle_ = local_handle;
  }

  {{Interface}}Proxy() {}

  {%- for Method in Methods %}
  void {{Method.name}}(
  {%- for argument_pair in Method.arguments %}
    {{ GetNativeType(argument_pair[0]) }} {{ argument_pair[1] }}{% if not loop.last %},{% endif %}
  {%- endfor %}
  ) {

    CHECK(bound_);

    // Serialize the message data.
    mage::Message message(mage::MessageType::USER_MESSAGE);
    mage::MessageFragment<{{Interface}}_{{Method.name}}_Params> message_fragment(message);
    message_fragment.Allocate();

{%- for argument_pair in Method.arguments %}
  {% if not IsArrayType(argument_pair[0]) %}
    message_fragment.data()->{{ argument_pair[1] }} = {{ argument_pair[1] }};
  {% else %}
    {
      // Create a new array message fragment.

      // TODO(domfarolino): Don't hardcode char. Introduce some "GetBaseType()"
      // method, or better yet, defer to C++ like mojo does.
      mage::MessageFragment<mage::ArrayHeader<char>> array(message);
      array.AllocateArray({{ argument_pair[1] }}.size());
      memcpy(array.data()->array_storage(), {{ argument_pair[1] }}.c_str(), {{ argument_pair[1] }}.size());
      message_fragment.data()->{{ argument_pair[1] }}.Set(array.data());
    }
  {% endif %}
{%- endfor %}

    message.FinalizeSize();
    mage::Core::SendMessage(local_handle_, std::move(message));
  }
  {%- endfor %}

 private:
  bool bound_;
  mage::MageHandle local_handle_; // Only set when |bound_| is true.
};

////////////////////////////////////////////////////////////////////////////////

class {{Interface}}ReceiverStub : public mage::Endpoint::ReceiverDelegate {
 public:
  void BindToHandle(mage::MageHandle local_handle, {{Interface}}* impl) {
    CHECK(!bound_);
    bound_ = true;
    local_handle_ = local_handle;
    impl_ = impl;

    // Set outselves up as the official delegate for the underlying endpoint
    // associated with |local_handle_|. That way any messages it receives, we'll
    // be able to deserialize and forward to the implementation.
    mage::Core::BindReceiverDelegateToEndpoint(local_handle, this);
  }

  // mage::Endpoint::ReceiverDelegate implementation.
  // This is what deserializes the message and dispatches the correct method to
  // the interface implementation.
  void OnReceivedMessage(mage::Message message) override {
    // Get an appropriate view over |message|.
    {{Interface}}_{{Methods[0].name}}_Params* params = message.Get<{{Interface}}_{{Methods[0].name}}_Params>(/*index=*/0);
    CHECK(params);

    // Initialize the variables for the method arguments.
    {%- for argument_pair in Methods[0].arguments %}
      {{ GetNativeType(argument_pair[0]) }} {{ argument_pair[1] }};
    {%- endfor %}

    // Deserialize each argument into its corresponding variable above.
    {%- for argument_pair in Methods[0].arguments %}
      {% if not IsArrayType(argument_pair[0]) %}
        {{ argument_pair[1] }} = params->{{ argument_pair[1] }};
      {% else %}
        {{ argument_pair[1] }} = std::string(
          params->{{ argument_pair[1] }}.Get()->array_storage(),
          params->{{ argument_pair[1] }}.Get()->array_storage() + params->{{ argument_pair[1] }}.Get()->num_elements
        );
      {% endif %}
    {%- endfor %}

    impl_->{{Methods[0].name}}(
      {%- for argument_pair in Methods[0].arguments %}
        {{ argument_pair[1] }}{% if not loop.last %},{% endif %}
      {%- endfor %}
    );
  }

 private:
  bool bound_;
  mage::MageHandle local_handle_;
  {{Interface}}* impl_;
};

////////////////////////////////////////////////////////////////////////////////



} // namespace magen.
""")
generated_magen_template = generated_magen_template.render(
                             Interface=interface_name,
                             Methods=list_of_methods,
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
