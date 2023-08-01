#include <cstring>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>

struct Header {
  char file_name[100];
  char file_mode[8];
  char owner_numeric_user_id[8];
  char group_numeric_user_id[8];
  char file_size[12];
  char last_mod_unix_time[12];
  char checksum_for_header_block[8];
  char file_type;
  char name_of_linked_file[100];
  char ustar_indicator[6];
  char ustar_version[2];
  char owner_user_name[32];
  char owner_group_name[32];
  char device_major_number[8];
  char device_minor_number[8];
  char file_name_prefix[155];
  char padding[12];
} __attribute__ ((__packed__));

static_assert(sizeof(Header) == 512, "Header size must be 512 bytes");

static int OctalStringToInt(char* string, int size) {
  int res = 0;
  int pos = 0;
  size -= 2;  // count NUL
  while (size > 0) {
    res += (string[size--]  - '0') * (1 << (3 * pos++));
  }
  return res;
}

static const Header header_zero{0};
static bool IsFilledWithZero(const std::unique_ptr<Header>& header) {
  if (!memcmp(header.get(), &header_zero, sizeof(Header))) {
    return true;
  }
  return false;
}

static bool CheckHeader(std::ifstream& ifs, int& header_zero_count) {
  auto header = std::make_unique<Header>();
  ifs.read(reinterpret_cast<char*>(header.get()), sizeof(Header));

  // check whether two consecutive blocks are filled with 0
  if (IsFilledWithZero(header)) {
    if (header_zero_count == 1) {
      return true;  // end of file
    }
    ++header_zero_count;
    return false;
  } else if (header_zero_count == 1) {
    std::cerr << "Header error" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::string path{header->file_name};
  std::size_t last_slash_index = path.rfind('/');
  std::string directory_path = path.substr(0, last_slash_index);
  int file_size = OctalStringToInt(header->file_size, 12);

  switch (header->file_type) {
    case '0':
    case '\0': {  // normal file
      auto file_content = std::make_unique<char[]>(file_size);
      ifs.read(file_content.get(), file_size);
      std::ofstream file{path};
      file.write(file_content.get(), file_size);
      ifs.ignore(sizeof(Header) - file_size % sizeof(Header));  // move to next block
      break;
    }
    case '1': // hard link
      std::filesystem::create_hard_link(header->name_of_linked_file, path);
      break;
    case '2': // symbolic link
      std::filesystem::create_symlink(header->name_of_linked_file, path);
      break;
    case '5': // directory
      std::filesystem::create_directories(directory_path);
      break;
    default:
      std::cerr << "Unsupported file type: " << header->file_type << std::endl;
      std::exit(EXIT_FAILURE);
  }
  std::cout << path << std::endl;
  return false;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Argument error" << std::endl;
    return 1;
  }
  if (!std::filesystem::is_regular_file(argv[1])) {
    std::cerr << "Not a regular file" << std::endl;
    return 1;
  }
  std::ifstream ifs{argv[1], std::ios::binary | std::ios::in};
  if (!ifs.is_open()) {
    std::cerr << "Failed to open file" << std::endl;
    return 1;
  }

  int header_zero_count = 0;
  for (;;) {
    bool end = CheckHeader(ifs, header_zero_count);
    if (end)
      break;
  }
}
