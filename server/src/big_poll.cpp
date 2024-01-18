#include "big_poll.h"

#include "../../common/utils.h"
#include "../../common/enums.h"
#include "user.h"
#include "player_queue.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <thread>
#include <optional>
#include <iostream>

int big_poll::listening_socket;
int big_poll::epoll_fd;
std::vector<epoll_event> big_poll::events;
size_t big_poll::events_capacity = 100;
size_t big_poll::events_size = 1;
std::mutex big_poll::mutex;

void big_poll::remove_disconnected_socket(size_t idx_to_remove) {
  const std::lock_guard lock(mutex);

  user::disconnectUser(events[idx_to_remove].data.fd);

  if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[idx_to_remove].data.fd, NULL) == -1) { error_print("big_poll epoll_ctl remove disconnected client"); }

  events_size -= 1;

  if (close(events[idx_to_remove].data.fd) == -1) { error_print("big_poll close disconnected client"); }

  fprintf(stderr, "disconnected socket %lu %d\n", idx_to_remove, events[idx_to_remove].data.fd);
}
void big_poll::remove_socket(size_t idx_to_remove) {
  const std::lock_guard lock(mutex);

  if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[idx_to_remove].data.fd, NULL) == -1) { error_print("epoll_ctl big_poll remove disconnected client"); }

  events_size -= 1;

  fprintf(stderr, "removed socket %lu %d\n", idx_to_remove, events[idx_to_remove].data.fd);
}
void big_poll::recv_send_fail_handler(size_t idx_to_remove, std::string_view message) {
  const std::lock_guard lock(mutex);

  int err = errno;
  error_print(message);

  user::disconnectUser(events[idx_to_remove].data.fd);

  if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[idx_to_remove].data.fd, NULL) == -1) { error_print("big_poll epoll_ctl remove disconnected client"); }

  events_size -= 1;

  if (err != EBADFD) {
    if (close(events[idx_to_remove].data.fd) == -1) { error_print("big_poll close disconnected client"); }
  }

  fprintf(stderr, "disconnected socket %lu %d\n", idx_to_remove, events[idx_to_remove].data.fd);
}
void big_poll::add_socket(int to_add) {
  const std::lock_guard lock(mutex);

  epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = to_add;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, to_add, &ev) == -1) { error_print("epoll_ctl big_poll add client"); }

  events_size += 1;
  if (events_size >= events_capacity) {
    events_capacity *= 2;
    events.reserve(events_capacity);
  }
  fprintf(stderr, "added new socket: %d\n", to_add);
}
int big_poll::get_listening_socket() {
  return listening_socket;
}
void big_poll::set_listening_socket(int new_listening_socket) {
  listening_socket = new_listening_socket;
}
void big_poll::poll_users() {
  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    error_print("big_poll epoll_create");
    return;
  }

  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = listening_socket;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listening_socket, &ev) == -1) {
    error_print("big_poll epoll_ctl add server");
    return;
  }

  events.reserve(events_capacity);

  while (true) {
    int nfds;
    {
      std::lock_guard lock(mutex);
      while ((nfds = epoll_wait(epoll_fd, events.data(), events_size, 500)) == -1 && errno == EINTR) {}
    }
    if (nfds == -1) {
      error_print("big_poll epoll_wait");
      continue;
    }
    if (nfds == 0) {
      //sleep here otherwise u can't add sockets from outside this thread (yea ik, kinda sucky)
      usleep(500000);
      continue;
    }
    fprintf(stderr, "no longer waiting, found %d readable sockets\n", nfds);
    //big poll has automatic unique ownership over the file descriptors returned by epoll_wait
    //other threads can only add file descriptors to the poll, only big_poll can remove them
    //no need for a lock here, me thinks
    for (size_t i = 0; i < (size_t)nfds; i += 1) {
      if (events[i].data.fd == listening_socket) {
        int conn_socket = accept(listening_socket, NULL, NULL);
        if (conn_socket == -1) {
          error_print("big_poll accept");
          continue;
        }
        FlipSocketBlocking(conn_socket, false);
        add_socket(conn_socket);
      } else {
        if (events[i].events & (EPOLLPRI | EPOLLERR | EPOLLRDHUP | EPOLLHUP)) {
          remove_disconnected_socket(i);
        } else if (events[i].events & EPOLLIN) {
          read_message(i);
        }
      }
    }
  }
}
void big_poll::read_message(size_t idx_to_read) {
  bool logged_in = user::isActiveUser(events[idx_to_read].data.fd);

  message m, to_send;

  ssize_t recv_retval = recv(events[idx_to_read].data.fd, &m, sizeof(message), 0);
  if (recv_retval == -1 || recv_retval == 0) {
    if (recv_retval == -1) { recv_send_fail_handler(idx_to_read, "big_poll message recv"); }
    else { remove_disconnected_socket(idx_to_read); }
    return;
  }

  if (logged_in == false && (m == message::login_data || m == message::signup_data)) {
    uint8_t username_length;
    uint8_t password_length;
    char username[256];
    char password[256];
    recv_retval = recv(events[idx_to_read].data.fd, &username_length, sizeof(username_length), 0);
    if (recv_retval == -1 || recv_retval == 0) {
      if (recv_retval == -1) { recv_send_fail_handler(idx_to_read, "big_poll username length recv"); }
      else { remove_disconnected_socket(idx_to_read); }
      return;
    }
    recv_retval = recv(events[idx_to_read].data.fd, &password_length, sizeof(password_length), 0);
    if (recv_retval == -1 || recv_retval == 0) {
      if (recv_retval == -1) { recv_send_fail_handler(idx_to_read, "big_poll password length recv"); }
      else { remove_disconnected_socket(idx_to_read); }
      return;
    }
    recv_retval = recv(events[idx_to_read].data.fd, username, username_length, 0);
    if (recv_retval == -1 || recv_retval == 0) {
      if (recv_retval == -1) { recv_send_fail_handler(idx_to_read, "big_poll username recv"); }
      else { remove_disconnected_socket(idx_to_read); }
      return;
    }
    recv_retval = recv(events[idx_to_read].data.fd, password, password_length, 0);
    if (recv_retval == -1 || recv_retval == 0) {
      if (recv_retval == -1) { recv_send_fail_handler(idx_to_read, "big_poll password recv"); }
      else { remove_disconnected_socket(idx_to_read); }
      return;
    }

    bool result;
    if (m == message::login_data) {
      result = user::getAcount(events[idx_to_read].data.fd, std::string_view(username, username_length), std::string_view(password, password_length));
    } else {
      result = user::createAccount(events[idx_to_read].data.fd, std::string_view(username, username_length), std::string_view(password, password_length));
    }

    if (result == false) {
      to_send = message::rejection;
    } else {
      to_send = message::confirmation;
    }
    
    ssize_t send_retval = send(events[idx_to_read].data.fd, &to_send, sizeof(message), 0);
    if (send_retval == -1 || send_retval == 0) {
      if (send_retval == -1) { recv_send_fail_handler(idx_to_read, "big_poll respoonse send"); }
      else { remove_disconnected_socket(idx_to_read); }
      return;
    }

    fprintf(stderr, "login/registration %s for socket %lu %d\n", result ? "successful" : "failed", idx_to_read, events[idx_to_read].data.fd);
  } else if (logged_in == true && (m == message::play || m == message::logout || m == message::delete_account)) {
    if (m == message::play) {
      remove_socket(idx_to_read);

      player_queue::add_socket(events[idx_to_read].data.fd);
    } else if (m == message::logout) {
      user::disconnectUser(events[idx_to_read].data.fd);

      to_send = message::confirmation;
      ssize_t send_retval = send(events[idx_to_read].data.fd, &to_send, sizeof(message), 0);
      if (send_retval == -1 || send_retval == 0) {
        if (send_retval == -1) { recv_send_fail_handler(idx_to_read, "big_poll logged_in logout send"); }
        else { remove_disconnected_socket(idx_to_read); }
        return;
      }
      fprintf(stderr, "logged out socket %lu %d\n", idx_to_read, events[idx_to_read].data.fd);
    } else if (m == message::delete_account) {
      bool result = user::deleteAccount(events[idx_to_read].data.fd);
      if (result == false) {
        to_send = message::rejection;
      } else {
        to_send = message::confirmation;
      }

      ssize_t send_retval = send(events[idx_to_read].data.fd, &to_send, sizeof(message), 0);
      if (send_retval == -1 || send_retval == 0) {
        if (send_retval == -1) { recv_send_fail_handler(idx_to_read, "big_poll logged_in account deletion response send"); }
        else { remove_disconnected_socket(idx_to_read); }
        return;
      }

      fprintf(stderr, "account deletion %s for socket %lu %d\n", result ? "successful" : "failed", idx_to_read, events[idx_to_read].data.fd);
    }
  } else {
    //if recieved_message is not valid, it means that the client is compromised and should be removed
    std::cerr << "(command: " << get_message_as_text(m) << ") ";
    fprintf(stderr, "disconnecting socket %lu %d\n", idx_to_read, events[idx_to_read].data.fd);
    if (m == message::quit) {
      to_send = message::confirmation;
      ssize_t send_retval = send(events[idx_to_read].data.fd, &to_send, sizeof(message), 0);
      if (send_retval == -1 || send_retval == 0) {
        if (recv_retval == -1) { recv_send_fail_handler(idx_to_read, "big_poll quit confirmation send"); }
        else { remove_disconnected_socket(idx_to_read); }
        return;
      }
      fprintf(stderr, "exited socket %lu %d\n", idx_to_read, events[idx_to_read].data.fd);
    }
    remove_disconnected_socket(idx_to_read);
  }
}
