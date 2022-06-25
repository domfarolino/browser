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

// We give each message, which corresponds to each method of the {{Interface}}
// interface, an ID that is unique within the interface. This allows
// {{Interface}}ReceiverStub::OnReceivedMessage() to distinguish between the
// messages, and go down deserialization path and invoke the right method on the
// interface implementation.
{%- for Method in Methods %}
static const int {{Interface}}_{{Method.name}}_ID = {{loop.index0}};
{%- endfor %}

////////////////////////////////////////////////////////////////////////////////

// This is the abstract class that user implementations of the interface will
// implement.
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

// Instances of this class are what mage::Remote<magen::{{Interface}}> objects
// send their messages to. Message serialization is handled by this class.
class {{Interface}}Proxy {
 public:
  void BindToHandle(mage::MageHandle local_handle) {
    CHECK(!bound_);
    bound_ = true;
    local_handle_ = local_handle;
  }

  {{Interface}}Proxy() = default;

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

{# At template substition time we generate the total number of handles that a
   message contains; this is better than doing it at runtime by looping in C++.
   TODO(domfarolino): Maybe get rid of this since it does not seem necessary. #}
{% set num_endpoints_in_message_jinja = [] %}
{%- for argument_pair in Method.arguments %}
  {% if IsHandleType(argument_pair[0]) %}
    {% set __ = num_endpoints_in_message_jinja.append(1) %}
  {% endif %}
{%- endfor%}
    // Pre-compute the number of handles this message is going to send, if any.
    const int num_endpoints_in_message = {{ num_endpoints_in_message_jinja|length }};
    std::vector<mage::EndpointDescriptor> endpoints_to_write;
    // End pre-compute.

{%- for argument_pair in Method.arguments %}
  {% if not IsArrayType(argument_pair[0]) and not IsHandleType(argument_pair[0]) %}
    message_fragment.data()->{{ argument_pair[1] }} = {{ argument_pair[1] }};
  {% elif IsArrayType(argument_pair[0]) %}
    {
      // Create a new array message fragment.

      // TODO(domfarolino): Don't hardcode char. Introduce some "GetBaseType()"
      // method, or better yet, defer to C++ like mojo does.
      mage::MessageFragment<mage::ArrayHeader<char>> array(message);
      array.AllocateArray({{ argument_pair[1] }}.size());
      memcpy(array.data()->array_storage(), {{ argument_pair[1] }}.c_str(), {{ argument_pair[1] }}.size());
      message_fragment.data()->{{ argument_pair[1] }}.Set(array.data());
    }
  {% elif IsHandleType(argument_pair[0]) %}
    {
      // Take that handle that we're sending, find its underlying `Endpoint`, and
      // use it to fill out a `mage::EndpointDescriptor` which is written to the message
      // buffer.
      mage::EndpointDescriptor& endpoint_descriptor_to_populate = message_fragment.data()->{{ argument_pair[1] }};
      mage::Core::PopulateEndpointDescriptorAndMaybeSetEndpointInProxyingState({{argument_pair[1]}}, local_handle_, endpoint_descriptor_to_populate);
      endpoints_to_write.push_back(endpoint_descriptor_to_populate);
    }
  {% endif %}
{%- endfor %}

    message.GetMutableMessageHeader().user_message_id = {{Interface}}_{{Method.name}}_ID;

    // Write the endpoints last in the header. See the documentation above
    // `mage::MessageHeader::endpoints_in_message` to see why this is necessary.
    // TODO(domfarolino): Get rid fo this printf.
    printf(\"endpoints_to_write.size(): %lu\\n\", endpoints_to_write.size());
    CHECK_EQ(num_endpoints_in_message, endpoints_to_write.size());
    mage::MessageFragment<mage::ArrayHeader<mage::EndpointDescriptor>> endpoint_array_at_end_of_message(message);
    endpoint_array_at_end_of_message.AllocateArray(num_endpoints_in_message);
    for (int i = 0; i < num_endpoints_in_message; ++i) {
      endpoints_to_write[i].Print();
      char* endpoint_as_bytes = reinterpret_cast<char*>(&endpoints_to_write[i]);
      // Write the endpoint.
      memcpy(endpoint_array_at_end_of_message.data()->array_storage() + i,
             /*source=*/endpoint_as_bytes,
             /*source_size=*/sizeof(mage::EndpointDescriptor));
    }
    message.GetMutableMessageHeader().endpoints_in_message.Set(endpoint_array_at_end_of_message.data());
    // End writing endpoints.

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
  void BindToHandle(mage::MageHandle local_handle, {{Interface}}* impl, std::shared_ptr<base::TaskRunner> impl_task_runner) {
    CHECK(!bound_);
    bound_ = true;
    local_handle_ = local_handle;
    impl_ = impl;

    // Set outselves up as the official delegate for the underlying endpoint
    // associated with |local_handle_|. That way any messages it receives, we'll
    // be able to deserialize and forward to the implementation.
    mage::Core::BindReceiverDelegateToEndpoint(local_handle, this, std::move(impl_task_runner));
  }

  // mage::Endpoint::ReceiverDelegate implementation.
  // This is what deserializes the message and dispatches the correct method to
  // the interface implementation.
  void OnReceivedMessage(mage::Message message) override {
    int user_message_id = message.GetMutableMessageHeader().user_message_id;

    // Here is where we determine which mage method this message is for.
    {%- for Method in Methods %}
      if (user_message_id == {{Interface}}_{{Method.name}}_ID) {
        // Get an appropriate view over |message|.
        {{Interface}}_{{Method.name}}_Params* params = message.GetView<{{Interface}}_{{Method.name}}_Params>();
        CHECK(params);

        // Initialize the variables for the method arguments.
        {%- for argument_pair in Method.arguments %}
          {{ GetNativeType(argument_pair[0]) }} {{ argument_pair[1] }};
        {%- endfor %}

        // Deserialize each argument into its corresponding variable above.
        {%- for argument_pair in Method.arguments %}
          {% if not IsArrayType(argument_pair[0]) and not IsHandleType(argument_pair[0]) %}
            {{ argument_pair[1] }} = params->{{ argument_pair[1] }};
          {% elif IsArrayType(argument_pair[0]) %}
            {{ argument_pair[1] }} = std::string(
              params->{{ argument_pair[1] }}.Get()->array_storage(),
              params->{{ argument_pair[1] }}.Get()->array_storage() + params->{{ argument_pair[1] }}.Get()->num_elements
            );
          {% elif IsHandleType(argument_pair[0]) %}
            // The handle and endpoint have already been processed on the IO
            // thread, so we can just grab the handle directly from the message.
            {{ argument_pair[1] }} = message.TakeNextHandle();
          {% endif %}
        {%- endfor %}

        impl_->{{Method.name}}(
          {%- for argument_pair in Method.arguments %}
            {{ argument_pair[1] }}{% if not loop.last %},{% endif %}
          {%- endfor %}
        );

        return;
      }
    {%- endfor %}

    // If we get here, that means |message|'s |user_message_id| did not match
    // method in {{Interface}}, so the message cannot be deserialized and
    // dispatched. This can only happen when we received a message for the wrong
    // mage interface.
    NOTREACHED();
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
                             IsHandleType=IsHandleType,
                           )

destination = sys.argv[1:][1]
text_file = open(destination, "w")
text_file.writelines(generated_magen_template)
text_file.writelines('\n')
text_file.close()

print("OK, the header has been generated")
