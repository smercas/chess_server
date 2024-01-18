#include "game.h"

#include "big_poll.h"
#include "player_queue.h"
#include "../../common/utils.h"

#include <fcntl.h>
#include <string.h>

#include <random>
#include <iostream>

void game::start_game(int first_player, int second_player) {
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    error_print("start_game epoll_create");
    disconnect_player_and_close(first_player);
    disconnect_player_and_close(second_player);
    return;
  }

  epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = first_player;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, first_player, &ev) == -1) {
    error_print("start_game epoll_ctl add first_player");
    disconnect_player_and_close(first_player);
    disconnect_player_and_close(second_player);
    if (close(epoll_fd) == -1) { error_print("start_game epoll_fd close"); }
    return;
  }

  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = second_player;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, second_player, &ev) == -1) {
    error_print("start_game epoll_ctl add second_player");
    disconnect_player_and_close(first_player);
    disconnect_player_and_close(second_player);
    if (close(epoll_fd) == -1) { error_print("start_game epoll_fd close"); }
    return;
  }

  bool t;
  {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution distribution(0, 1);

    t = static_cast<bool>(distribution(gen));
  }
  //true  -> first player is white, second player is black
  //false -> first player is black, second player is white

  message first_message = (t ? message::white : message::black);
  ssize_t send_retval = send(first_player, &first_message, sizeof(message), 0);
  if (send_retval == -1 || send_retval == 0) {
    if (send_retval == -1) { user::recv_send_fail_handler(first_player, "start_game white player color send"); }
    else { disconnect_player_and_close(first_player); }

    if (fcntl(second_player, F_GETFD) == -1 && errno == EBADFD) {
      user::disconnectUser(second_player);
    } else {
      fprintf(stderr, "relocating the black file descriptor %d back to the queue\n", second_player);
      player_queue::add_socket(second_player);
    }

    if (close(epoll_fd) == -1) { error_print("start_game epoll_fd close"); }
    return;
  }

  message second_message = (t ? message::black : message::white);
  send_retval = send(second_player, &second_message, sizeof(message), 0);
  if (send_retval == -1 || send_retval == 0) {
    if (send_retval == -1) { user::recv_send_fail_handler(second_player, "start_game black player color send"); }
    else { disconnect_player_and_close(first_player); }

    if (fcntl(first_player, F_GETFD) == -1 && errno == EBADFD) {
      user::disconnectUser(first_player);
    } else {
      fprintf(stderr, "relocating the white file descriptor %d back to the queue\n", first_player);
      player_queue::add_socket(first_player);
    }

    if (close(epoll_fd) == -1) { error_print("start_game epoll_fd close"); }
    return;
  }

  game instance(t ? first_player : second_player, t ? second_player : first_player);
  instance.play_game(epoll_fd);
}
void game::disconnect_player_and_close(int fd) {
  user::disconnectUser(fd);
  if (close(fd) == -1) { error_print("player close"); }
}
void game::handle_abort(int fd) {
  message to_send = message::confirmation;
  ssize_t send_retval = send(fd, &to_send, sizeof(message), 0);
  if (send_retval == -1 || send_retval == 0) {
    if (send_retval == -1) { user::recv_send_fail_handler(fd, "player abort confirmation send"); }
    else { disconnect_player_and_close(fd); }
  } else {
    fprintf(stderr, "successfully forfeited the match for player %d\n", fd);
    big_poll::add_socket(fd);
  }
}
void game::handle_quit(int fd) {
  message to_send = message::confirmation;
  ssize_t send_retval = send(fd, &to_send, sizeof(message), 0);
  if (send_retval == -1 || send_retval == 0) {
    if (send_retval == -1) { user::recv_send_fail_handler(fd, "player quit confirmation send"); }
    else { disconnect_player_and_close(fd); }
  } else {
    fprintf(stderr, "successfully exited the match (and the game) for player %d\n", fd);
    disconnect_player_and_close(fd);
  }
}
void game::handle_opponent_disconnect(int fd, message to_send) {
  ssize_t send_retval = send(fd, &to_send, sizeof(message), 0);
  if (send_retval == -1 || send_retval == 0) {
    if (send_retval == -1) { user::recv_send_fail_handler(fd, "player forfeit send"); }
    else { disconnect_player_and_close(fd); }
  } else {
    fprintf(stderr, "successfully send player %d to the main menu\n", fd);
    big_poll::add_socket(fd);
  }
}
ssize_t game::send_move(int fd, message msg, std::array<uint8_t, 3> moveset, int flags) {
  char sendbuf[sizeof(message) + 3 * sizeof(uint8_t)];
  memcpy(sendbuf, &msg, sizeof(message));
  memcpy(sendbuf + sizeof(message), moveset.data(), 3 * sizeof(uint8_t));
  return send(fd, sendbuf, sizeof(message) + 3 * sizeof(uint8_t), flags);
}
bool game::consume_message(int fd) {
  char buf[4];
  ssize_t recv_retval = recv(fd, buf, 4, MSG_DONTWAIT);
  if (recv_retval == 0 || (recv_retval == -1 && errno != EWOULDBLOCK && errno != EAGAIN)) {
    fprintf(stderr, "failed to exhaust the contents of %d\n", fd);
    if (recv_retval == -1) { user::recv_send_fail_handler(fd, "player forfeit send"); }
    else { disconnect_player_and_close(fd); }
    return false;
  }
  return true;
}
game::game(int first_player, int second_player) : m_players({first_player, second_player}), m_board() {}
void game::play_game(int epoll_fd) {
  while (true) {
    std::array<epoll_event, 2> player_events;
    int nfds;
    while ((nfds = epoll_wait(epoll_fd, player_events.data(), 2, -1)) == -1 && errno != EINTR) {}
    if (nfds == -1) {
      error_print("okay_game epoll_wait");
      disconnect_both_players_and_poll(epoll_fd);
      return;
    }
    if (nfds == 0) {
      disconnect_both_players_and_poll(epoll_fd);
      return;
    }
    //
    // E - epoll error
    // R - recv error
    // A - recv returned message::abort_match
    // Q - recv returned message::quit
    // M - recv returned message::move
    // O - recv returned anything else (client compromised)
    if (nfds == 2) {
      std::array<int, 2> player_fd = { player_events[0].data.fd, player_events[1].data.fd };
      std::array<bool, 2> player_ev_err = { static_cast<bool>(player_events[0].events & (EPOLLPRI | EPOLLERR | EPOLLRDHUP | EPOLLHUP)),
                                            static_cast<bool>(player_events[1].events & (EPOLLPRI | EPOLLERR | EPOLLRDHUP | EPOLLHUP)) };
      std::array<message, 2> player_message;
      std::array<ssize_t, 2> player_recv_retval;
      std::array<int, 2> player_errno;
      bool turn_of = static_cast<bool>(m_board.turn());
      if (player_fd[0] != m_players[0]) {
        turn_of = !turn_of;
      }
      for (size_t i = 0; i < 2; i += 1) {
        if (player_ev_err[i] == false) {
          player_recv_retval[i] = recv(player_fd[i], &player_message[i], sizeof(message), 0);
          if (player_recv_retval[i] == 0) { player_ev_err[i] = true; }
          player_errno[i] = errno;
        }
      }
      // to | nto
      //----+-----
      // E  | E
      if (player_ev_err[0] && player_ev_err[1]) {
        fprintf(stderr, "both players are erronious, will cancel the match and disconnect both of them (%d and %d)\n", m_players[0], m_players[1]);
        disconnect_both_players_and_poll(epoll_fd);
        return;
      }
      // to | nto
      //----+-----
      // R  | R
      if (player_recv_retval[0] == -1 && player_recv_retval[1] == -1) {
        for (size_t i = 0; i < 2; i += 1) {
          user::recv_send_fail_handler(player_fd[i], "player message recv", player_errno[i]);
        }
        if (close(epoll_fd) == -1) { error_print("play_game 2 msg poll close"); }
        return;
      }
      // to | nto
      //----+-----
      // E  | R
      // R  | E
      for (size_t j = 0; j < 2; j += 1) {
        bool i = static_cast<bool>(j);
        if (player_ev_err[i] && player_recv_retval[!i] == -1) {
          disconnect_player_and_close(player_fd[i]);
          user::recv_send_fail_handler(player_fd[!i], "player message recv", player_errno[!i]);
          if (close(epoll_fd) == -1) { error_print("play_game 2 msg poll close"); }
          return;
        }
      }
      // to | nto
      //----+-----
      // E  | A
      // E  | Q
      // E  | M
      // E  | O
      // A  | E
      // Q  | E
      // O  | E
      for (size_t j = 0; j < 2; j += 1) {
        bool i = static_cast<bool>(j);
        if (player_ev_err[i]) {
          if (player_message[!i] == message::abort_match) {
            disconnect_player_and_close(player_fd[i]);
            handle_abort(player_fd[!i]);
            if (close(epoll_fd) == -1) { error_print("invalid message poll close"); }
            return;
          } else if (player_message[!i] == message::quit) {
            disconnect_player_and_close(player_fd[i]);
            handle_quit(player_fd[!i]);
            if (close(epoll_fd) == -1) { error_print("invalid message poll close"); }
            return;
          } else if (i == turn_of || player_message[!i] != message::move) {
            // if it's i's turn then any other mesages are invalid
            // if it's not i's turn then any messages except move are invalid
            // i == turn_of || (!i == turn_of && player_message[!i] != message::move)
            // i == turn_of || (i != turn_of && player_message[!i] != message::move)
            std::cerr << "recieved invalid message (" << get_message_as_text(player_message[!i]) << ") from socket ";
            std::cerr << player_fd[!i] << "; since the other socket is already invalid both sockets will be disconnected" << std::endl;
            disconnect_both_players_and_poll(epoll_fd);
            return;
          }
        }
      }
      // to | nto
      //----+-----
      // R  | A
      // R  | Q
      // R  | M
      // R  | O
      // A  | R
      // Q  | R
      // O  | R
      for (size_t j = 0; j < 2; j += 1) {
        bool i = static_cast<bool>(j);
        if (player_recv_retval[i] == -1) {
          if (player_message[!i] == message::abort_match) {
            user::recv_send_fail_handler(player_fd[i], "player message recv", player_errno[i]);
            handle_abort(player_fd[!i]);
            if (close(epoll_fd) == -1) { error_print("invalid message poll close"); }
            return;
          } else if (player_message[!i] == message::quit) {
            user::recv_send_fail_handler(player_fd[i], "player message recv", player_errno[i]);
            handle_abort(player_fd[!i]);
            if (close(epoll_fd) == -1) { error_print("invalid message poll close"); }
            return;
          } else if (!(!i == turn_of && player_message[!i] == message::move)) {
            //loof at the previous for loop if you don't understand this cpondition
            std::cerr << "recieved invalid message (" << get_message_as_text(player_message[!i]) << ") from socket ";
            std::cerr << player_fd[!i] << "; since the other socket is already invalid both sockets will be disconnected" << std::endl;
            user::recv_send_fail_handler(player_fd[i], "player message recv", player_errno[i]);
            disconnect_player_and_close(player_fd[!i]);
            if (close(epoll_fd) == -1) { error_print("invalid message poll close"); }
            return;
          }
        }
      }
      //at this point, turn_of does not have errors

      // to | nto
      //----+-----
      // M  | E
      // M  | R
      // M  | A
      // M  | Q
      // M  | M
      // M  | O
      if (player_message[turn_of] == message::move) {
        bool is_abort_or_quit = false;
        if (player_ev_err[!turn_of]) {
          // E
          disconnect_player_and_close(player_fd[!turn_of]);
        } else if (player_recv_retval[!turn_of] == -1) {
          // R
          user::recv_send_fail_handler(player_fd[!turn_of], "player message recv", player_errno[!turn_of]);
        } else if (player_message[!turn_of] != message::abort_match && player_message[!turn_of] != message::quit) {
          // M and O
          std::cerr << "recieved invalid message (" << get_message_as_text(player_message[!turn_of]) << ") from socket ";
          std::cerr << player_fd[!turn_of] << "; it'll be disconnected" << std::endl;
          disconnect_player_and_close(player_fd[!turn_of]);
        } else {
          is_abort_or_quit = true;
        }

        std::array<uint8_t, 3> moveset;
        ssize_t recv_retval = recv(player_fd[turn_of], moveset.data(), 3 * sizeof(uint8_t), 0);
        if (recv_retval == -1 || recv_retval == 0) {
          if (recv_retval == -1) { user::recv_send_fail_handler(player_fd[turn_of], "player move recv"); }
          else { disconnect_player_and_close(player_fd[turn_of]); }

          if (is_abort_or_quit) {
            if (player_message[!turn_of] == message::abort_match) {
              handle_abort(player_fd[!turn_of]);
            } else  if (player_message[!turn_of] == message::quit) {
              handle_quit(player_fd[!turn_of]);
            }
          }
          if (close(epoll_fd) == -1) { error_print("play_game 2 msg poll close"); }
          return;
        }
        auto &&[source, destination, promotion] = destructured_move(moveset);
        //for mover:
        // won -> won
        // draw -> draw
        // confirmation -> forfeit
        // rejection -> forfeit

        message move_retval = m_board.check_move(source, destination, promotion);
        message to_send_to_mover = move_retval;
        if (move_retval == message::confirmation || move_retval == message::rejection) {
          to_send_to_mover = message::forfeit;
        }
        handle_opponent_disconnect(player_fd[turn_of], to_send_to_mover);

        if (is_abort_or_quit) {
          //for optional forfeiter:
          // won -> lost + move
          // draw -> draw + move
          // confirmation -> confirmation + move
          // rejection -> confirmation
          message to_send_to_forfeiter = move_retval;
          if (move_retval == message::won) {
            to_send_to_forfeiter = message::lost;
          }
          if (move_retval == message::rejection) {
            to_send_to_forfeiter = message::confirmation;
          }
          ssize_t send_retval;
          if (move_retval != message::rejection) {
            //add moveset to the forfeiter message if it loses or draws
            send_retval = send_move(player_fd[!turn_of], to_send_to_forfeiter, moveset);
          } else {
            send_retval = send(player_fd[!turn_of], &to_send_to_forfeiter, sizeof(message), 0);
          }
          if (send_retval == -1 || send_retval == 0) {
            if (send_retval == -1) { user::recv_send_fail_handler(player_fd[!turn_of], "forfeiter move/confirmation send"); }
            else { disconnect_player_and_close(player_fd[!turn_of]); }
          } else {
            if (player_message[!turn_of] == message::abort_match) {
              fprintf(stderr, "successfully forfeited the match for player %d\n", player_fd[!turn_of]);
              big_poll::add_socket(player_fd[!turn_of]);
            } else if (player_message[!turn_of] == message::quit) {
              fprintf(stderr, "successfully exited the match for player %d\n", player_fd[!turn_of]);
              disconnect_player_and_close(player_fd[!turn_of]);
            }
          }
        }
        if (close(epoll_fd) == -1) { error_print("play_game 2 msg poll close"); }
        return;
      }
      //at this point, all E and R cases have been treated on both sides

      // to | nto
      //----+-----
      // A  | A
      // A  | Q
      // Q  | A
      // Q  | Q
      if ((player_message[0] == message::abort_match || player_message[0] == message::quit) && (player_message[1] == message::abort_match || player_message[1] == message::quit)) {
        for (size_t i = 0; i < 2; i += 1) {
          if (player_message[i] == message::abort_match) {
            handle_abort(player_fd[i]);
          } else {
            handle_quit(player_fd[i]);
          }
        }
        if (close(epoll_fd) == -1) { error_print("play_game 2 msg poll close"); }
        return;
      }
      //at this point, both sides are a message, but not both are valid messages

      // to | nto
      //----+-----
      // A  | M
      // A  | O
      // Q  | M
      // Q  | O
      // O  | A
      // O  | Q
      for (size_t j = 0; j < 2; j += 1) {
        bool i = static_cast<bool>(j);
        if (player_message[i] == message::abort_match) {
          handle_abort(player_fd[i]);
        } else if (player_message[i] == message::quit) {
          handle_quit(player_fd[i]);
        } else { continue; }
        std::cerr << "recieved invalid message (" << get_message_as_text(player_message[!i]);
        std::cerr << ") from socket " << player_fd[!i] << "; it'll be disconnected" << std::endl;
        disconnect_player_and_close(player_fd[!i]);
        if (close(epoll_fd) == -1) { error_print("play_game 2 msg poll close"); }
        return;
      }
      // to | nto
      //----+-----
      // O  | M
      // O  | O
      std::cerr << "recieved invalid messages (" << get_message_as_text(player_message[0]) << " and " << get_message_as_text(player_message[1]);
      std::cerr << ") from both sockets (" << player_fd[0] << " and " << player_fd[1] << "); will cancel the match and disconnect both of them" << std::endl;
      disconnect_both_players_and_poll(epoll_fd);
      return;
    }
    if (nfds == 1) {
      int active_fd = player_events[0].data.fd;
      int other_fd = get_other_player(active_fd);
      if (player_events[0].events & (EPOLLPRI | EPOLLERR | EPOLLRDHUP | EPOLLHUP)) {
        disconnect_player_and_close(active_fd);
        if (consume_message(other_fd)) { handle_opponent_disconnect(other_fd); }
        if (close(epoll_fd) == -1) { error_print("poll close"); }
        return;
      }
      message to_recv;
      ssize_t recv_retval = recv(active_fd, &to_recv, sizeof(message), 0);
      if (recv_retval == -1 || recv_retval == 0) {
        if (recv_retval == -1 ) { user::recv_send_fail_handler(active_fd, "player message recv"); }
        else { disconnect_player_and_close(active_fd); }

        if (consume_message(other_fd)) { handle_opponent_disconnect(other_fd); }
        if (close(epoll_fd) == -1) { error_print("poll close"); }
        return;
      }
      if (to_recv != message::move || (to_recv == message::move && active_fd != m_players[static_cast<bool>(m_board.turn())])) {
        if (to_recv == message::abort_match) {
          handle_abort(active_fd);
        } else if (to_recv == message::quit) {
          handle_quit(active_fd);
        } else {
          std::cerr << "recieved invalid message (" << get_message_as_text(to_recv) << ") from socket ";
          std::cerr << active_fd << ", so it'll be disconnected" << std::endl;
          disconnect_player_and_close(active_fd);
        }
        if (consume_message(other_fd)) { handle_opponent_disconnect(other_fd); }
        if (close(epoll_fd) == -1) { error_print("poll close"); }
        return;
      }
      std::array<uint8_t, 3> moveset;
      recv_retval = recv(active_fd, moveset.data(), 3 * sizeof(uint8_t), 0);
      if (recv_retval == -1 || recv_retval == 0) {
        if (recv_retval == -1) { user::recv_send_fail_handler(active_fd, "player move recv"); }
        else { disconnect_player_and_close(active_fd); }

        if (consume_message(other_fd)) { handle_opponent_disconnect(other_fd); }
        if (close(epoll_fd) == -1) { error_print("poll close"); }
        return;
      }
      auto &&[source, destination, promotion] = destructured_move(moveset);

      message move_retval = m_board.check_move(source, destination, promotion);
      message to_send = move_retval;
      ssize_t send_retval = send(active_fd, &move_retval, sizeof(message), 0);
      if (send_retval == -1 || send_retval == 0) {
        if (send_retval == -1) { user::recv_send_fail_handler(active_fd, "move validity send"); }
        else { disconnect_player_and_close(active_fd); }
        if (consume_message(other_fd)) {
          // won -> lost + moveset
          // draw -> draw + moveset
          // confirmation -> forfeit + moveset
          // rejection -> forfeit
          if (move_retval == message::won) {
            to_send = message::lost;
          } else if (move_retval == message::confirmation || move_retval == message::rejection) {
            to_send = message::forfeit;
          }
          if (move_retval != message::rejection) {
            send_retval = send_move(other_fd, to_send, moveset);
          } else {
            send_retval = send(other_fd, &to_send, sizeof(message), 0);
          }
          if (send_retval == -1 || send_retval == 0) {
            if (send_retval == -1) { user::recv_send_fail_handler(other_fd, "other player forfeit/lost/draw send"); }
            else { disconnect_player_and_close(other_fd); }
          } else {
            big_poll::add_socket(other_fd);
          }
        }
        if (close(epoll_fd) == -1) { error_print("poll close"); }
        return;
      }
      //normal message for opposing player
      // won -> lost + moveset
      // draw -> draw + moveset
      // confirmation -> move + moveset
      // rejection -> 
      if (move_retval == message::won) {
        to_send = message::lost;
      } else if (move_retval == message::confirmation) {
        to_send = message::move;
      }

      if (move_retval != message::rejection) {
        send_retval = send_move(other_fd, to_send, moveset);
        if (send_retval == -1 || send_retval == 0) {
          if (send_retval == -1) { user::recv_send_fail_handler(other_fd, "player send move"); }
          else { disconnect_player_and_close(other_fd); }

          handle_opponent_disconnect(active_fd);
          if (close(epoll_fd) == -1) { error_print("poll close"); }
          return;
        }
        if (move_retval != message::confirmation) {
          big_poll::add_socket(active_fd);
          big_poll::add_socket(other_fd);
          if (close(epoll_fd) == -1) { error_print("poll close"); }
          return;
        }
      }
    }
  }
}
int game::get_other_player(int fd) {
  return fd != m_players[0] ? m_players[0] : m_players[1];
}
void game::disconnect_both_players_and_poll(int epoll_fd) {
  for (int player_fd : m_players) {
    disconnect_player_and_close(player_fd);
  }
  if (close(epoll_fd) == -1) { error_print("disconnect_both_players_and_poll poll close"); }
}
