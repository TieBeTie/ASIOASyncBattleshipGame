#include <net.h>

#include <iostream>

#include "Battleship.h"

using std::string;

enum class MessageTypes : uint32_t {
  ServerAccept,
  Battleship,
  Win,
};

class BattleshipServer : public net::IServer<MessageTypes> {
 public:
  BattleshipServer(uint16_t nPort)
      : net::IServer<MessageTypes>(nPort) {}

 protected:
  // Переопределяем методы так, как нужно для работы Морского Боя
  virtual bool OnClientConnect(
      std::shared_ptr<net::Connection<MessageTypes>> client) {

    // Создаём игру для нового клиента
    auto game = new Battleship();
    games_.push_back(game);
    std::cout << game->GetBoard(false);

    // Создаём и отправляем начальные позиции новому игроку
    net::Message<MessageTypes> message;
    message.header.id = MessageTypes::ServerAccept;
    char buffer[1024];
    strcpy_s(buffer, game->GetBoard(true).c_str());
    message << buffer;
    client->Send(message);
    return true;
  }

  virtual void OnClientDisconnect(
      std::shared_ptr<net::Connection<MessageTypes>> client) {
    std::cout << "Removing client [" << client->GetID() << "]\n";
  }

  // Обработчик сообщений от клиентов
  virtual void OnMessage(
      std::shared_ptr<net::Connection<MessageTypes>> client,
      net::Message<MessageTypes>& user_msg) {
    switch (user_msg.header.id) {

      case MessageTypes::Battleship: {
        char attack_pos[3];
        user_msg >> attack_pos;
        games_[client->GetID()]->Attack(attack_pos);

        net::Message<MessageTypes> message;
        char buffer[1024];

        auto game = games_[client->GetID()];

        if (games_[client->GetID()]->CheckWin()) {
          message.header.id = MessageTypes::Win;
          std::string win_message(game->GetBoard(false).c_str());
          win_message += "You win!\n";
          strcpy_s(buffer, win_message.c_str());   
        } else {
          message.header.id = MessageTypes::Battleship;
          strcpy_s(buffer, game->GetBoard(true).c_str());    
        }
        message << buffer;
        client->Send(message);
      } break;
    }
  }
  vector<Battleship*> games_;
};

int main() {
  BattleshipServer server(60000);
  server.Start();

  while (1) {
    server.Update(-1, true);
  }

  return 0;
}