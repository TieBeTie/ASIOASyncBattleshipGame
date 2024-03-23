#pragma once
#include "Common.h"

namespace net {


template<typename T>
struct MessageHeader {
  T id{};
  uint32_t size = 0;
};


template<typename T>
struct Message {
  MessageHeader<T> header{};
  std::vector<uint8_t> body;

  size_t Size() const {
    return body.size();
  }

  friend std::ostream &operator<<(std::ostream &os, const Message<T> &message) {
    os << "ID:" << int(message.header.id) << " Size:" << message.header.size;
    return os;
  }

  template<typename DataType>
  friend Message<T> &operator<<(Message<T> &message, const DataType &data) {
    static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");
    size_t shift = message.body.size();
    message.body.resize(message.body.size() + sizeof(DataType));
    std::memcpy(message.body.data() + shift, &data, sizeof(DataType));
    message.header.size = message.Size();
    return message;
  }

  template<typename DataType>
  friend Message<T> &operator>>(Message<T> &message, DataType &data) {
    static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pulled from vector");

    size_t i = message.body.size() - sizeof(DataType);
    std::memcpy(&data, message.body.data() + i, sizeof(DataType));
    message.body.resize(i);
    message.header.size = message.Size();
    return message;
  }
};

template<typename T>
class Connection;

template<typename T>
struct OwnedMessage {
  std::shared_ptr<Connection<T>> remote = nullptr;
  Message<T> message;

  friend std::ostream &operator<<(std::ostream &os, const OwnedMessage<T> &message) {
    os << message.message;
    return os;
  }
};
}
