#include <cstring>
#include <vector>
#include <string>
#include <iostream>
#include <thread>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>

#include "../../common/enums.h"
#include "../../common/utils.h"

enum class state {
  not_logged_in,
  logged_in,
  searching,
  in_game,
};

std::vector<std::string_view> get_words(const std::string_view& original) {
  size_t i = 0; // general index
  size_t j = 0; // individual word index
  std::vector<std::string_view> result;
  while (i < original.length()) {
    if (strchr("-_", original[i]) || isalnum(original[i])) {
      j += 1;
    } else {
      if (j != 0) {
        result.push_back(std::string_view(original.data() + i - j, j));
        j = 0;
      }
    }
    i += 1;
  }
  return result;
}

int get_connected_socket(const char *port) {
  //setting up the hints field for getaddrinfo()
  struct addrinfo hints; {
    //node == NULL -> uses INADDR_LOOPBACK
    //AI_NUMERICSERV -> skips the service name resolution, needs a string of numbers
    hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED | AI_ALL | AI_NUMERICSERV;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_addrlen = 0;
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;
    hints.ai_next = NULL;
  }
  //pointers for storage and traversal of the return from getaddrinfo()
  addrinfo *addrinfos, *elem;
  int gai_retval = getaddrinfo(NULL, port, &hints, &addrinfos);
  if (gai_retval != 0) {
    fprintf(stderr, "getaddrinfo");
    if (gai_retval == EAI_SYSTEM) {
      fprintf(stderr, " (%s): %s\n", strerrorname_np(errno), strerrordesc_np(errno));
    } else {
      fprintf(stderr, " %d: %s\n", gai_retval, gai_strerror(gai_retval));
    }
    exit(EXIT_FAILURE);
  }
  int sfd;
  for (elem = addrinfos; elem != NULL; elem = elem->ai_next) {
    sfd = socket(elem->ai_family, elem->ai_socktype, elem->ai_protocol);
    if (sfd == -1) {
      continue;
    }
    if (connect(sfd, elem->ai_addr, elem->ai_addrlen) == 0) {
      break;
    }
    close(sfd);
  }
  freeaddrinfo(addrinfos);
  if (elem == NULL) {
    fprintf(stderr, "Could not bind.\n");
    exit(EXIT_FAILURE);
  }
  return sfd;
}

//first optional represents errors different from EAGAIN and EWOULDBLOCK
//second optional represents EAGAIN or EWOULDBLOCK errors, meaning no object was obtained
std::optional<std::optional<std::array<uint8_t, 3>>> get_move_opt(int sfd) {
  std::array<uint8_t, 3> retval;
  ssize_t recv_retval = recv(sfd, retval.data(), 3, MSG_DONTWAIT);
  if (recv_retval == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
    return std::optional<std::optional<std::array<uint8_t, 3>>>();
  } else if (recv_retval == -1) {
    return std::optional<std::optional<std::array<uint8_t, 3>>>(std::optional<std::array<uint8_t, 3>>());
  } else {
    return retval;
  }
}

std::optional<std::array<uint8_t, 3>> get_move(int sfd) {
  auto r =  get_move_opt(sfd);
  if (r.has_value() == false) { return {}; }
  else { return r.value(); }
}

int main() {
  const char *port = "2048";
  // socket file descriptor
  int sfd = get_connected_socket(port);

  // epoll stuff for option to abort
  epoll_event ev, events[2];
  int epollfd = epoll_create1(0);
  if (epollfd == -1) { error_print("epoll create"); exit(EXIT_FAILURE); }
  ev.events = EPOLLIN;
  ev.data.fd = sfd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd, &ev) == -1) { error_print("epoll add socket"); exit(EXIT_FAILURE); }
  ev.events = EPOLLIN;
  ev.data.fd = STDIN_FILENO;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) { error_print("epoll add stdin"); exit(EXIT_FAILURE); }

  state client_state = state::not_logged_in;

  ssize_t command_length;
  char command[4096 + 1];

  char sendbuf[1 + 1 + 1 + 256 + 256];
  
  message recv_msg;
  color player_color;
  bool first_move;

  while (true) {
    switch (client_state) {
      case state::not_logged_in: {
        fprintf(stdout, "not logged in, you can log in (example: login ex_name ex_passwd), register (example: register ex_name ex_paswd) or quit:\n");
        command_length = read(STDIN_FILENO, command, sizeof(command));
        if (command_length == -1) { error_print("command read"); exit(EXIT_FAILURE); }
        command[command_length] = '\0';
        auto &&words = get_words(command);

        message to_send;
        if (words[0] == "quit") {
          sendbuf[0] = (char)message::quit;
          if (send(sfd, &sendbuf, sizeof(message), 0) == -1) { error_print("quit send"); continue; }

          if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("server confirmation recv"); continue; }
          fprintf(stdout, "exited\n");
          exit(EXIT_SUCCESS);
        } else if (words[0] == "register") {
          to_send = message::signup_data;
        } else if (words[0] == "login") {
          to_send = message::login_data;
        } else {
          fprintf(stdout, "invalid command\n");
          continue;
        }
        memcpy(sendbuf, &to_send, sizeof(message));
        uint8_t to_send_len = words[1].length();
        memcpy(sendbuf + sizeof(message), &to_send_len, sizeof(uint8_t));
        to_send_len = words[2].length();
        memcpy(sendbuf + sizeof(message) + sizeof(uint8_t), &to_send_len, sizeof(uint8_t));
        strncpy(sendbuf + 3, words[1].data(), words[1].length());
        strncpy(sendbuf + 3 + words[1].length(), words[2].data(), words[2].length());
        size_t sendbuf_len = 3 + words[1].length() + words[2].length();
        if (send(sfd, sendbuf, sendbuf_len, 0) == -1) { error_print("username + password send"); continue; }

        if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("server confirmation recv"); continue; }
        if (recv_msg == message::confirmation) {
          fprintf(stdout, "login successful\n");
          client_state = state::logged_in;
        } else if (recv_msg == message::rejection) {
          fprintf(stdout, "login failed\n");
        }
      } break;
      case state::logged_in: {
        fprintf(stdout, "logged in, you can start looking for a match (with: play), log out (with: logout), delete your account(with: delete-account) or quit:\n");
        command_length = read(STDIN_FILENO, command, sizeof(command));
        if (command_length == -1) { error_print("command read"); exit(EXIT_FAILURE); }
        command[command_length] = '\0';
        auto &&words = get_words(command);

        if (words[0] == "play") {
          sendbuf[0] = (char)message::play;
          if (send(sfd, sendbuf, sizeof(message), 0) == -1) { error_print("play send"); continue; }

          fprintf(stdout, "searching...\n");
          client_state = state::searching;
        } else if (words[0] == "logout") {
          sendbuf[0] = (char)message::logout;
          if (send(sfd, sendbuf, sizeof(message), 0) == -1) { error_print("logout send"); continue; }

          if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("server confirmation recv"); continue; }
          fprintf(stdout, "logged out\n");
          client_state = state::not_logged_in;
        } else if (words[0] == "delete_account") {
          sendbuf[0] = (char)message::delete_account;
          if (send(sfd, sendbuf, sizeof(message), 0) == -1) { error_print("delete-account send"); continue; }

          if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("delete-account server confirmation recieve"); continue; }
          if (recv_msg == message::confirmation) {
            fprintf(stdout, "account deletion suiccessful, not you're not logged in\n");
            client_state = state::not_logged_in;
          } else if (recv_msg == message::rejection) {
            fprintf(stdout, "failed to delete your account, idk what to tell you\n");
          }
        } else if (words[0] == "quit") {
          sendbuf[0] = (char)message::quit;
          if (send(sfd, sendbuf, sizeof(message), 0) == -1) { error_print("quit send"); continue; }

          if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("server confirmation recv"); continue; }
          fprintf(stdout, "exited\n");
          exit(EXIT_SUCCESS);
        } else {
          fprintf(stdout, "invalid command\n");
          continue;
        }
      } break;
      case state::searching: {
        fprintf(stdout, "searching for an opponent, you can abort the search anytime (before finding a match) or even disconnect\n");
        int nfds = epoll_wait(epollfd, events, 2, -1);
        if (nfds == -1) {
          error_print("search epoll wait"); continue;
        } else if (nfds == 2 || (nfds == 1 && events[0].data.fd == sfd)) {
          //if nfds is 2 then both stdin and the server have returned an answer at the same time, a very unfortunate happenstance, server gets the priority
          if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("search thread recv"); continue; }
          //recv_msg will just be white or black, doubles as a confirmation that an opponent was found and the match can start
          client_state = state::in_game;
          fprintf(stdout, "found opponent, starting as ");
          if (recv_msg == message::white) {
            player_color = color::white;
            first_move = true;
            fprintf(stdout, "white\n");
          } else if (recv_msg == message::black) {
            player_color = color::black;
            first_move = false;
            fprintf(stdout, "black\n");
          }
        } else {
          command_length = read(STDIN_FILENO, command, sizeof(command));
          if (command_length == -1) { error_print("command read"); exit(EXIT_FAILURE); }
          command[command_length] = '\0';
          auto &&words = get_words(command);

          if (words[0] == "abort") {
            sendbuf[0] = (char)message::abort_match;
            if (send(sfd, sendbuf, sizeof(message), 0) == -1) { error_print("abort while searching send"); continue; }

            if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("server confirmation recv"); continue; }  //can only recieve a confirmation from the server
            fprintf(stdout, "aborted the search\n");
            client_state = state::logged_in;
            continue;
          } else if (words[0] == "quit") {
            sendbuf[0] = (char)message::quit;
            if (send(sfd, sendbuf, sizeof(message), 0) == -1) { error_print("quit while searching send"); continue; }

            if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("server confirmation recv"); continue; }  //can only recieve a confirmation from the server
            fprintf(stdout, "exited\n");
            exit(EXIT_SUCCESS);
          } else {
            fprintf(stdout, "invalid command\n");
            continue;
          }
        }                                              
      } break;
      case state::in_game: {
        if (first_move == false) {
          fprintf(stdout, "waiting for the opponent's move (you could abort or quit before the opponent moves)...\n");
          int nfds = epoll_wait(epollfd, events, 2, -1);
          if (nfds == -1) {
            error_print("in-game epoll wait");
            continue;
          } else if (nfds == 2 || (nfds == 1 && events[0].data.fd == sfd)) {
            if (nfds == 2) { fprintf(stdout, "your command was ignored due to recieving data from the server at the same time\n"); }
            if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("opponent message recv"); continue; }
            if (recv_msg == message::forfeit) {
              auto &&move_opt = get_move_opt(sfd);
              if (!move_opt) {
                error_print("opponent optional moveset recv");
                continue;
              }
              if (move_opt.value()) {
                fprintf(stdout, "recieved move from opponent, ");
                print_move(move_opt.value().value());
                fprintf(stdout, ", but ");
              }
              fprintf(stdout, "the opponent forfeited the match\n");
            } else {
              auto &&move = get_move(sfd);
              if (!move) {
                error_print("opponent moveset recv");
                continue;
              }
              if (recv_msg == message::lost) {
                fprintf(stdout, "recieved move from the opponent, ");
                print_move(move.value());
                fprintf(stdout, "made you lose\n");
              } else if (recv_msg == message::draw) {
                fprintf(stdout, "recieved move from the opponent, ");
                print_move(move.value());
                fprintf(stdout, "caused a draw\n");
              } else if (recv_msg == message::move) {
                fprintf(stdout, "recieved a move from the opponent: ");
                print_move(move.value());
                std::cout << std::endl;
              }
            }
            if (recv_msg == message::move) {
              fprintf(stdout, "it's your time to move now\n");
            } else {
              fprintf(stdout, "going back to the main menu...\n");
              client_state = state::logged_in;
              continue;
            }
          } else {
            command_length = read(STDIN_FILENO, command, sizeof(command));
            if (command_length == -1) { error_print("command read"); exit(EXIT_FAILURE); }
            command[command_length] = '\0';
            auto &&words = get_words(command);

            if (words[0] == "abort") {
              sendbuf[0] = (char)message::abort_match;
              if (send(sfd, sendbuf, sizeof(message), 0) == -1) { error_print("abort while not turn of send"); continue; }
            } else if (words[0] == "quit") {
              sendbuf[0] = (char)message::quit;
              if (send(sfd, sendbuf, sizeof(message), 0) == -1) { error_print("quit while not turn of send"); continue; }
            } else {
              fprintf(stdout, "invalid command\n");
              continue;
            }

            if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("server confirmation recv"); continue; }

            if (recv_msg == message::confirmation || recv_msg == message::forfeit) {
              if (recv_msg == message::confirmation) {
                if (words[0] == "abort") { fprintf(stdout, "recieved confirmation to abort match\n"); }
                else { fprintf(stdout, "recieved confirmation to exit match\n"); }
              } else {
                fprintf(stdout, "opponent forfeited/disconnected before you could");
                if (words[0] == "quit") { fprintf(stdout, ", will still quit the game tho"); }
                fprintf(stdout, "\n");
              }
              auto &&move_opt = get_move_opt(sfd);
              if (!move_opt) {
                error_print("opponent optional moveset recv");
                continue;
              }
              if (move_opt.value()) {
                fprintf(stdout, "also, here's the move send by the opponent: ");
                print_move(move_opt.value().value());
                std::cout << std::endl;
              }
            } else {
              auto &&move = get_move(sfd);
              if (!move) {
                error_print("opponent moveset recv");
                continue;
              }
              if (words[0] == "abort") { fprintf(stdout, "could not abort fast enough\n"); }
              else { fprintf(stdout, "could not quit fast enough\n"); }
              if (recv_msg == message::lost) {
                fprintf(stdout, "recieved move from the opponent, ");
                print_move(move.value());
                fprintf(stdout, "made you lose\n");
              } else if (recv_msg == message::draw) {
                fprintf(stdout, "recieved move from the opponent, ");
                print_move(move.value());
                fprintf(stdout, "caused a draw\n");
              } else if (recv_msg == message::move) {
                fprintf(stdout, "recieved a move from the opponent: ");
                print_move(move.value());
                std::cout << std::endl;
                if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("server confirmation recv"); }
                //always message::confirmation
                if (words[0] == "abort") { fprintf(stdout, "recieved confirmation to abort match\n"); }
                else { fprintf(stdout, "recieved confirmation to exit match\n"); }
              }
            }

            if (words[0] == "abort") {
              fprintf(stdout, "aborted the match\n");
              client_state = state::logged_in;
              continue;
            } else {
              fprintf(stdout, "exited\n");
              exit(EXIT_SUCCESS);
            }
          }
        }

        fprintf(stdout, "write your move here (example: move 70 43 (with optional promotion to knight - n, bishop - b, rook - r, queen - q)), abort or quit (opponent might forfeit in the meantime):\n");

        int nfds = epoll_wait(epollfd, events, 2, -1);
        if (nfds == -1) {
          error_print("in-game epoll wait");
          continue;
        } else if (nfds == 2 || (nfds == 1 && events[0].data.fd == sfd)) {
          if (nfds == 2) { fprintf(stdout, "your command was ignored due to recieving data from the server at the same time\n"); }
          if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("oppoennt forfeit recv"); continue; }
          //always message::forfeit
          fprintf(stdout, "opponent forfeited match\n");
          fprintf(stdout, "going back to the main menu...\n");
          client_state = state::logged_in;
          continue;
        } else {
          command_length = read(STDIN_FILENO, command, sizeof(command));
          if (command_length == -1) { error_print("command read"); exit(EXIT_FAILURE); }
          command[command_length] = '\0';
          auto &&words = get_words(command);

          //either got a move from the opponent or you're white and it's your time to move
          if (words[0] == "move") {
            bool has_promotion = words.size() == 4;
            message to_send = message::move;
            memcpy(sendbuf, &to_send, sizeof(message));
            memcpy(sendbuf + sizeof(message), text_to_move(words[1], words[2], has_promotion ? words[3] : std::optional<std::string_view>()).data(), 3);
            if (send(sfd, sendbuf, sizeof(message) + 3 * sizeof(uint8_t), 0) == -1) { error_print("move send"); continue; }

            if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("move confirmation/rejection/win/draw/forfeit recv"); continue; }
            std::cerr << "sent move: " << words[1] << " -> " << words[2] << " (" << (has_promotion ? words[3] : "") << ")" << std::endl;
            if (recv_msg == message::rejection) {
              fprintf(stdout, "invalid move, try again\n");
              continue;
            } else if (recv_msg == message::confirmation) {
              fprintf(stdout, "move is valid, waiting for the opponent's move...\n");
              first_move = false;
            } else {
              if (recv_msg == message::won) {
                fprintf(stdout, "you won\n");
                continue;
              } else if (recv_msg == message::draw) {
                fprintf(stdout, "move is valid, but you ended up in a draw\n");
              } else if (recv_msg == message::forfeit) {
                //could send move as opposing player forfeit is being processed, counts as you not even getting ur move checked, sorry
                fprintf(stdout, "opponent forfeited the match\n");
              }
              fprintf(stdout, "going back to the main menu...\n");
              client_state = state::logged_in;
              continue;
            }
          } else if (words[0] == "abort") {
            message to_send = message::abort_match;
            memcpy(sendbuf, &to_send, sizeof(message));
            if (send(sfd, sendbuf, sizeof(message), 0) == -1) { error_print("abort send"); continue; }

            //your forfeit couyld be sent as the oppoent's forfeit is being processed but it doesn't matter since you're reading a message anyway
            if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("server confirmation recv"); continue; }
            fprintf(stdout, "aborted match\ngoing back to the main menu...\n");
            client_state = state::logged_in;
            continue;
          } else if (words[0] == "quit") {
            message to_send = message::quit;
            memcpy(sendbuf, &to_send, sizeof(message));
            if (send(sfd, sendbuf, sizeof(message), 0) == -1) { error_print("quit send"); continue; }

            if (recv(sfd, &recv_msg, sizeof(message), 0) == -1) { error_print("server confirmation recv"); continue; }
            fprintf(stdout, "exited\n");
            exit(EXIT_SUCCESS);
          } else {
            fprintf(stdout, "invalid command\n");
            continue;
          }
        }
      } break;
    }
  }
  close(sfd);
}
