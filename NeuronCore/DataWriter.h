#pragma once

#define MAX_PACKET_SIZE 1500
#define DATALOAD_SIZE (MAX_PACKET_SIZE - 32) // Reserve some bytes for headers

namespace Neuron
{
  /**
   * @brief Writes binary data to a fixed-size buffer with bounds and error checking.
   */
  class DataWriter
  {
  public:
    /**
     * @brief Default constructor. Creates an empty writer.
     */
    DataWriter() : m_size(0), m_error(false) {}

    /**
     * @brief Write a single char to the buffer.
     */
    void WriteChar(const char _value)
    {
      if (m_size + sizeof(_value) > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "WriteChar out of bounds");
        return;
      }
      m_data[m_size++] = static_cast<std::byte>(_value);
    }

    /**
     * @brief Write a single byte to the buffer.
     */
    void WriteByte(const uint8_t _value)
    {
      if (m_size + sizeof(_value) > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "WriteByte out of bounds");
        return;
      }
      m_data[m_size++] = static_cast<std::byte>(_value);
    }

    /**
     * @brief Write an int16 to the buffer.
     */
    void WriteInt16(const int16_t _value)
    {
      if (m_size + sizeof(_value) > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "WriteInt16 out of bounds");
        return;
      }
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    /**
     * @brief Write a uint16 to the buffer.
     */
    void WriteUInt16(const uint16_t _value)
    {
      if (m_size + sizeof(_value) > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "WriteUInt16 out of bounds");
        return;
      }
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    /**
     * @brief Write an int32 to the buffer.
     */
    void WriteInt32(const int32_t _value)
    {
      if (m_size + sizeof(_value) > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "WriteInt32 out of bounds");
        return;
      }
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    /**
     * @brief Write a uint32 to the buffer.
     */
    void WriteUInt32(const uint32_t _value)
    {
      if (m_size + sizeof(_value) > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "WriteUInt32 out of bounds");
        return;
      }
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    /**
     * @brief Write an int64 to the buffer.
     */
    void WriteInt64(const int64_t _value)
    {
      if (m_size + sizeof(_value) > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "WriteInt64 out of bounds");
        return;
      }
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    /**
     * @brief Write a uint64 to the buffer.
     */
    void WriteUInt64(const uint64_t _value)
    {
      if (m_size + sizeof(_value) > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "WriteUInt64 out of bounds");
        return;
      }
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    /**
     * @brief Write a float to the buffer.
     */
    void WriteFloat(const float _value)
    {
      if (m_size + sizeof(_value) > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "WriteFloat out of bounds");
        return;
      }
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    /**
     * @brief Write a string (length-prefixed) to the buffer. Maximum length is 1024 bytes.
     */
    void WriteString(const std::string& _value)
    {
      constexpr uint32_t MAX_STRING_LEN = 1024;
      uint32_t len = static_cast<uint32_t>(_value.size());
      if (len > MAX_STRING_LEN || m_size + sizeof(uint32_t) + len > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "WriteString out of bounds or too long");
        return;
      }
      WriteUInt32(len);
      std::memcpy(m_data.data() + m_size, _value.data(), len);
      m_size += len;
    }

    /**
     * @brief Write raw bytes to the buffer.
     * @param _data Pointer to data.
     * @param _size Number of bytes to write.
     */
    void WriteRaw(const uint8_t* _data, size_t _size)
    {
      if (m_size + _size > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "WriteRaw out of bounds");
        return;
      }
      std::memcpy(m_data.data() + m_size, _data, _size);
      m_size += _size;
    }

    /**
     * @brief Clear the buffer and reset error state.
     */
    void Clear()
    {
      m_size = 0;
      m_error = false;
    }

    /**
     * @brief Get the buffer data pointer.
     */
    [[nodiscard]] const char* Data() const { return reinterpret_cast<const char*>(m_data.data()); }

    /**
     * @brief Get the size of the buffer.
     */
    [[nodiscard]] int Size() const { return static_cast<int>(m_size); }

    /**
     * @brief Returns true if an error occurred during writing.
     */
    bool HasError() const { return m_error; }

  protected:
    std::array<std::byte, DATALOAD_SIZE> m_data;
    size_t m_size = 0;
    bool m_error = false;
  };
}
