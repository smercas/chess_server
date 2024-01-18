#include "user.h"

#include "../../common/utils.h"

//

#include <iostream>
#include <fstream>
#include <vector>
#include <tuple>

std::mutex user::db_mutex;
std::unordered_map<int, user> user::active_users;

user::user(std::string_view username, size_t rank) : m_username(username), m_rank(rank) {}
std::string_view user::username() const {
  return m_username;
}
size_t user::rank() const {
  return m_rank;
}
void user::set_username(std::string_view new_username) {
  m_username = new_username;
}
void user::set_rank(size_t new_rank) {
  m_rank = new_rank;
}
size_t user::add_to_rank(size_t to_add) {
  if (m_rank > m_rank + to_add) {
    to_add = __SIZE_MAX__ - m_rank;
    m_rank = __SIZE_MAX__;
  } else {
    m_rank += to_add;
  }
  return to_add;
}
size_t user::remove_from_rank(size_t to_remove) {
  if (m_rank < to_remove) {
    to_remove = m_rank;
    m_rank = 0;
  } else {
    m_rank -= to_remove;
  }
  return to_remove;
}

bool user::createAccount(int fd, std::string_view username, std::string_view password) {
  const std::lock_guard lock(db_mutex);

  std::fstream usersFile;

  //first check if the username is taken
  {
    usersFile.open("users.txt");
    std::string user;
    std::string pass;
    size_t rank;
    if (usersFile.is_open() == false) {
      std::cerr << "unable to open file" << std::endl;
      return false;
    }
    while (usersFile >> user >> pass >> rank) {
      if (username == user) {
        std::cerr << "username is taken" << std::endl;
        return false;
      }
    }
    usersFile.close();
  }

  //then write the user in the file
  usersFile.open("users.txt", std::ios_base::app);
  if (usersFile.is_open() == false) {
    std::cerr << "unable to open file" << std::endl;
    return false;
  }
  usersFile << username << ' ' << password << ' ' << 1000 << std::endl;
  usersFile.close();

  //add to the active users
  active_users.emplace(fd, user(username, 1000));
  return true;
}
bool user::deleteAccount(int fd) {
  const std::lock_guard lock(db_mutex);
  std::vector<std::tuple<std::string, std::string, size_t>> users;
  //copy everything except the user to be removed
  {
    std::ifstream usersFile("users.txt");
    std::string user;
    std::string pass;
    size_t rank;
    if (usersFile.is_open() == false) {
      std::cerr << "Unable to open file!" << std::endl;
      return false;
    }
    while (usersFile >> user >> pass >> rank) {
      if (active_users.at(fd).m_username != user) {
        users.emplace_back(std::forward_as_tuple(user, pass, rank));
      }
    }
    usersFile.close();
  }

  //remove from the active users
  active_users.erase(fd);

  // whole file is rewritten
  std::ofstream usersFile("users.txt", std::ios_base::trunc);
  if (usersFile.is_open() == false) {
    std::cerr << "Unable to open file!" << std::endl;
    return false;
  }
  for (const auto& [user, pass, rank] : users) {
    usersFile << user << " " << pass << ' ' << rank << std::endl;
  }
  usersFile.close();
  return true;
}
bool user::getAcount(int fd, std::string_view username, std::string_view password) {
  const std::lock_guard lock(db_mutex);

  //checks the active users to see if the user is already logged in
  for (auto &&[_, user] : active_users) {
    if (user.username() == username) {
      std::cerr << "user is already logged in" << std::endl;
      return false;
    }
  }

  std::ifstream usersFile;
  usersFile.open("users.txt");
  std::string usr;
  std::string pass;
  size_t rank;
  if (usersFile.is_open() == false) {
    std::cerr << "unable to open file" << std::endl;
    return false;
  }
  while (usersFile >> usr >> pass >> rank) {
    if (username == usr) { 
      if (password == pass) {
        //add to the active users
        active_users.emplace(fd, user(username, rank));
        return true;
      } else {
        std::cerr << "incorrect password" << std::endl;
        return false;
      }
    }
  }
  usersFile.close();
  std::cerr << "username not found" << std::endl;
  return false;
}
void user::disconnectUser(int fd) {
  const std::lock_guard lock(db_mutex);
  active_users.erase(fd);
}
bool user::isActiveUser(int fd) {
  return active_users.contains(fd);
}
size_t user::get_rank_by_fd(int fd) {
  return active_users.at(fd).m_rank;
}
std::string_view user::get_username_by_fd(int fd) {
  return active_users.at(fd).m_username;
}
void user::recv_send_fail_handler(int fd, std::string_view message, int err) {
  error_print(message, err);
  disconnectUser(fd);
  if (err != EBADFD) {
    if (close(fd) == -1) { error_print("close"); }
  }
}
