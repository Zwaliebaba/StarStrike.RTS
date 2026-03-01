#pragma once

#define MAX_PACKET_SIZE 1500
#define DATALOAD_SIZE (MAX_PACKET_SIZE - 32) // Reserve some bytes for headers

namespace Neuron
{
  template <class T>
  static constexpr bool IsReadableType = std::is_trivially_copyable_v<T>;

  /**
   * @brief Reads binary data from a fixed-size buffer with bounds and error checking.
   */
  class DataReader
  {
  public:
    /**
     * @brief Default constructor. Creates an empty reader.
     */
    DataReader() = default;

    /**
     * @brief Initializes the reader with a data buffer.
     * @param _data Pointer to data.
     * @param _size Size of data.
     */
    DataReader(const uint8_t *_data, size_t _size)
      : m_size(0), m_error(false)
    {
      if (_size > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "DataReader initialized with too large data size");
        return;
      }
      std::copy_n(_data, _size, reinterpret_cast<uint8_t *>(m_data.data()));
    }

    /**
     * @brief Reads a trivially copyable type from the buffer.
     * @tparam T Type to read.
     * @return Value of type T.
     */
    template <typename T>
    [[nodiscard]] T Read()
    {
      static_assert(IsReadableType<T>, "Type is not readable");

      T value{};

      if (m_size + sizeof(T) > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "Read out of bounds");
        return value;
      }

      std::copy_n(reinterpret_cast<const uint8_t *>(m_data.data() + m_size), sizeof(T), reinterpret_cast<uint8_t *>(&value));
      m_size += sizeof(T);
      return value;
    }

    /**
     * @brief Reads an array of trivially copyable types from the buffer.
     * @tparam T Type to read.
     * @param _array Pointer to array.
     * @param _count Number of elements.
     */
    template <typename T>
    void ReadArray(T *_array, size_t _count)
    {
      if constexpr (std::is_trivially_copyable_v<T>)
      {
        const size_t bytes = sizeof(T) * _count;
        if (m_size + bytes > DATALOAD_SIZE)
        {
          m_error = true;
          ASSERT_TEXT(false, "ReadArray out of bounds");
          return;
        }
        std::memcpy(_array, m_data.data() + m_size, bytes);
        m_size += bytes;
      }
      else
      {
        for (size_t i = 0; i < _count; ++i)
        {
          _array[i] = Read<T>();
        }
      }
    }

    /**
     * @brief Reads a string from the buffer. Maximum length is 1024 bytes.
     * @return String value.
     */
    std::string ReadString()
    {
      constexpr uint32_t MAX_STRING_LEN = 1024;
      uint32_t len = Read<uint32_t>();

      if (len > MAX_STRING_LEN || m_size + len > DATALOAD_SIZE)
      {
        m_error = true;
        ASSERT_TEXT(false, "ReadString out of bounds or too long");
        return std::string();
      }
      std::string value(reinterpret_cast<const char *>(m_data.data() + m_size), len);
      m_size += len;
      return value;
    }

    /**
     * @brief Returns true if an error occurred during reading.
     */
    bool HasError() const { return m_error; }

  protected:
    std::array<std::byte, DATALOAD_SIZE> m_data;
    size_t m_size = 0;
    bool m_error = false;
  };
}
