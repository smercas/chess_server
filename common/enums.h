#pragma once

#include <cstdint>

enum class promotion : uint8_t {
  none,
  knight,
  bishop,
  rook,
  queen,
};

enum class color : bool {
  white,
  black,
};

// when in a gamen and it's not its turn, a player should expect:
//    - if getting the moveset from the opponent failed:
//        forfeit
//    - if sending the response to the opponent for its moveset failed:
//        lost + moveset | draw + moveset | forfeit + moveset | forfeit
//    - else:
//        lost + moveset | draw + moveset | move + moveset
// overall, check for: lost + moveset | draw + moveset | move + moveset | forfeit + moveset | forfeit
// when in a game and it's its turn, a player should expect a forfeit.
//
// when waiting for a message and a moveset, use recv with MSG_DONTWAIT for the moveset
// if the moveset is optional, EAGAIN or EWOULDBLOCK means no moveset was sent
// otherwise, any fail represents an error
enum class message : uint8_t {
  // message: 1 byte
  // starting position: 1 byte
  // finishing position: 1 byte
  // optional promotion: 1 byte
  //
  // either:
  // a valid move made by a player, sent to its opponent or
  // a move sent to the server to be validated
  // it should expect,
  //    - if server recieved messages from both clients at the same time:
  //        won | draw
  //        forfeit, if move did not cause the match to end
  //    - if server recieved this move while an abort / quit form the opponent was being processed:
  //        forfeit
  //    - if sending the move to the opponent fails:
  //        forfeit
  //    - else:
  //        won | draw | confirmation | rejection
  // overall, check for:
  //    won | draw | confirmation | rejection | forfeit
  move,
  // message: 1 byte
  //
  // user request to enter queue
  // message: 1 byte
  //
  // sent when a player wants to abort the search for a match or the match itself
  // when cancelling a search, it should expect a confirmation
  // while in a match, it should expect,
  //    - if server recieved messages from both clients at the same time:
  //        lost + moveset | draw + moveset | confirmation + moveset | confirmation
  //    - if server recieved this while an abort / quit from the opponent was being processed
  //        forfeit
  //    - if getting the moveset from the opponent failed:
  //        forfeit
  //    - if sending the response to the opponent for its moveset failed:
  //        lost + moveset | draw + moveset | forfeit + moveset | forfeit
  //    - else:
  //        lost + moveset | draw + moveset | move + moveset + confirmation | confirmation
  // overall, check for:
  //    lost + moveset | draw + moveset | move + moveset + confirmation | confirmation | confirmation + moveset | forfeit + moveset | forfeit
  abort_match,
  // message: 1 byte
  //
  // sent when a player quits
  // when cancelling a search, it should expect a confirmation
  // while in a match, it should expect,
  //    - if server recieved messages from both clients at the same time:
  //        lost + moveset | draw + moveset | confirmation + moveset | confirmation
  //    - if server recieved this while an abort / quit from the opponent was being processed
  //        forfeit
  //    - if getting the moveset from the opponent failed:
  //        forfeit
  //    - if sending the response to the opponent for its moveset failed:
  //        lost + moveset | draw + moveset | forfeit + moveset | forfeit
  //    - else:
  //        lost + moveset | draw + moveset | move + moveset + confirmation | confirmation
  // overall, check for:
  //    lost + moveset | draw + moveset | move + moveset + confirmation | confirmation | confirmation + moveset | forfeit + moveset | forfeit
  quit,
  // message: 1 byte
  //
  // basically what the server sends when:
  //    signup\login data is valid and the signup process was successful
  //        (signup could fail if the username is taken)
  //        (login  could fail if the username was not found or if the password is not correct)
  //    sent move is valid
  //    acknowledgement for abort/quit
  confirmation,
  // message: 1 byte
  //
  // sent by the server when:
  //    signup/login failed:
  //        (signup could fail if the username is taken)
  //        (login  could fail if the username was not found or if the password is not correct)
  //    sent move is invalid
  rejection,
  // message: 1 byte
  //
  // sent to the client that won
  won,
  // message: 1 byte
  //
  // sent to the client that lost
  // will always be sent along with a move
  lost,
  // message: 1 byte
  //
  // sent to both clients when they draw
  // can be sent along with a move if it's caused by the opponent
  draw,
  // message: 1 byte
  //
  // sent from the server to a in game client to tell him his opponent ditched him
  // can be sent to a forfeiter if the other player forfeits faster
  forfeit,
  // message: 1 byte
  //
  // sent when a logged user wants to delete its account, can fail
  // sxpects a confirmation or a rejection
  delete_account,
  // message: 1 byte
  //
  // sent when a player logs out
  // expects a confirmation
  logout,
  // message: 1 byte
  //
  // starts looking for a match
  // does not expect anything immediately, but it can recieve white or black from the server
  play,
  // structure:
  // message: 1 byte
  // username length: 1 byte
  // password length: 1 byte
  // username: at most 256 bytes
  // password: at most 256 bytes
  //
  // signup data
  // sxpects a confirmation or a rejection
  signup_data,
  // message: 1 byte
  // username length: 1 bytes
  // password length: 1 byte
  // username: at most 256 bytes
  // password: at most 256 bytes
  //
  // login data
  // sxpects a confirmation or a rejection
  login_data,
  // message: 1 byte
  //
  // sent when the match starts, own color is white
  white,
  // message: 1 byte
  //
  // sent when the match starts, own color is black
  black,
};

