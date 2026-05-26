#include "file_writer.h"
#include <iostream>
#include <cstring>

namespace tools {

void FileWriter::SaveMap(const std::string& file_path, const std::string& suffix,
                         const std::vector<unsigned char>& data, unsigned int resolution_id, const int zone_id,
                         const unsigned int m, const unsigned int n) {
  map_data_ = data;
  resolution_id_ = resolution_id;
  m_ = m;
  n_ = n;
  zone_id_ = zone_id;
  const std::string new_file_path_ = file_path + MakeNodeFileName("." + suffix);
  CreateFinalBinary(new_file_path_);
}

unsigned int FileWriter::GetHeaderBinarySize() {
  return static_cast<int>(sizeof(unsigned int)      // index_.resolution_id_
                          + sizeof(int)             // index_.zone_id_
                          + sizeof(unsigned int)    // index_.m_
                          + sizeof(unsigned int)    // index_.n_
                          + sizeof(unsigned int));  // the body size in file.
}

unsigned int FileWriter::CreateBodyBinary(std::vector<unsigned char>* buf) {
  unsigned int body_size = GetBodyBinarySize();
  buf->resize(body_size);
  CreateBinary(&((*buf)[0]), body_size);
  file_body_binary_size_ = static_cast<unsigned int>(buf->size());
  return static_cast<unsigned int>(buf->size());
}
unsigned int FileWriter::CreateHeaderBinary(unsigned char* buf, unsigned int buf_size) const {
  unsigned int target_size = GetHeaderBinarySize();
  if (buf_size >= target_size) {
    auto* p = reinterpret_cast<unsigned int*>(buf);
    *p = resolution_id_;
    std::cout << "resolution_id = " << resolution_id_ << std::endl;
    ++p;
    int* pp = reinterpret_cast<int*>(p);
    *pp = zone_id_;
    std::cout << "zone_id = " << zone_id_ << std::endl;
    ++pp;
    p = reinterpret_cast<unsigned int*>(pp);
    *p = m_;
    std::cout << "m = " << m_ << std::endl;
    ++p;
    *p = n_;
    std::cout << "n = " << n_ << std::endl;
    ++p;
    *p = file_body_binary_size_;  // Set it before call this function!
    std::cout << "file_body_binary_size_ = " << file_body_binary_size_ << std::endl;
  }
  return target_size;
}

unsigned int FileWriter::CreateFinalBinary(const std::string& file_path) {
  FILE* file = fopen(file_path.c_str(), "wb");
  unsigned int buf_size = GetBinarySize();
  std::vector<unsigned char> buffer;
  buffer.resize(buf_size);

  unsigned int binary_size = 0;
  std::vector<unsigned char> body_buffer;
  CreateBodyBinary(&body_buffer);

  unsigned int header_size = GetHeaderBinarySize();
  unsigned int processed_size = CreateHeaderBinary(&buffer[0], buf_size);
  if (processed_size != header_size) {
    std::cout << "Error: buffer size is not enough to store header binary." << std::endl;
  }
  const unsigned int buffer_bias = processed_size;
  buf_size -= processed_size;
  binary_size += processed_size;
  if (buf_size != body_buffer.size()) {
    std::cout << "Error: buffer size is not enough to store body binary." << std::endl;
    return 0;
  }
  memcpy(&buffer[buffer_bias], &body_buffer[0], body_buffer.size());
  binary_size += static_cast<unsigned int>(body_buffer.size());
  fwrite(&buffer[0], 1, binary_size, file);
  if (file) {
    fclose(file);
  } else {
    std::cout << "Error: could not write binary file." << std::endl;
  }
  return binary_size;
}

unsigned int FileWriter::CreateBinary(unsigned char* buf, unsigned int buf_size) {
  unsigned int required_size = GetBodyBinarySize();
  if (buf_size < required_size) {
    std::cerr << "Error: Buffer size too small. Required: " << required_size << ", Provided: " << buf_size << std::endl;
    return 0;
  }
  if (!map_data_.empty()) {
    std::cerr << "Error: Buffer size too small. Required: " << required_size << ", Provided: " << buf_size << std::endl;
    memcpy(buf, map_data_.data(), map_data_.size());
  }
  return required_size;
}

std::string FileWriter::MakeNodeFileName(const std::string& ext) const {
  char buf[1024];
  // 不在这里写后缀
  snprintf(buf, sizeof(buf), "%03u_%d_%08u_%08u", resolution_id_, zone_id_, m_, n_);
  // 拼接后缀（确保包含点号）
  return std::string(buf) + ext;
}

}  // namespace tools
