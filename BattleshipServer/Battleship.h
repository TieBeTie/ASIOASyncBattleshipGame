#pragma once
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using std::vector;

const uint8_t BOARD_SIZE = 10;
const uint8_t THE_BIGGEST_SHIP = 4;

class Battleship {
 public:
  Battleship() : board_(BOARD_SIZE), hiden_board_(BOARD_SIZE) {
    board_.assign(BOARD_SIZE, vector<Cell>(BOARD_SIZE));
    RandomArrangement();
    hiden_board_.assign(BOARD_SIZE, vector<Cell>(BOARD_SIZE));
  }
  Battleship(const Battleship&) = delete;
  void Print(bool hiden) {
    vector<vector<Cell>>* board = &board_;
    if (hiden) {
      board = &hiden_board_;
    }

    std::cout << "  ";
    for (char i = 0; i < BOARD_SIZE; ++i) {
      std::cout << (char)('a' + i) << ' ';
    }
    std::cout << '\n';
    for (int i = 0; i < BOARD_SIZE; ++i) {
      std::cout << i;
      for (int j = 0; j < BOARD_SIZE; ++j) {
        std::cout << ' ' << (*board)[i][j].mark;
      }
      std::cout << '\n';
    }
  }
  std::string GetBoard(bool hiden) {
    vector<vector<Cell>>* board = &board_;
    if (hiden) {
      board = &hiden_board_;
    }
    std::stringstream buffer;
    buffer << "  ";
    for (char i = 0; i < BOARD_SIZE; ++i) {
      buffer << (char)('a' + i) << ' ';
    }
    buffer << '\n';
    for (int i = 0; i < BOARD_SIZE; ++i) {
      buffer << i;
      for (int j = 0; j < BOARD_SIZE; ++j) {
        buffer << ' ' << (*board)[i][j].mark;
      }
      buffer << '\n';
    }
    return buffer.str();
  }
  void RandomArrangement() {
    int iteration = 0;
    for (int ship_type = 0; ship_type < THE_BIGGEST_SHIP; ++ship_type) {
      // Чем больше корабли, тем меньше их количество.
      for (int ship_iter = 0; ship_iter < THE_BIGGEST_SHIP - ship_type;
           ++ship_iter) {
        bool is_placed = false;
        while (!is_placed) {
          // Пусть корабль стоит вертикально, значит ограничиваем x
          // Ограничиваем для того, чтобы случайно не попасть за пределы доски
          int x_max = BOARD_SIZE - 1 - ship_type;
          int y_max = BOARD_SIZE - 1;
          bool is_vertical = UniformDist(false, true);
          if (!is_vertical) {
            std::swap(x_max, y_max);
          }
          is_placed = TryPlaceShip(UniformDist(0, x_max), UniformDist(0, y_max),
                                   ship_type, ship_iter, is_vertical);
          if (++iteration > 1000 && !is_placed) {
            Clear(board_);
            ship_type = 0;
            ship_iter = 0;
            iteration = 0;
            std::cout << "Refresh, iteration count > 1000\n";
          }
        }
      }
    }
    std::cout << "Iteration count: " << iteration << '\n';
  }
  void Attack(const std::string& attack) {
    char x = attack.at(1) - '0';
    char y = attack.at(0) - 'a';
    if (hiden_board_[x][y].mark == ' ') {
      if (board_[x][y].mark == 'X') {
        hiden_board_[x][y].mark = 'X';

        if (--ships_lifes_[board_[x][y].id] == 0) {
          if (--ships_alive_count_ == 0) {
            win_ = true;
          }
          vector<vector<bool>> visited(BOARD_SIZE, vector<bool>());
          visited.assign(BOARD_SIZE, vector<bool>(BOARD_SIZE));
          Brush(x, y, visited);
        }
      } else {
        hiden_board_[x][y].mark = '@';
      }
    }
  }
  bool CheckWin() { return win_; }

 private:
  int UniformDist(int from, int to) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(from, to);
    return distrib(gen);
  }
  bool CheckCollision(int x, int y, int ship_type, bool is_vertical) {
    for (int i = x; i <= x + is_vertical * ship_type; ++i) {
      for (int j = y; j <= y + !is_vertical * ship_type; ++j) {
        if (board_[i][j].mark != ' ') {
          return false;
        }
      }
    }
    return true;
  }
  bool CheckBoardLimit(int x, int y) {
    return 0 <= x && x < BOARD_SIZE && 0 <= y && y < BOARD_SIZE;
  }
  bool CheckShipContinue(int x, int y) {
    return CheckBoardLimit(x, y) && board_[x][y].mark == 'X';
  }
  void Brush(int x, int y, vector<vector<bool>>& visited) {
    visited[x][y] = true;
    if (CheckShipContinue(x, y + 1) && !visited[x][y + 1]) {
      Brush(x, y + 1, visited);
    }
    if (CheckShipContinue(x, y - 1) && !visited[x][y - 1]) {
      Brush(x, y - 1, visited);
    }
    if (CheckShipContinue(x + 1, y) && !visited[x + 1][y]) {
      Brush(x + 1, y, visited);
    }
    if (CheckShipContinue(x - 1, y) && !visited[x - 1][y]) {
      Brush(x - 1, y, visited);
    }

    for (int i = x - 1; i <= x + 1; ++i) {
      for (int j = y - 1; j <= y + 1; ++j) {
        if (CheckBoardLimit(i, j) && hiden_board_[i][j].mark == ' ') {
          hiden_board_[i][j].mark = '@';
        }
      }
    }
  }
  bool TryPlaceShip(int x, int y, int ship_type, int ship_iter,
                    bool is_vertical) {
    int ship_id = ships_lifes_.size();
    if (CheckCollision(x, y, ship_type, is_vertical)) {
      for (int i = x - 1; i <= x + is_vertical * ship_type + 1; ++i) {
        for (int j = y - 1; j <= y + !is_vertical * ship_type + 1; ++j) {
          if (0 <= i && i < BOARD_SIZE && 0 <= j && j < BOARD_SIZE) {
            board_[i][j].mark = '@';
          }
        }
      }
      for (int i = x; i <= x + is_vertical * ship_type; ++i) {
        for (int j = y; j <= y + !is_vertical * ship_type; ++j) {
          board_[i][j].mark = 'X';
          board_[i][j].id = ships_lifes_.size();
        }
      }
      ++ships_alive_count_;
    } else {
      return false;
    }
    // Массив нужен нам, чтобы понять, когда корабль будет уничтожен
    ships_lifes_.push_back(ship_type + 1);
    return true;
  }
  /*
  // Рандомная расстановка кораблей на доске
  void TrueRandomArrangement(uint8_t) {
    int placed = 0;
    for (int ship_type = 0; ship_type < THE_BIGGEST_SHIP; ++ship_type) {
      // Чем больше корабли, тем меньше их количество.
      for (int ship_iter = 0; ship_iter < THE_BIGGEST_SHIP - ship_type;
           ++ship_iter) {
        // Выбор направления
        bool is_vertical = UniformDist(false, true);
        // Сокращаем доску, для того, чтобы конец корабля не был за пределами
        // доски. А выбор позиции, происходит из оставшихся свободных. То есть,
        // занятыми считаются клетки, которые попали в сокращение доски и
  корабли,
        // которые были расставлены.
        int reduced_board = BOARD_SIZE * ship_type;
        int position =
            UniformDist(0, BOARD_SIZE * BOARD_SIZE - reduced_board - placed);
        // Теперь отситываем от начала доски эту позкрицию,
        // пропуская занятые и ставим в неё корабль.
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
          if (board_[i / BOARD_SIZE][i % BOARD_SIZE].mark == ' ' &&
                  (i % BOARD_SIZE < BOARD_SIZE - count_reduced_board??? &&
                   !is_vertical) ||
              (i / BOARD_SIZE < BOARD_SIZE - ???reduced_board??? &&
  is_vertical)) { if (position <= 0) { board_[i / BOARD_SIZE][i %
  BOARD_SIZE].mark = 'X'; for (int j = ; j < BOARD_SIZE; ++j)
            }
          }
        }
      }
    }
  }
  */

 private:
  struct Cell {
    int id = -1;
    char mark = ' ';
  };
  void Clear(vector<vector<Cell>>& board) {
    for (int i = 0; i < BOARD_SIZE; ++i) {
      for (int j = 0; j < BOARD_SIZE; ++j) {
        board[i][j].id = -1;
        board[i][j].mark = ' ';
      }
    }
  }
  vector<vector<Cell>> board_;
  vector<vector<Cell>> hiden_board_;
  vector<int> ships_lifes_;
  int ships_alive_count_ = 0;
  int ship_alive_ = 0;
  bool win_ = false;
};