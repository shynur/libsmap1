#pragma once

#include <string>
#include <vector>

namespace tools {

class FileWriter {
 public:
  FileWriter() = default;
  ~FileWriter() = default;

  void SaveMap(const std::string& file_path, const std::string& suffix, const std::vector<unsigned char>& data,
               unsigned int resolution_id = 0, int zone_id = 0, unsigned int m = 0, unsigned int n = 0);

 private:
  static unsigned int GetHeaderBinarySize();
  unsigned int CreateFinalBinary(const std::string& file_path);
  unsigned int CreateHeaderBinary(unsigned char* buf, unsigned int buf_size) const;
  unsigned int CreateBodyBinary(std::vector<unsigned char>* buf);
  unsigned int CreateBinary(unsigned char* buf, unsigned int buf_size);
  unsigned int GetBodyBinarySize() const { return static_cast<unsigned int>(map_data_.size()); }
  unsigned int GetBinarySize() const { return GetBodyBinarySize() + GetHeaderBinarySize(); }

  // 生成保存文件的后缀

  std::string MakeNodeFileName(const std::string& ext) const;
  // 要保存的地图数据
  std::vector<unsigned char> map_data_;

  unsigned int file_body_binary_size_ = 0;
  unsigned int resolution_id_ = 0;
  int zone_id_ = 0;
  unsigned int m_ = 0;
  unsigned int n_ = 0;
};

}  // namespace tools
