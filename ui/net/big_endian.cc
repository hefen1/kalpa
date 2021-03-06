// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/net/big_endian.h"

#include "base/strings/string_piece.h"

namespace net {

BigEndianReader::BigEndianReader(const void* buf, size_t len)
    : ptr_(reinterpret_cast<const char*>(buf)), end_(ptr_ + len) {}

bool BigEndianReader::Skip(size_t len) {
  if (ptr_ + len > end_)
    return false;
  ptr_ += len;
  return true;
}

bool BigEndianReader::ReadBytes(void* out, size_t len) {
  if (ptr_ + len > end_)
    return false;
  memcpy(out, ptr_, len);
  ptr_ += len;
  return true;
}

bool BigEndianReader::ReadPiece(base::StringPiece* out, size_t len) {
  if (ptr_ + len > end_)
    return false;
  *out = base::StringPiece(ptr_, len);
  ptr_ += len;
  return true;
}

template<typename T>
bool BigEndianReader::Read(T* value) {
  if (ptr_ + sizeof(T) > end_)
    return false;
  ReadBigEndian<T>(ptr_, value);
  ptr_ += sizeof(T);
  return true;
}

bool BigEndianReader::ReadU8(uint8* value) {
  return Read(value);
}

bool BigEndianReader::ReadU16(uint16* value) {
  return Read(value);
}

bool BigEndianReader::ReadU32(uint32* value) {
  return Read(value);
}

BigEndianWriter::BigEndianWriter(void* buf, size_t len)
    : ptr_(reinterpret_cast<char*>(buf)), end_(ptr_ + len) {}

bool BigEndianWriter::Skip(size_t len) {
  if (ptr_ + len > end_)
    return false;
  ptr_ += len;
  return true;
}

bool BigEndianWriter::WriteBytes(const void* buf, size_t len) {
  if (ptr_ + len > end_)
    return false;
  memcpy(ptr_, buf, len);
  ptr_ += len;
  return true;
}

template<typename T>
bool BigEndianWriter::Write(T value) {
  if (ptr_ + sizeof(T) > end_)
    return false;
  WriteBigEndian<T>(ptr_, value);
  ptr_ += sizeof(T);
  return true;
}

bool BigEndianWriter::WriteU8(uint8 value) {
  return Write(value);
}

bool BigEndianWriter::WriteU16(uint16 value) {
  return Write(value);
}

bool BigEndianWriter::WriteU32(uint32 value) {
  return Write(value);
}

}  // namespace net

