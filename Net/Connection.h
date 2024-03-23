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
  void Send(const Message<T>& message) {
    asio::post(context_, [this, message]() {
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

    asio::async_write(socket_,
                      asio::buffer(messages_out_.Front().body.data(),
                                   messages_out_.Front().body.size()),
                      [this](std::error_code ec, std::size_t length) {
                        if (!ec) {
                          messages_out_.PopFront();

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

  void AddToIncomingMessageQueue() {
    if (owner_type_ == owner::server)
      messages_in.PushBack({this->shared_from_this(), temp_message_in_});
    else
      messages_in.PushBack({nullptr, temp_message_in_});
    ReadHeader();
  }

 protected:
  // Каждое соединение имеет уникальный сокет
  asio::ip::tcp::socket socket_;

  // Общий ASIO
  asio::io_context& context_;
  TSDeque<Message<T>> messages_out_;

  TSDeque<OwnedMessage<T>>& messages_in;

  Message<T> temp_message_in_;

  owner owner_type_ = owner::server;

  uint32_t id = 0;
};
}
