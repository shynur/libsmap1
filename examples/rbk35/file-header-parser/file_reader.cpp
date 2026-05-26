#include "file_reader.h"
#include <iostream>
#include <cstring>

namespace tools {

unsigned int FileReader::GetHeaderBinarySize() {
  return static_cast<unsigned int>(sizeof(unsigned int)      // resolution_id_
                                   + sizeof(int)             // zone_id_
                                   + sizeof(unsigned int)    // m_
                                   + sizeof(unsigned int)    // n_
                                   + sizeof(unsigned int));  // file_body_binary_size_
}

unsigned int FileReader::LoadHeaderBinary(unsigned char* buf) {
  unsigned int target_size = GetHeaderBinarySize();
  auto* p = reinterpret_cast<unsigned int*>(buf);
  resolution_id_ = *p;
  ++p;
  int* pp = reinterpret_cast<int*>(p);
  zone_id_ = *pp;
  ++pp;
  p = reinterpret_cast<unsigned int*>(pp);
  m_ = *p;
  ++p;
  n_ = *p;
  ++p;
  file_body_binary_size_ = *p;
  return target_size;
}

unsigned int FileReader::LoadBodyBinary(std::vector<unsigned char>* buf, unsigned int file_body_binary_size) {
  if (buf == nullptr) {
    std::cerr << "Error: Invalid buffer" << std::endl;
    return 0;
  }
  data_.resize(file_body_binary_size);
  memcpy(data_.data(), buf->data(), file_body_binary_size);
  return file_body_binary_size;
}

unsigned int FileReader::LoadBinary(const std::string& file_path) {
  FILE* file = fopen(file_path.c_str(), "rb");
  if (file == nullptr) {
    std::cerr << "Error: Cannot open file " << file_path << std::endl;
    return 0;
  }

  unsigned int header_size = GetHeaderBinarySize();
  std::vector<unsigned char> buf(header_size);
  size_t read_size = fread(&buf[0], 1, header_size, file);
  if (read_size != header_size) {
    std::cerr << "Error: Reading binary file header failed." << std::endl;
    fclose(file);
    return 0;
  }

  unsigned int processed_size = LoadHeaderBinary(&buf[0]);
  if (processed_size != header_size) {
    std::cerr << "Error: Processing binary file header failed." << std::endl;
    fclose(file);
    return 0;
  }

  buf.resize(file_body_binary_size_);
  read_size = fread(&buf[0], 1, file_body_binary_size_, file);
  if (read_size != file_body_binary_size_) {
    std::cerr << "Error: Reading binary file body failed." << std::endl;
    fclose(file);
    return 0;
  }

  processed_size += LoadBodyBinary(&buf, file_body_binary_size_);
  fclose(file);
  return processed_size;
}

void FileReader::LoadMap(const std::string& file_path, std::vector<unsigned char>& out_data) {
  unsigned int processed_size = LoadBinary(file_path);
  if (processed_size > 0) {
    out_data = data_;
  } else {
    out_data.clear();
  }
}

}  // namespace tools
