#include <iostream>

#include "base/callback.h"

int main() {
  base::OnceClosure closure = [](){
    std::cout << "OnceClosure() is being invoked()" << std::endl;
  };
  closure();
  return 0;
}
