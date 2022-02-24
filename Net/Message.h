#pragma once
#include "Common.h"

namespace net {

// Message Header отправляется в начале всех сообщений. Шаблон позволяет
// нам использовать "enum class", чтобы убедиться, что сообщения действительны
// во время компиляции 
template<typename T>
struct MessageHeader {
  T id{};
  uint32_t size = 0;
};

// Message Body содержит header и std::vector, содержащий
// необработанные байты информации. Таким образом, сообщение может быть
// переменной длины, но размер в заголовке должен быть обновлен.
template<typename T>
struct Message {
  MessageHeader<T> header{};
  std::vector<uint8_t> body;

  // Размер сообщения в байтах
  size_t Size() const {
    return body.size();
  }

  // friend - для доступа к переменным внутри
  friend std::ostream &operator<<(std::ostream &os, const Message<T> &message) {
    os << "ID:" << int(message.header.id) << " Size:" << message.header.size;
    return os;
  }

  // Отправка POD типов сообщений
  template<typename DataType>
  friend Message<T> &operator<<(Message<T> &message, const DataType &data) {
    // https://en.cppreference.com/w/cpp/named_req/PODType
    static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");

    // Запомним то место, туда, где мы будем вставлять оставшиеся данные 
    size_t shift = message.body.size();
    message.body.resize(message.body.size() + sizeof(DataType));
    std::memcpy(message.body.data() + shift, &data, sizeof(DataType));
    message.header.size = message.Size();
    return message;
  }

  // Чтение POD типов сообщений
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


// owned_message идентично обычному сообщению, но оно связано с
// соединением. На сервере владельцем будет клиент, отправивший сообщение, на
// клиенте владельцем будет сервер.
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
