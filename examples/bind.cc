#include <iostream>

#include "base/callback.h"

void FunctionA(int n) {
  std::cout << "FunctionA called with n = " << n << std::endl;
}

void FunctionB(base::OnceClosure cb) {
  cb();
}

int main() {
  base::OnceClosure closure = base::BindOnce(FunctionA, 101);
  FunctionB(std::move(closure));
  return 0;
}
