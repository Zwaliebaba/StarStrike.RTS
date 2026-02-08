#include "pch.h"
#include "FileSys.h"

byte_buffer_t BinaryFile::ReadFile(const std::wstring_view _fileName)
{
  byte_buffer_t data;

	std::wstring fullName = GetHomeDirectory() + std::wstring(_fileName);
  ScopedHandle hFile(safe_handle(CreateFile2(fullName.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
  if (!hFile)
    return {};

  // Get the file size.
  FILE_STANDARD_INFO fileInfo;
  if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    return {};

  // File is too big for 32-bit allocation, so reject read.
  if (fileInfo.EndOfFile.HighPart > 0)
    return {};

  // Create enough space for the file data.
  data.resize(fileInfo.EndOfFile.LowPart);

  // Read the data in.
  DWORD bytesRead = 0;

  if (!::ReadFile(hFile.get(), data.data(), fileInfo.EndOfFile.LowPart, &bytesRead, nullptr))
    return {};

  if (bytesRead < fileInfo.EndOfFile.LowPart)
    return {};

  return data;
}

std::wstring TextFile::ReadFile(const std::wstring_view _fileName)
{
  std::wstring fullName = GetHomeDirectory() + std::wstring(_fileName);
  ScopedHandle hFile(safe_handle(CreateFile2(fullName.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
  if (!hFile)
    return {};

  // Get the file size.
  FILE_STANDARD_INFO fileInfo;
  if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    return {};

  // File is too big for 32-bit allocation, so reject read.
  if (fileInfo.EndOfFile.HighPart > 0)
    return {};

  DWORD fileSize = fileInfo.EndOfFile.LowPart;
  if (fileSize == 0)
    return {};

  // Read the raw bytes first.
  std::vector<uint8_t> rawData(fileSize);
  DWORD bytesRead = 0;

  if (!::ReadFile(hFile.get(), rawData.data(), fileSize, &bytesRead, nullptr))
    return {};

  if (bytesRead < fileSize)
    return {};

  // Detect encoding via BOM and convert to wide string.
  const uint8_t* dataPtr = rawData.data();
  size_t dataSize = rawData.size();

  // Check for UTF-16 LE BOM (FF FE)
  if (dataSize >= 2 && dataPtr[0] == 0xFF && dataPtr[1] == 0xFE)
  {
    dataPtr += 2;
    dataSize -= 2;
    size_t wcharCount = dataSize / sizeof(wchar_t);
    return std::wstring(reinterpret_cast<const wchar_t*>(dataPtr), wcharCount);
  }

  // Check for UTF-16 BE BOM (FE FF) - need to byte-swap
  if (dataSize >= 2 && dataPtr[0] == 0xFE && dataPtr[1] == 0xFF)
  {
    dataPtr += 2;
    dataSize -= 2;
    size_t wcharCount = dataSize / sizeof(wchar_t);
    std::wstring data(wcharCount, L'\0');
    for (size_t i = 0; i < wcharCount; ++i)
    {
      data[i] = static_cast<wchar_t>((dataPtr[i * 2] << 8) | dataPtr[i * 2 + 1]);
    }
    return data;
  }

  // Check for UTF-8 BOM (EF BB BF) - skip it
  if (dataSize >= 3 && dataPtr[0] == 0xEF && dataPtr[1] == 0xBB && dataPtr[2] == 0xBF)
  {
    dataPtr += 3;
    dataSize -= 3;
  }

  // Treat as UTF-8 (with or without BOM)
  if (dataSize == 0)
    return {};

  int wideSize = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(dataPtr), static_cast<int>(dataSize), nullptr, 0);
  if (wideSize == 0)
    return {};

  std::wstring data(wideSize, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(dataPtr), static_cast<int>(dataSize), data.data(), wideSize);

  return data;
}
