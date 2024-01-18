#pragma once

#include <signal.h>
#include "enums.h"
#include <stdint.h>
#include <array>
#include <tuple>
#include <string>
#include <optional>
#include <initializer_list>
#include "errno.h"

struct coords {
  uint8_t x;
  uint8_t y;
  coords();
  coords(uint8_t, uint8_t);
  bool operator == (const coords &);
  bool operator == (coords &&);
  coords altered_with(uint8_t, uint8_t);
  coords altered_with(std::pair<uint8_t, uint8_t>);
  coords altered_with(const coords &);
};

std::array<uint8_t, 3> text_to_move(std::string_view, std::string_view, std::optional<std::string_view>);
std::tuple<std::string, std::string, promotion> move_to_text(std::array<uint8_t, 3>);
std::tuple<coords, coords, promotion> destructured_move(std::array<uint8_t, 3>);
void print_move(std::array<uint8_t, 3>);
void error_print(std::string_view, int = errno);
sigset_t get_all_but_SIGINT_blocking_mask();
bool FlipSocketBlocking(int, bool);
std::string_view get_message_as_text(message);
std::string_view get_promotion_as_text(promotion);
