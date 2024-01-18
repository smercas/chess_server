#pragma once

#include "../../common/enums.h"
#include "board.h"

//

#include <array>
#include <optional>
#include <string>

class game {
public:
  static void start_game(int, int);
private:
  // will disconnect the user and close its socket
  static void disconnect_player_and_close(int);
  // used for sockets that recieved a abort_match message
  // sends a confirmation message
  // if send fails, it disconnects the user and closes its socket through the user recv_send_fail_handler or disconnect_player_and_close
  // otherwise the socket is placed in the big_poll
  static void handle_abort(int);
  // used for sockets that recieved a quit message
  // sends a confirmation message
  // if send fails, it disconnects the user and closes its socket through the user recv_send_fail_handler or disconnect_player_and_close
  // otherwise it does the same thing but with disconnect_player_and_close
  // the main differnces between recv_send_fail_handler and disconnect_player_and_close is that
  // the former prints an eror message and checks if the socket is still valid
  static void handle_quit(int);
  // used when the opponent disconnects
  // sends the message if one is passed, or forfeit
  // if send fails, it disconnects the user and closes its socket with recv_send_fail_handler or disconnect_player_and_close
  // otherwise the socket is placed in the big_poll
  static void handle_opponent_disconnect(int, message = message::forfeit);
  // consumes the message
  // if recv fails, it disconnects the user and closes its socket through the user recv_send_fail_handler or disconnect_player_and_close, then returns false
  // else returns true
  static bool consume_message(int);
  // this is its own function only because it repeats 3 times
  // basically sends message and moveset in a single buffer of size (sizeof(message) + 3 * sizeof(uint8_t))
  // flags, if none are passed, default to 0
  static ssize_t send_move(int, message, std::array<uint8_t, 3>, int = 0);
  game(int, int);
  // this is the place where player messsages are processed
  void play_game(int);
  int get_other_player(int);

  // this repeats a lot so it's its own function
  // uses disconnect_player_and_close on both players, then closes the passed epoll_fd
  void disconnect_both_players_and_poll(int);
  game() = delete;
  game(const game &) = delete;
  game(game &&) = delete;
  game &operator = (const game &) = delete;
  game &operator = (game &&) = delete;
  ~game() = default;

  std::array<int, 2> m_players;
  board m_board;
};
