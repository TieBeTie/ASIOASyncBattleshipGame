#pragma once
#include "Common.h"

namespace net {
template <typename T>
class IClient {
 public:
  IClient() {}

  virtual ~IClient() {
    // Если клиент уничтожается, отключаемся от сервера
    Disconnect();
  }

 public:
  bool Connect(const std::string& host, const uint16_t port) {
    try {
      asio::ip::tcp::resolver resolver(context_);
      asio::ip::tcp::resolver::results_type endpoints =
          resolver.resolve(host, std::to_string(port));

      connection_ = std::make_unique<Connection<T>>(
          Connection<T>::owner::client, context_,
          asio::ip::tcp::socket(context_), messages_in_);

      connection_->ConnectToServer(endpoints);

      // Начать выполнение потока с ASIO context
      context_thread_ = std::thread([this]() { context_.run(); });
    } catch (std::exception& e) {
      std::cerr << "Client Exception: " << e.what() << "\n";
      return false;
    }
    return true;
  }

  void Disconnect() {
    if (IsConnected()) {
      connection_->Disconnect();
    }

    // Остановка ASIO context
    context_.stop();
    // и потока
    if (context_thread_.joinable()) context_thread_.join();

    connection_.release();
  }

  bool IsConnected() {
    return connection_ ? connection_->IsConnected() : false;
  }

 public:
  void Send(const Message<T>& message) {
    if (IsConnected()) connection_->Send(message);
  }

  // Получение очереди сообщений с сервера
  TSDeque<OwnedMessage<T>>& Incoming() { return messages_in_; }

 protected:
  // ASIO context обрабатывает передачу данных
  asio::io_context context_;
  // и нуждается в собственном потоке для выполнения своих рабочих команд
  std::thread context_thread_;
  // У клиента есть единственный экземпляр Connection<T>, который
  // обрабатывает передачу данных
  std::unique_ptr<Connection<T>> connection_;

 private:
  // Это потокобезопасный дек входящих сообщений от сервера
  TSDeque<OwnedMessage<T>> messages_in_;
};
}