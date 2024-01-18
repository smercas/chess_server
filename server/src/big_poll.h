#pragma once

#include <sys/epoll.h>

#include <vector>
#include <mutex>

class big_poll {
public:
  static void add_socket(int);
  static int get_listening_socket();
  static void set_listening_socket(int);
  static void poll_users();
private:
  big_poll() = delete;
  big_poll(const big_poll &) = delete;
  big_poll(big_poll &&) = delete;
  big_poll &operator = (const big_poll &) = delete;
  big_poll &operator = (big_poll &&) = delete;
  ~big_poll() = delete;

  static void read_message(size_t);
  static void remove_disconnected_socket(size_t);
  static void remove_socket(size_t);
  static void recv_send_fail_handler(size_t, std::string_view);

  static int listening_socket;
  static int epoll_fd;
  static std::vector<epoll_event> events;
  static size_t events_capacity;
  static size_t events_size;
  static std::mutex mutex;
};
