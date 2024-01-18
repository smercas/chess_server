#pragma once

#include "user.h"

#include "sys/epoll.h"

#include <queue>
#include <condition_variable>
#include <list>
#include <vector>

class player_queue {
public:
  static void add_socket(int);

  static void poll_users();
  static void queue_work();
private:
  player_queue() = delete;
  player_queue(const player_queue &) = delete;
  player_queue(player_queue &&) = delete;
  player_queue &operator = (const player_queue &) = delete;
  player_queue &operator = (player_queue &&) = delete;
  ~player_queue() = delete;

  static void remove_sockets(size_t);
  static void remove_socket(int);
  static void disconnect_socket(int);

  static void read_message(size_t);
  static void process_message(int, message);

  static int epoll_fd;
  static std::vector<epoll_event> events;
  static size_t events_capacity;
  static size_t events_size;
  static std::list<int> queue;
  static std::condition_variable cond_var;
  static std::mutex mutex;
};
