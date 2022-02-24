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
      // Перед запуском контекста, выдадим ему задание, чтобы он не завершился
      WaitForClientConnection();

      // Запуск контекста отдельным потоком
      context_thread_ = std::thread([this]() { context_.run(); });
    } catch (std::exception& e) {
      std::cerr << "[Server] Exception: " << e.what() << "\n";
      return false;
    }

    std::cout << "[Server] Started!\n";
    return true;
  }

  void Stop() {
    // Выключим контекст
    context_.stop();

    // Выполняющий поток, ждёт пока контекст закончит свою работу
    if (context_thread_.joinable()) context_thread_.join();

    std::cout << "[Server] Stopped!\n";
  }

  // Выполняет ASIO context, чтобы не завершится, выполняет всегда
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

  // Сервер отвечает на входящие сообщения
  void Update(size_t nMaxMessages = -1, bool bWait = false) {
    if (bWait) {
      // Ожидаем входящие сообщения
      messages_in_.Wait();
    }

    size_t nMessageCount = 0;
    while (nMessageCount < nMaxMessages && !messages_in_.Empty()) {
      auto message = messages_in_.PopFront();

      // Передаём обработчику сообщений первое письмо
      OnMessage(message.remote, message.message);

      nMessageCount++;
    }
  }

 protected:
  // Эти методы класс сервера должен переопределить

  // Вызываеся при подключении клиента
  virtual bool OnClientConnect(std::shared_ptr<Connection<T>> client) {
    return false;
  }

  // При отключении
  virtual void OnClientDisconnect(std::shared_ptr<Connection<T>> client) {}

  // При поступлении сообщения от клиента
  virtual void OnMessage(std::shared_ptr<Connection<T>> client,
                         Message<T>& message) {}

 protected:
  // Потокобезопасный дек для входящих сообщений
  TSDeque<OwnedMessage<T>> messages_in_;

  // Дек для подтверждённых соединений
  std::deque<std::shared_ptr<Connection<T>>> connections_;

  // Порядок объявления и инициализации важен!
  asio::io_context context_;
  std::thread context_thread_;

  // Будет выполнять ASIO context
  asio::ip::tcp::acceptor acceptor_;

  // Идендификаторы клиентов для удобного обращения
  uint32_t id_counter_ = 0;
};
}