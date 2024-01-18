#include "utils.h"
#include <stdio.h>
#include <string.h>
#include "unistd.h"
#include "fcntl.h"
#include <iostream>
#include <sys/socket.h>

coords::coords() : x(UINT8_MAX), y(UINT8_MAX) {}
coords::coords(uint8_t x, uint8_t y) : x(x), y(y) {}
bool coords::operator == (const coords &other) {
  return this->x == other.x && this->y == other.y;
}
bool coords::operator == (coords &&other) {
  return this->x == other.x && this->y == other.y;
}
coords coords::altered_with(uint8_t ax, uint8_t ay) {
  return { static_cast<uint8_t>(this->x + ax), static_cast<uint8_t>(this->y + ay) };
}
coords coords::altered_with(std::pair<uint8_t, uint8_t> a) {
  return { static_cast<uint8_t>(this->x + a.first), static_cast<uint8_t>(this->y + a.second) };
}
coords coords::altered_with(const coords &other) {
  return { static_cast<uint8_t>(this->x + other.x), static_cast<uint8_t>(this->y + other.y) };
}
std::array<uint8_t, 3> text_to_move(std::string_view src, std::string_view dest, std::optional<std::string_view> promotion) {
  std::array<uint8_t, 3> retval;
  retval[0] = src[0] - '0' + (src[1] - '0') * 8;
  retval[1] = dest[0] - '0' + (dest[1] - '0') * 8;
  if (promotion.has_value() == false) {
    retval[2] = static_cast<uint8_t>(promotion::none);
  } else {
    switch (promotion.value()[0]) {
      case 'n': retval[2] = static_cast<uint8_t>(promotion::knight); break;
      case 'b': retval[2] = static_cast<uint8_t>(promotion::bishop); break;
      case 'r': retval[2] = static_cast<uint8_t>(promotion::rook); break;
      case 'q': retval[2] = static_cast<uint8_t>(promotion::queen); break;
    }
  }
  return retval;
}
std::tuple<std::string, std::string, promotion> move_to_text(std::array<uint8_t, 3> move) {
  std::tuple<std::string, std::string, promotion> retval;
  auto &[src, dest, promo] = retval;
  src.reserve(2);
  src.push_back(move[0] % 8 + '0'); src.push_back(move[0] / 8 + '0');
  dest.reserve(2);
  dest.push_back(move[1] % 8 + '0'); dest.push_back(move[1] / 8 + '0');
  promo = static_cast<promotion>(move[2]);
  return retval;
}
std::tuple<coords, coords, promotion> destructured_move(std::array<uint8_t, 3> move) {
  std::tuple<coords, coords, promotion> retval;
  auto &[src, dest, promo] = retval;
  src.x = move[0] % 8; src.y = move[0] / 8;
  dest.x = move[1] % 8; dest.y = move[1] / 8;
  promo = static_cast<promotion>(move[2]);
  return retval;
}
void print_move(std::array<uint8_t, 3> move) {
  auto &&[source, destination, promotion] = move_to_text(move);
  std::cout << source << " -> " << destination << " (" << get_promotion_as_text(promotion) << ")";
}
void error_print(std::string_view text, int err) {
  std::cerr << text.data();
  fprintf(stderr, " (%s): %s\n", strerrorname_np(err), strerrordesc_np(err));
}
bool FlipSocketBlocking(int fd, bool blocking) {
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) { error_print("fcntl"); return false; }
   flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
}
sigset_t get_all_but_SIGINT_blocking_mask() {
  sigset_t retval;
  sigfillset(&retval);
  sigdelset(&retval, SIGINT);
  return retval;
}
std::string_view get_message_as_text(message m) {
  switch (m) {
    case message::abort_match: return "abort_match";
    case message::delete_account: return "delete_account";
    case message::login_data: return "login_data";
    case message::logout: return "logout";
    case message::play: return "play";
    case message::quit: return "quit";
    case message::signup_data: return "signup_data";
    case message::move: return "move";
    case message::black: return "black";
    case message::confirmation: return "confirmation";
    case message::draw: return "draw";
    case message::forfeit: return "forfeit";
    case message::lost: return "lost";
    case message::rejection: return "rejection";
    case message::white: return "white";
    case message::won: return "won";
  }
  return NULL;
}
std::string_view get_promotion_as_text(promotion p) {
  switch (p) {
    case promotion::none: return "";
    case promotion::knight: return "knight";
    case promotion::bishop: return "bishop";
    case promotion::rook: return "rook";
    case promotion::queen: return "queen";
    default: return "";
  }
}
