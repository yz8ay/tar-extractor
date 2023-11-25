#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>

#include "tarball_unpacker.hpp"

const auto header_zero = std::make_unique<Header>();

bool Header::IsFilledWithZero() {
  if (!memcmp(this, header_zero.get(), sizeof(*this))) {
    return true;
  }
  return false;
}

TarballUnpacker::TarballUnpacker() {}

std::ifstream TarballUnpacker::OpenFile(const std::string& path) {
  namespace fs = std::filesystem;
  if (!fs::is_regular_file(path)) {
    std::cerr << fs::weakly_canonical(fs::absolute(path)) << " is not a regular file" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  std::ifstream ifs{path, std::ios::binary | std::ios::in};
  if (!ifs.is_open()) {
    std::cerr << "Failed to open file: " << fs::weakly_canonical(fs::absolute(path)) << std::endl;
    std::exit(EXIT_FAILURE);
  }

  return ifs;
}

void TarballUnpacker::Unpack(const std::string& path) {
  auto ifs = OpenFile(path);
  int header_zero_count = 0;

  for (;;) {
    auto header = std::make_unique<Header>();
    ifs.read(reinterpret_cast<char*>(header.get()), sizeof(Header));

    // check whether two consecutive blocks are filled with 0
    if (header->IsFilledWithZero()) {
      if (header_zero_count == 1) {
        break;  // end of file
      }
      ++header_zero_count;
      continue;
    } else if (header_zero_count == 1) {
      std::cerr << "Header error" << std::endl;
      std::exit(EXIT_FAILURE);
    }

    ReadHeader(ifs, std::move(header));
  }
}

int TarballUnpacker::OctalStringToInt(char* string, int size) {
  int res = 0;
  int pos = 0;
  size -= 2;  // count NUL
  while (size > 0) {
    res += (string[size--]  - '0') * (1 << (3 * pos++));
  }

  return res;
}

void TarballUnpacker::ReadHeader(std::ifstream& ifs, const std::unique_ptr<Header> header, std::string long_path) {
  if (memcmp(header->ustar_indicator, "ustar", 5) ||
      memcmp(header->ustar_version, "00", 2)) {
    std::cerr << "Unsupported ustar format" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::string path = header->file_name;
  if (!long_path.empty()) {
    path = long_path;
  }

  switch (header->file_type) {
  case '0': // normal file
    MakeNormalFile(ifs, path, OctalStringToInt(header->file_size, 12));
    break;
  case '1': // hard link
    MakeHardLink(header->name_of_linked_file, path);
    break;
  case '2': // symbolic link
    MakeSymbolicLink(header->name_of_linked_file, path);
    break;
  case '5': // directory
    MakeDirectory(path);
    break;
  case 'x': // extended header
    ProcessExtendedHeader(ifs);
    break;
  default:
    std::cerr << "Unsupported file type: " << header->file_type << std::endl;
    std::exit(EXIT_FAILURE);
  }

  if (header->file_type != 'x')
    std::cout << path << std::endl;
}

std::string TarballUnpacker::ReadExtendedHeader(std::ifstream& ifs) {
  auto tmp = std::make_unique<Header>();
  ifs.read(reinterpret_cast<char*>(tmp.get()), sizeof(Header));
  std::string extended_header_data{reinterpret_cast<char*>(tmp.get())};
  size_t start_pos = extended_header_data.find("path=");
  if (start_pos == std::string::npos) {
    std::cerr << "Extended header error" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  start_pos += std::strlen("path=");
  size_t end_pos = extended_header_data.find('\n', start_pos);
  if (end_pos == std::string::npos) {
    std::cerr << "Extended header error" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  std::string long_file_name = extended_header_data.substr(start_pos, end_pos - start_pos);

  return long_file_name;
}

void TarballUnpacker::MakeNormalFile(std::ifstream& ifs, std::string path, int file_size) {
  auto file_content = std::make_unique<char[]>(file_size);
  ifs.read(file_content.get(), file_size);
  std::ofstream file{path};
  file.write(file_content.get(), file_size);
  size_t over_read_size = file_size % sizeof(Header);
  if (over_read_size != 0)
    ifs.ignore(sizeof(Header) - over_read_size);  // move to next block
}

void TarballUnpacker::MakeHardLink(std::string file_name, std::string path) {
  std::filesystem::create_hard_link(file_name, path);
}

void TarballUnpacker::MakeSymbolicLink(std::string file_name, std::string path) {
  std::filesystem::create_symlink(file_name, path);
}

void TarballUnpacker::MakeDirectory(std::string path) {
  std::size_t last_slash_index = path.rfind('/');
  std::string directory_path = path.substr(0, last_slash_index);
  std::filesystem::create_directories(directory_path);
}

void TarballUnpacker::ProcessExtendedHeader(std::ifstream& ifs) {
  // Read extended header
  std::string long_file_name = ReadExtendedHeader(ifs);

  // Read normal header using long file name
  auto header = std::make_unique<Header>();
  ifs.read(reinterpret_cast<char*>(header.get()), sizeof(Header));
  ReadHeader(ifs, std::move(header), long_file_name);
}
