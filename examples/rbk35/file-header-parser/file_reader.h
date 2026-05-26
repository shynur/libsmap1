#pragma once

#include <string>
#include <vector>

namespace tools {
class FileReader {
 public:
  FileReader() = default;
  ~FileReader() = default;

  void LoadMap(const std::string& file_path, std::vector<unsigned char>& out_data);

 private:
  unsigned int GetHeaderBinarySize();
  unsigned int LoadHeaderBinary(unsigned char* buf);
  unsigned int LoadBodyBinary(std::vector<unsigned char>* buf, unsigned int file_body_binary_size);
  unsigned int LoadBinary(const std::string& file_path);
  unsigned int resolution_id_ = 0;
  int zone_id_ = 0;
  unsigned int m_ = 0;
  unsigned int n_ = 0;
  unsigned int file_body_binary_size_ = 0;

  // 数据
  std::vector<unsigned char> data_;
};

}  // namespace tools