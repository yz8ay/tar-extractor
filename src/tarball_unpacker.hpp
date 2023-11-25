#pragma once

#include <cstring>
#include <memory>

struct Header {
  Header() {
    std::memset(this, 0, sizeof(*this));
  }

  bool IsFilledWithZero();

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

class TarballUnpacker {
 public:
  TarballUnpacker();

  void Unpack(const std::string& path);

 private:
  std::ifstream OpenFile(const std::string& path);
  int OctalStringToInt(char* string, int size);

  void ReadHeader(std::ifstream& ifs, const std::unique_ptr<Header> header, std::string long_path = "");
  std::string ReadExtendedHeader(std::ifstream& ifs);
  void MakeNormalFile(std::ifstream& ifs, std::string path, int file_size);
  void MakeHardLink(std::string file_name, std::string path);
  void MakeSymbolicLink(std::string file_name, std::string path);
  void MakeDirectory(std::string path);
  void ProcessExtendedHeader(std::ifstream& ifs);
};
