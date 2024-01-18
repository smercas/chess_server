#pragma once

#include "../../common/enums.h"
#include "../../common/utils.h"

#include <array>
#include <vector>
#include <optional>
#include <variant>

enum piece_type {
  rook,
  knight,
  bishop,
  king,
  queen,
  pawn,
};

enum move_type_enum {
  move,
  take,
  castle,
  en_passant,
  pawn_two_step,
  promote,
  take_and_promote,
};

struct move_type {
  move_type_enum type;
  std::optional<std::variant<coords, uint8_t>> influence;
  //en passant and castle
  move_type(move_type_enum type, coords influenced_piece) : type(type), influence(influenced_piece) {}
  //for two step pawn moves
  move_type(uint8_t influenced_collumn) : type(pawn_two_step), influence(influenced_collumn) {}
  //anything else
  move_type(move_type_enum type) : type(type), influence({}) {}
};

static const std::array<coords, 8> knight_permutations = { coords(-2,  1), coords(-1,  2), coords( 1,  2), coords( 2,  1), coords( 2, -1), coords( 1, -2), coords(-1, -2), coords(-2, -1) };
static const std::array<coords, 4> rook_permutations   = { coords(-1,  0), coords( 0,  1), coords( 1,  0), coords( 0, -1) };
static const std::array<coords, 4> bishop_permutations = { coords(-1,  1), coords( 1,  1), coords( 1, -1), coords(-1, -1) };
static const std::array<coords, 8> queen_permutations  = { coords(-1,  0), coords(-1,  1), coords( 0,  1), coords( 1,  1), coords( 1,  0), coords( 1, -1), coords( 0, -1), coords(-1, -1) };

struct piece {
  piece_type type;
  color colour;
};

class board {
public:
  board();
  message check_move(coords, coords, promotion);
  color turn();
private:
  std::vector<std::pair<coords, move_type>> get_potential_moves_for(coords) const;
  static void add_move(const std::array<std::array<std::optional<piece>, 8>, 8> &, std::vector<std::pair<coords, move_type>> &, coords, move_type);
  static void add_move_with_takes(const std::array<std::array<std::optional<piece>, 8>, 8> &, std::vector<std::pair<coords, move_type>> &, coords, color);
  static std::array<std::array<std::optional<piece>, 8>, 8> get_move_demo(coords, move_type);
  static bool is_in_bounds(coords);
  bool no_piece_in(coords) const;
  static void add_move_with_takes_by_step(const std::array<std::array<std::optional<piece>, 8>, 8> &, std::vector<std::pair<coords, move_type>> &, coords, coords, color);
  bool king_would_be_in_check(coords, coords, promotion) const;

  std::array<std::array<std::optional<piece>, 8>, 8> m_tiles;
  color m_turn;
  std::optional<uint8_t> m_en_passant_colllumn;
  //what can_castle means:
  //king has not moved
  //the rook it's trying to castle with hasn't moved
  //the king is not in check, will not get in check after castling and the skipped swuare is not checked
  std::array<std::array<bool, 2>, 2> m_can_castle;
  std::array<coords, 2> m_king_coords;
};
