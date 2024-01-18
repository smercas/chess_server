#include "board.h"

piece wp(piece_type p) {
  return { p, color::white };
}
piece bp(piece_type p) {
  return { p, color::black };
}
std::optional<piece> none() {
  return {};
}
board::board() : m_tiles( { std::array<std::optional<piece>, 8>( {  wp(rook) , wp(knight), wp(bishop), wp(queen) ,  wp(king) , wp(bishop), wp(knight),  wp(rook)  } ),
                            std::array<std::optional<piece>, 8>( {  wp(pawn) ,  wp(pawn) ,  wp(pawn) ,  wp(pawn) ,  wp(pawn) ,  wp(pawn) ,  wp(pawn) ,  wp(pawn)  } ),
                            std::array<std::optional<piece>, 8>( {   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()   } ),
                            std::array<std::optional<piece>, 8>( {   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()   } ),
                            std::array<std::optional<piece>, 8>( {   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()   } ),
                            std::array<std::optional<piece>, 8>( {   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()  ,   none()   } ),
                            std::array<std::optional<piece>, 8>( {  bp(pawn) ,  bp(pawn) ,  bp(pawn) ,  bp(pawn) ,  bp(pawn) ,  bp(pawn) ,  bp(pawn) ,  bp(pawn)  } ),
                            std::array<std::optional<piece>, 8>( {  bp(rook) , bp(knight), bp(bishop), bp(queen) ,  bp(king) , bp(bishop), bp(knight),  bp(rook)  } ), } ),
                            m_turn(color::white), m_en_passant_colllumn({}), m_can_castle( {  std::array<bool, 2>( { true, true } ), std::array<bool, 2>( { true, true } ) } ), m_king_coords( { coords(0, 4), coords(7, 4) } ) {}
message board::check_move(coords src, coords dest, promotion p) {
  return message::confirmation;
}
color board::turn() {
  return m_turn;
}
std::vector<std::pair<coords, move_type>> board::get_potential_moves_for(coords source) const {
  std::vector<std::pair<coords, move_type>> moves;
  const std::optional<piece> &sp_opt = m_tiles[source.x][source.y];
  //source should have a piece and that piece should be of the turning player's colour
  if (!sp_opt || (sp_opt && sp_opt.value().colour != m_turn)) {
    return moves;
  }
  //src and dest should be within bounds (0..8 or 0..=7)
  if (!is_in_bounds(source)) {
    return moves;
  }
  const piece &sp = sp_opt.value();

  switch (sp.type) {
    case piece_type::pawn: {
      bool is_in_starting_row;
      bool is_in_en_passant_row;
      bool is_in_second_to_last_row;
      bool is_in_last_row;
      coords one_step_forward;
      coords two_steps_forward;
      std::array<coords, 2> takes;
      uint8_t takes_size = 0;
      switch (sp.colour) {
        case color::white: {
          is_in_starting_row = source.x == 1;
          is_in_en_passant_row = source.x == 4;
          is_in_second_to_last_row = source.x == 6;
          is_in_last_row = source.x == 7;
          one_step_forward = source.altered_with(1, 0);
          two_steps_forward = source.altered_with(2, 0);
          for (coords take : { source.altered_with(1, 1), source.altered_with(1, -1) }) {
            if (is_in_bounds(take)) {
              takes[takes_size] = take;
              takes_size += 1;
            }
          }
        } break;
        case color::black: {
          is_in_starting_row = source.x == 6;
          is_in_en_passant_row = source.x == 3;
          is_in_second_to_last_row = source.x == 1;
          is_in_last_row = source.x == 0;
          one_step_forward = source.altered_with(-1, 0);
          two_steps_forward = source.altered_with(-2, 0);
          for (coords take : { source.altered_with(-1, 1), source.altered_with(-1, -1) }) {
            if (is_in_bounds(take)) {
              takes[takes_size] = take;
              takes_size += 1;
            }
          }
        } break;
      }
      // add move 1 tile and move 2 tiles form the starting position of a pawn
      if (is_in_starting_row && no_piece_in(one_step_forward)) {
        add_move(m_tiles, moves, one_step_forward, move_type(move));
        if (no_piece_in(two_steps_forward)) { add_move(m_tiles, moves, two_steps_forward, move_type(source.y)); }
      }
      // add move 1 tile to promotion
      if (is_in_second_to_last_row && no_piece_in(one_step_forward)) {
        add_move(m_tiles, moves, one_step_forward, move_type(promote));
      }
      // add move 1 tile
      if (!is_in_starting_row && !is_in_second_to_last_row && !is_in_last_row && no_piece_in(one_step_forward)) {
        add_move(m_tiles, moves, one_step_forward, move_type(move));
      }
      // add en passant
      if (is_in_en_passant_row && m_en_passant_colllumn) {
        std::optional<coords> en_passant_coords = {};
        coords captured_piece;
        if (m_en_passant_colllumn.value() == source.y - 1) {
          en_passant_coords = one_step_forward.altered_with(0, -1);
          captured_piece = source.altered_with(0, -1);
        } else if (m_en_passant_colllumn.value() == source.y + 1) {
          en_passant_coords = one_step_forward.altered_with(0, 1);
          captured_piece = source.altered_with(0, 1);
        }
        if (en_passant_coords) {
          add_move(m_tiles, moves, en_passant_coords.value(), move_type(en_passant, captured_piece));
        }
      }
      // add take
      for (uint8_t i = 0; i < takes_size; i += 1) {
        const std::optional<piece> &to_take = m_tiles[takes[i].x][takes[i].y];
        if (to_take && to_take.value().colour != sp.colour) {
          if (is_in_second_to_last_row) {
            add_move(m_tiles, moves, takes[i], move_type(take_and_promote));
          }
          add_move(m_tiles, moves, takes[i], move_type(take));
        }
      }
    } break;
    case piece_type::rook: {
      for (coords p : rook_permutations) {
        add_move_with_takes_by_step(m_tiles, moves, source, p, sp.colour);
      }
    } break;
    case piece_type::knight: {
      for (coords p : knight_permutations) {
        coords pos = source.altered_with(p);
        if (is_in_bounds(pos)) { add_move_with_takes(m_tiles, moves, pos, sp.colour); }
      }
    } break;
    case piece_type::bishop: {
      for (coords p : bishop_permutations) {
        add_move_with_takes_by_step(m_tiles, moves, source, p, sp.colour);
      }
    } break;
    case piece_type::king: {
      for (coords p : queen_permutations) {
        coords pos = source.altered_with(p);
        if (is_in_bounds(pos)) { add_move_with_takes(m_tiles, moves, pos, sp.colour); }
      }
      if (m_can_castle[static_cast<bool>(m_turn)][0]) {
        //
      }
      if (m_can_castle[static_cast<bool>(m_turn)][1]) {
        //
      }
    } break;
    case piece_type::queen: {
      for (coords p : queen_permutations) {
        add_move_with_takes_by_step(m_tiles, moves, source, p, sp.colour);
      }
    } break;
  }
  return moves;
}
void board::add_move(const std::array<std::array<std::optional<piece>, 8>, 8> &tiles, std::vector<std::pair<coords, move_type>> &potential_moves, coords coords, move_type move_type) {
  
}
void board::add_move_with_takes(const std::array<std::array<std::optional<piece>, 8>, 8> &tiles, std::vector<std::pair<coords, move_type>> &potential_moves, coords coords, color colour) {
  if (tiles[coords.x][coords.y]) {
    if (tiles[coords.x][coords.y].value().colour != colour) {
      add_move(tiles, potential_moves, coords, take);
    }
    return;
  }
  add_move(tiles, potential_moves, coords, move);
}
bool board::is_in_bounds(coords c) {
  return c.x < 8 && c.y < 8;
}
bool board::no_piece_in(coords c) const {
  return !m_tiles[c.x][c.y];
}
void board::add_move_with_takes_by_step(const std::array<std::array<std::optional<piece>, 8>, 8> &tiles, std::vector<std::pair<coords, move_type>> &potential_moves, coords c, coords step, color colour) {
  while (is_in_bounds(c)) {
    if (tiles[c.x][c.y]) {
      if (tiles[c.x][c.y].value().colour != colour) {
        add_move(tiles, potential_moves, c, take);
      }
      return;
    }
    add_move(tiles, potential_moves, c, move);
    c = c.altered_with(step);
  }
}
bool board::king_would_be_in_check(coords src, coords dest, promotion p) const {
  return false;
}
