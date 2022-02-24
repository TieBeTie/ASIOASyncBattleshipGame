#include <net.h>

#include <iostream>

enum class MessageTypes : uint32_t {
  ServerAccept,
  Attack,
  Win,
};

class BattleshipClient : public net::IClient<MessageTypes> {
 public:
  void Attack() {
    std::string attack_pos;
    bool incorrect_input = true;
    while (incorrect_input) {
      std::cin >> attack_pos;
      if (attack_pos.size() == 2 && 'a' <= attack_pos[0] && attack_pos[0] <= 'j' && '0' <= attack_pos[1] &&
          attack_pos[1] <= '9') {
        incorrect_input = false;
      } else {
        std::cout << "Incorrect input, format is 'XY', where X = a-j, Y = 0-9, "
                     "for example 'a3'"
                  << '\n';
      }
    }
    char buffer[3];
    strcpy_s(buffer, attack_pos.c_str());
    net::Message<MessageTypes> message;
    message.header.id = MessageTypes::Attack;
    message << buffer;
    Send(message);
  }
};

int main() {
  BattleshipClient client;
  client.Connect("tiebetie.servegame.com", 60000);

  bool quit = false;
  while (!quit) {
    if (client.IsConnected()) {
      if (!client.Incoming().Empty()) {
        auto message = client.Incoming().PopFront().message;
        switch (message.header.id) {

          case MessageTypes::ServerAccept: {
            // Server has responded to a ping request
            std::cout << "Server accept connection!\n";
            char buffer[1024];
            message >> buffer;
            std::cout << buffer << '\n';
            std::cout
                << "Answer format is 'XY', where X = a - j, Y = 0 - 9 for "
                   "example 'a3'\n";
            client.Attack();
          } break;

          case MessageTypes::Attack: {
            char buffer[1024];
            message >> buffer;
            std::cout << buffer << "##############################\n";

            client.Attack();
          } break;

          case MessageTypes::Win: {
            char buffer[1024];
            message >> buffer;
            std::cout << buffer << "##############################\n";
            quit = true;
          } break;
        }
      }
    } else {
      quit = true;
    }
  }
  std::cout << "For exit the game enter anything and press 'ENTER'\n";
  std::string tmp;
  std::cin >> tmp;
  return 0;
}