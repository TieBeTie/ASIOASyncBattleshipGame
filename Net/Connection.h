#pragma once

#include "Common.h"
#include "Message.h"
#include "TSDeque.h"

namespace net {
template <typename T>
class Connection : public std::enable_shared_from_this<Connection<T>> {
 public:
  // Соединение принадлежит либо серверу, либо сокету, их поведение немного
  // разное
  enum class owner { server, client };

 public:
  Connection(owner parent, asio::io_context& asioContext,
             asio::ip::tcp::socket socket, TSDeque<OwnedMessage<T>>& qIn)
      : context_(asioContext),
        socket_(std::move(socket)),
        messages_in(qIn),
        owner_type_(parent) {}

  virtual ~Connection() {}

  uint32_t GetID() const { return id; }

 public:
  void ConnectToClient(uint32_t uid = 0) {
    if (owner_type_ == owner::server) {
      if (socket_.is_open()) {
        id = uid;
        ReadHeader();
      }
    }
  }
  //  Выполняет ASIO context
  void ConnectToServer(const asio::ip::tcp::resolver::results_type& endpoints) {
    if (owner_type_ == owner::client) {
      // ASIO пытается подключиться к endpoints
      asio::async_connect(
          socket_, endpoints,
          [this](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
            if (!ec) {
              ReadHeader();
            }
          });
    }
  }
  //  Выполняет ASIO context
  void Disconnect() {
    if (IsConnected()) {
      // пытаемся закрыть сокет post создаёт функцию, а ASIO выполняет асинхронно
      asio::post(context_, [this]() { socket_.close(); });
    }
  }

  bool IsConnected() const { return socket_.is_open(); }

 public:
  //  Выполняет ASIO context, отправляет сообщения от клиента к серверу и наоборот
  void Send(const Message<T>& message) {
    asio::post(context_, [this, message]() {
      // Единственное сообщение отправиться без задержек,
      // а если поступит ещё, то те, кто пошли на отправку,
      // вызовут процесс записи следующего сообщения
      bool writing_message = !messages_out_.Empty();
      messages_out_.PushBack(message);
      if (!writing_message) {
        WriteHeader();
      }
    });
  }

 private:
  //  Выполняет ASIO context
  void WriteHeader() {
    // Если эта функция вызвана, мы знаем, что в очереди исходящих сообщений
    // должно быть хотя бы одно сообщение для отправки. Поэтому выделяем буфер
    // передачи для хранения сообщение, а ASIO выдаем команду отправить эти байты
    asio::async_write(
        socket_,
        asio::buffer(&messages_out_.Front().header, sizeof(MessageHeader<T>)),
        [this](std::error_code ec, std::size_t length) {
          if (!ec) {
            if (messages_out_.Front().body.size() > 0) {
              WriteBody();
            } else {
              messages_out_.PopFront();
              if (!messages_out_.Empty()) {
                WriteHeader();
              }
            }
          } else {
            std::cout << "[" << id << "] Write Header Fail.\n";
            socket_.close();
          }
        });
  }

  //  Выполняет ASIO context
  void WriteBody() {
    // Если вызывается эта функция, значит, только что был отправлен header,
    // и этот заголовок указывает на существование тела для этого сообщения.
    asio::async_write(socket_,
                      asio::buffer(messages_out_.Front().body.data(),
                                   messages_out_.Front().body.size()),
                      [this](std::error_code ec, std::size_t length) {
                        if (!ec) {
                          // Отправка прошла успешно, поэтому мы закончили
                          // работу с сообщением и удаляем его из очереди
                          messages_out_.PopFront();

                          // Если в очереди еще есть сообщения, то выдаём
                          // задание на отправку заголовка следующих сообщений.
                          if (!messages_out_.Empty()) {
                            WriteHeader();
                          }
                        } else {
                          std::cout << "[" << id << "] Write Body Fail.\n";
                          socket_.close();
                        }
                      });
  }

  //  Выполняет ASIO context
  void ReadHeader() {
    // Если эта функция будет вызвана, мы ожидаем, что ASIO будет ждать, пока он
    // не получит достаточно байт для формирования заголовка сообщения. Мы
    // знаем, что заголовки имеют фиксированный размер, поэтому выделяем буфер
    // передачи достаточно большой, чтобы хранить его. На самом деле, мы будем
    // конструировать сообщение во "временном" объекте сообщения, так как с ним
    // удобно работать.
    asio::async_read(
        socket_,
        asio::buffer(&temp_message_in_.header, sizeof(MessageHeader<T>)),
        [this](std::error_code ec, std::size_t length) {
          if (!ec) {
            // Полный заголовок сообщения прочитан, проверим, есть ли у этого 
            // сообщения тело
            if (temp_message_in_.header.size > 0) {
              temp_message_in_.body.resize(temp_message_in_.header.size);
              ReadBody();
            } else {
              AddToIncomingMessageQueue();
            }
          } else {
            std::cout << "[" << id << "] Read Header Fail.\n";
            socket_.close();
          }
        });
  }

  //  Выполняет ASIO context
  void ReadBody() {
    // Если эта функция вызвана, то заголовок уже прочитан, и этот заголовок
    // запрашивает, чтобы мы прочитали тело
    asio::async_read(socket_,
                     asio::buffer(temp_message_in_.body.data(),
                                  temp_message_in_.body.size()),
                     [this](std::error_code ec, std::size_t length) {
                       if (!ec) {
                         AddToIncomingMessageQueue();
                       } else {
                         std::cout << "[" << id << "] Read Body Fail.\n";
                         socket_.close();
                       }
                     });
  }

  // Последнее, что осталось, просто добавить сообщение в очередь
  void AddToIncomingMessageQueue() {
    if (owner_type_ == owner::server)
      messages_in.PushBack({this->shared_from_this(), temp_message_in_});
    else
      messages_in.PushBack({nullptr, temp_message_in_});

    // Теперь мы должны заправить ASIO CONTEXT для получения следующего
    // сообщения. Он будет просто сидеть и ждать, пока придут байты, и процесс
    // создания сообщения повторится
    ReadHeader();
  }

 protected:
  // Каждое соединение имеет уникальный сокет
  asio::ip::tcp::socket socket_;

  // Общий ASIO
  asio::io_context& context_;
  // В отправке
  TSDeque<Message<T>> messages_out_;

  // Приходящие сообщения
  TSDeque<OwnedMessage<T>>& messages_in;

  // Так как сообщения приходит асинхронно, будем хранить частично собранное,
  // пока оно не будет готово
  Message<T> temp_message_in_;

  owner owner_type_ = owner::server;

  uint32_t id = 0;
};
}