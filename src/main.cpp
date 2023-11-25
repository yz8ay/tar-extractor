#include <iostream>
#include <memory>

#include "tarball_unpacker.hpp"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <tar file>" << std::endl;
    return 1;
  }

  auto unpacker = std::make_unique<TarballUnpacker>();
  unpacker->Unpack(argv[1]);
}
