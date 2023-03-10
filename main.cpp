#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <cmath>

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
  char padding[255];
} __attribute__ ((__packed__));

int OctalStringToInt(char* string, int size) {
  int res = 0;
  int pos = 0;
  size -= 2;  // count NUL
  while (size > 0) {
    res += (string[size--]  - '0') * static_cast<int>(pow(8, pos++));
  }
  return res;
}

bool IsFilledWithZero(Header* header) {
  Header header_zero{0};
  if (!memcmp(header, &header_zero, sizeof(Header))) {
    return true;
  } else {
    return false;
  }
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Argument error" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::ifstream ifs(argv[1], std::ios::binary | std::ios::in);
  if (!ifs.is_open()) {
    std::cerr << "Cannot open file" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  int header_zero_count = 0;

  for (;;) {
    char data, header_buf[512];

    for (int i = 0; i < 512; ++i) {
      ifs.get(data);
      header_buf[i] = data;
    }
    Header* header = reinterpret_cast<Header*>(header_buf);

    // check whether two consecutive blocks are filled with 0
    if (IsFilledWithZero(header)) {
      if (header_zero_count == 1) {
        break;  // end of file
      }
      ++header_zero_count;
      continue;
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
        char* file_content = (char*)malloc(file_size);
        for (int i = 0; i < file_size; ++i) {
          ifs.get(data);
          file_content[i] = data;
        }
        std::ofstream file(path);
        file.write(file_content, file_size);
        free(file_content);
        ifs.ignore(512 - file_size % 512);  // move to next block
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
        break;
    }
    std::cout << path << std::endl;
  }
}