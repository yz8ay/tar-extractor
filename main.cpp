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

static const auto header_zero = std::make_unique<Header>();
static bool IsFilledWithZero(const std::unique_ptr<Header>& header) {
  if (!memcmp(header.get(), header_zero.get(), sizeof(Header))) {
    return true;
  }
  return false;
}

static void CheckHeader(std::ifstream& ifs, const std::unique_ptr<Header>& header, std::string long_path = "") {
  std::string path;
  if (long_path == "") {
    path = header->file_name;
  } else {
    path = long_path;
  }
  std::size_t last_slash_index = path.rfind('/');
  std::string directory_path = path.substr(0, last_slash_index);
  int file_size = OctalStringToInt(header->file_size, 12);

  switch (header->file_type) {
    case '0': { // normal file
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
    case 'x': { // extended header
      // Read extended header data
      auto tmp = std::make_unique<Header>();
      ifs.read(reinterpret_cast<char*>(tmp.get()), sizeof(Header));
      std::string extended_header_data{reinterpret_cast<char*>(tmp.get())};
      size_t start_pos = extended_header_data.find("path=");
      if (start_pos == std::string::npos) {
        std::cerr << "Extended header error" << std::endl;
        std::exit(EXIT_FAILURE);
      }
      start_pos += 5;
      size_t end_pos = extended_header_data.find('\n', start_pos);
      if (end_pos == std::string::npos) {
        std::cerr << "Extended header error" << std::endl;
        std::exit(EXIT_FAILURE);
      }
      std::string long_file_name = extended_header_data.substr(start_pos, end_pos - start_pos);

      // Read normal header using long file name
      auto header = std::make_unique<Header>();
      ifs.read(reinterpret_cast<char*>(header.get()), sizeof(Header));
      CheckHeader(ifs, header, long_file_name);
      break;
    }
    default:
      std::cerr << "Unsupported file type: " << header->file_type << std::endl;
      std::exit(EXIT_FAILURE);
  }

  if (header->file_type != 'x')
    std::cout << path << std::endl;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <tar file>" << std::endl;
    return 1;
  }
  if (!std::filesystem::is_regular_file(argv[1])) {
    std::cerr << argv[1] << " is not a regular file" << std::endl;
    return 1;
  }
  std::ifstream ifs{argv[1], std::ios::binary | std::ios::in};
  if (!ifs.is_open()) {
    std::cerr << "Failed to open file: " << argv[1] << std::endl;
    return 1;
  }

  int header_zero_count = 0;
  for (;;) {
    auto header = std::make_unique<Header>();
    ifs.read(reinterpret_cast<char*>(header.get()), sizeof(Header));

    // check whether two consecutive blocks are filled with 0
    if (IsFilledWithZero(header)) {
      if (header_zero_count == 1) {
        break;  // end of file
      }
      ++header_zero_count;
      continue;
    } else if (header_zero_count == 1) {
      std::cerr << "Header error" << std::endl;
      return 1;
    }

    CheckHeader(ifs, header);
  }
}
