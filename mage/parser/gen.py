from jinja2 import Template

import sys
print(sys.argv[1:]) # For debugging

# Parse the source.
source_location = sys.argv[1:][0]
source = open(source_location, "r")

source_text = source.read()
source_text = ''.join(e for e in source_text if (e.isalnum() or e.isspace()))
source_text_split = source_text.split()
print(source_text_split)

source_interface_name = source_text_split[1]
source_interface_method_name = source_text_split[2]

source.close()


# Generate a C++ header out of the parsed source parameters.

generated_magen_template = Template("""
// This file is generated at build-time. Do not edit it.

#include <vector>

namespace {

template <typename T>
T* Get(char* buffer) {
  return reinterpret_cast<T*>(buffer);
}

}; // namespace

namespace magen {

// The class that user implementations of the interface will implement.
class {{Interface}} {
 public:
  virtual void {{Method}}() = 0;
};

////////////////////////////////////////////////////////////////////////////////

/* For each method on the interface we want to generate the following kind of class*/
// These classes carry the arguments
class {{Interface}}_{{Method}}_Params {
 public:
  {{Interface}}_{{Method}}_Params() : bytes(sizeof(*this)) {}
  int bytes;

  /* For each argument in the method, we want a C++ member representing it. */
};

////////////////////////////////////////////////////////////////////////////////

// Instances of this class are what the mage::Remote<T> calls into to serialize and send messages.
class {{Interface}}Proxy {
 public:
  void {{Method}}() {
    // Serialize the message data.
    int payload_length = sizeof({{Interface}}_{{Method}}_Params);
    std::vector<char> buffer(payload_length);

    // Instantiate a new {{Interface}}_{{Method}} over the buffer.
    new (buffer.data()) {{Interface}}_{{Method}}_Params();
    auto params = Get<{{Interface}}_{{Method}}_Params>(buffer.data());
    params->bytes = params->bytes;
  }
};

} // namespace magen.
""")
generated_magen_template = generated_magen_template.render(
                             Interface=source_interface_name,
                             Method=source_interface_method_name
                           )

destination = sys.argv[1:][1]
text_file = open(destination, "w")
text_file.writelines(generated_magen_template)
text_file.writelines('\n')
text_file.close()

print("OK, the header has been generated")
