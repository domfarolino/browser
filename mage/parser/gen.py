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

# Create a string out of the parsed source parameters.

generated_class = """
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
class %s {
 public:
  virtual void %s() = 0;
};

////////////////////////////////////////////////////////////////////////////////

/* For each method on the interface we want to generate the following kind of class*/
// These classes carry the arguments
class %s_%s_Params {
 public:
  %s_%s_Params() : bytes(sizeof(*this)) {}
  int bytes;

  /* For each argument in the method, we want a C++ member representing it. */
};

////////////////////////////////////////////////////////////////////////////////

// Instances of this class are what the mage::Remote<T> calls into to serialize and send messages.
class %sProxy {
 public:
  void %s() {
    // Serialize the message data.
    std::vector<char> buffer(sizeof(%s_%s_Params));
    new (buffer.data()) %s_%s_Params();
    auto params = Get<%s_%s_Params>(buffer.data());
    params->bytes = params->bytes;
  }
};

} // namespace magen.
""" % (
        source_interface_name,
        source_interface_method_name,
        source_interface_name,
        source_interface_method_name,
        source_interface_name,
        source_interface_method_name,
        source_interface_name,
        source_interface_method_name,
        source_interface_name,
        source_interface_method_name,
        source_interface_name,
        source_interface_method_name,
        source_interface_name,
        source_interface_method_name,
      )

destination = sys.argv[1:][1]
text_file = open(destination, "w")
text_file.writelines(generated_class)
text_file.writelines('\n')
text_file.close()

print("OK, the header has been generated")
