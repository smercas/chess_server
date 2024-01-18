#pragma once

#include "../../common/enums.h"

#include "sys/socket.h"

#include <optional>
#include <string>
#include <mutex>
#include <unordered_map>

class user {
public:
  user(std::string_view, size_t);

  std::string_view username() const;
  size_t rank() const;
  void set_username(std::string_view);
  void set_rank(size_t);
  size_t add_to_rank(size_t);
  size_t remove_from_rank(size_t);

  // we can make these use JSON later...

  //creates an account, will check if the account already exists
  static bool createAccount(int, std::string_view, std::string_view);
  //deletes an account that already exists because you can only delete an account when you're logged in
  static bool deleteAccount(int);
  //checks if an user with the respective username and password exist
  static bool getAcount(int, std::string_view, std::string_view);
  static void disconnectUser(int);
  static bool isActiveUser(int);
  static size_t get_rank_by_fd(int);
  static std::string_view get_username_by_fd(int);
  static void recv_send_fail_handler(int, std::string_view, int = errno);

private:
  static std::mutex db_mutex;
  static std::unordered_map<int, user> active_users;
  std::string m_username;
  size_t m_rank;
};
