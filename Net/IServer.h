#pragma once

#include "Common.h"
#include "Connection.h"
#include "Message.h"
#include "TSDeque.h"

namespace net {
template <typename T>
class IServer {
 public:
  // Создаёт сервер
  IServer(uint16_t port)
      : acceptor_(context_,
                       asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {}

  virtual ~IServer() { Stop(); }

  bool Start() {
    try {
      WaitForClientConnection();

      context_thread_ = std::thread([this]() { context_.run(); });
    } catch (std::exception& e) {
      std::cerr << "[Server] Exception: " << e.what() << "\n";
      return false;
    }

    std::cout << "[Server] Started!\n";
    return true;
  }

  void Stop() {
    context_.stop();
    if (context_thread_.joinable()) context_thread_.join();
    std::cout << "[Server] Stopped!\n";
  }

  void WaitForClientConnection() {
    // Ожидая, принимаем входящее соедение
    acceptor_.async_accept([this](std::error_code ec,
                                       asio::ip::tcp::socket socket) {
      // Просыпаемся, когда оно пришло и обрабатываем...
      if (!ec) {
        std::cout << "[Server] New Connection: " << socket.remote_endpoint()
                  << "\n";

        // Создаём соединение (сокет) для общения с клиентом,
        // умный указатель, необходим для того, чтобы если соединение
        // будет без ожидающих задач, то он удалит объект Connection
        std::shared_ptr<Connection<T>> newconn =
            std::make_shared<Connection<T>>(Connection<T>::owner::server,
                                            context_, std::move(socket),
                                            messages_in_);

        // Можем отменить соедение, по умолчанию нет
        if (OnClientConnect(newconn)) {
          connections_.push_back(std::move(newconn));

          // У соединения выдаём задание по чтению байтов его ASIO context
          connections_.back()->ConnectToClient(id_counter_++);

          std::cout << "[" << connections_.back()->GetID()
                    << "] Connection Approved\n";
        } else {
          std::cout << "[-----] Connection Denied\n";

          // А соединение без области видимости и ожидающих задач будет
          // уничтожено
        }
      } else {
        std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
      }

      // Обрабатываем следующее соединение
      WaitForClientConnection();
    });
  }

  void Update(size_t nMaxMessages = -1, bool bWait = false) {
    if (bWait) {
      messages_in_.Wait();
    }

    size_t nMessageCount = 0;
    while (nMessageCount < nMaxMessages && !messages_in_.Empty()) {
      auto message = messages_in_.PopFront();
      OnMessage(message.remote, message.message);

      nMessageCount++;
    }
  }

 protected:

  virtual bool OnClientConnect(std::shared_ptr<Connection<T>> client) {
    return false;
  }

  virtual void OnClientDisconnect(std::shared_ptr<Connection<T>> client) {}

  virtual void OnMessage(std::shared_ptr<Connection<T>> client,
                         Message<T>& message) {}

 protected:
  TSDeque<OwnedMessage<T>> messages_in_;

  std::deque<std::shared_ptr<Connection<T>>> connections_;

  // Порядок объявления и инициализации важен!
  asio::io_context context_;
  std::thread context_thread_;

  // Будет выполнять ASIO context
  asio::ip::tcp::acceptor acceptor_;

  uint32_t id_counter_ = 0;
};
}
