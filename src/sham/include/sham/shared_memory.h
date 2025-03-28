/*
MIT License - Copyright (c) 2023 Pierric Gimmig

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#pragma once

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// Cross-platform interface for creating and accessing shared memory.
namespace sham {

#ifdef _WIN32
#include <windows.h>
using FileHandle = HANDLE;
constexpr FileHandle kInvalidFileHandle = nullptr;
#else
using FileHandle = int;
constexpr FileHandle kInvalidFileHandle = -1;
#endif

// Create a new file mapping.
inline FileHandle CreateFileMapping(std::string_view name, size_t size);
// Open a view on an existing file mapping.
inline FileHandle OpenFileMapping(std::string_view name);
// Destroy a file mapping. Must be called by same process that called CreateFileMapping().
inline void DestroyFileMapping(FileHandle file_handle, std::string_view name);
// Map file into memory.
inline uint8_t* MapViewOfFile(FileHandle file_handle, size_t size);
// Unmap file from memory.
inline void UnMapViewOfFile(uint8_t* address, size_t size);

}  // namespace sham

#ifdef _WIN32
sham::FileHandle sham::CreateFileMapping(std::string_view name, size_t capacity) {
  std::string map_name(name);
  sham::FileHandle handle = ::CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                                 static_cast<DWORD>(capacity), map_name.c_str());

  if (handle == nullptr) {
    std::cout << "Could not create file mapping for " << name << ":" << GetLastError() << std::endl;
  }
  return handle;
}

sham::FileHandle sham::OpenFileMapping(std::string_view name) {
  std::string map_name(name);
  FileHandle handle = ::OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, map_name.c_str());

  if (handle == nullptr) {
    std::cout << "Could not open file mapping for " << name << ":" << GetLastError() << std::endl;
  }
  return handle;
}

void sham::DestroyFileMapping(FileHandle handle, std::string_view name) {
  if (handle) CloseHandle(handle);
}

uint8_t* sham::MapViewOfFile(FileHandle file_handle, size_t size) {
  LPCTSTR ptr = (LPTSTR)::MapViewOfFile(file_handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
  return (uint8_t*)(ptr);
}

void sham::UnMapViewOfFile(uint8_t* address, size_t /*size*/) { UnmapViewOfFile(address); }
#else
sham::FileHandle sham::CreateFileMapping(std::string_view name, size_t size) {
  std::string map_name(name);
  sham::FileHandle handle = shm_open(map_name.c_str(), O_RDWR | O_CREAT,
                                     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (handle == -1) {
    perror("Can't open memory fd");
  }

  // Change permission of the shared memory to make sure that non-root processes can access it.
  if (fchmod(handle, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) == -1) {
    perror("Can't change permission on fd");
  }

  if ((ftruncate(handle, size)) == -1) {
    perror("Can't truncate memory");
  }
  return handle;
}

sham::FileHandle sham::OpenFileMapping(std::string_view name) {
  std::string map_name(name);
  FileHandle handle = shm_open(map_name.c_str(), O_RDWR, 0600);
  if (handle == -1) {
    perror("Can't open file descriptor");
  }
  return handle;
}

void sham::DestroyFileMapping(FileHandle handle, std::string_view name) {
  std::string map_name(name);
  if (handle != kInvalidFileHandle) shm_unlink(map_name.c_str());
}

uint8_t* sham::MapViewOfFile(FileHandle file_handle, size_t size) {
  void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, file_handle, 0);
  if (ptr != MAP_FAILED) return static_cast<uint8_t*>(ptr);
  perror("Memory mapping failed");
  return static_cast<uint8_t*>(ptr);
}

void sham::UnMapViewOfFile(uint8_t* address, size_t size) {
  if (address == nullptr) return;
  munmap(address, size);
}

#endif
