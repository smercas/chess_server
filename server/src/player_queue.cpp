#include "player_queue.h"

#include "../../common/utils.h"
#include "big_poll.h"
#include "game.h"
#include <iostream>

//

//

int player_queue::epoll_fd;
std::vector<epoll_event> player_queue::events;
size_t player_queue::events_capacity = 100;
size_t player_queue::events_size = 0;
std::list<int> player_queue::queue;
std::condition_variable player_queue::cond_var;
std::mutex player_queue::mutex;

void player_queue::remove_sockets(size_t nfds) {
  for (size_t i = 0; i < (size_t)nfds; i += 1) {
    remove_socket(events[i].data.fd);

    fprintf(stderr, "removed socket %lu %d\n", i, events[i].data.fd);
  }
}
void player_queue::remove_socket(int fd) {
  queue.remove(fd);

  if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) { error_print("epoll_ctl player_queue remove client"); }

  events_size -= 1;
}
void player_queue::disconnect_socket(int fd) {
  user::disconnectUser(fd);

  if (close(fd) == -1) { error_print("player_queue close client"); }
}
void player_queue::add_socket(int to_add) {
  const std::lock_guard lock(mutex);

  decltype(queue)::iterator it = queue.begin();
  while (it != queue.end() && user::get_rank_by_fd(*it) < user::get_rank_by_fd(to_add)) { ++it; }
  queue.insert(it, to_add);

  epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = to_add;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, to_add, &ev) == -1) { error_print("epoll_ctl player_queue add client"); }

  events_size += 1;
  if (events_size >= events_capacity) {
    events_capacity *= 2;
    events.reserve(events_capacity);
  }
  fprintf(stderr, "added new socket: %d\n", to_add);

  cond_var.notify_one();
}
void player_queue::poll_users() {
  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    error_print("player_queue epoll_create");
    return;
  }

  events.reserve(events_capacity);

  while (true) {
    std::unique_lock lock(mutex);
    // i DESPISE THE WAITS HERE but I have no idea how else to avoid deadlocks here
    // this, as far as I know, is a fool-proof way of avoiding deadlocks
    if (events_size == 0) {
      lock.unlock();
      usleep(500000);
      continue;
    }
    int nfds;
    while ((nfds = epoll_wait(epoll_fd, events.data(), events_size, 500)) == -1 && errno == EINTR) {}
    if (nfds == -1) {
      error_print("player_queue epoll_wait");
      continue;
    }
    if (nfds == 0) {
      lock.unlock();
      cond_var.notify_one();
      usleep(500000);
      continue;
    }
    fprintf(stderr, "no longer waiting, found %d readable sockets ready to remove\n", nfds);
    remove_sockets((size_t)nfds);
    lock.unlock();
    for (size_t i = 0; i < (size_t)nfds; i += 1) {
      if (events[i].events & (EPOLLPRI | EPOLLERR | EPOLLRDHUP | EPOLLHUP)) {
        fprintf(stderr, "error on socket %lu %d, will disconnect the user and delete the file descriptor\n", i, events[i].data.fd);
        disconnect_socket(events[i].data.fd);
      } else if (events[i].events & EPOLLIN) {
        fprintf(stderr, "reading the message from %lu %d\n", i, events[i].data.fd);
        read_message(i);
      }
    }
  }
}
void player_queue::queue_work() {
  while (true) {
    std::unique_lock lock(mutex);
    //cond var woken up by adding an element or periodically by the poll if it's not processing anything
    //I suspect that different queuing algorithms will have to use busy waits and/or usleep calls, may god help me then
    cond_var.wait(lock, [] { return queue.size() >= 2; } );
    int fd1 = queue.front();
    remove_socket(queue.front());
    int fd2 = queue.front();
    remove_socket(queue.front());
    lock.unlock();

    message m1, m2;

    ssize_t recv_ret1 = recv(fd1, &m1, sizeof(message), 0);
    int errno1 = errno;
    ssize_t recv_ret2 = recv(fd2, &m2, sizeof(message), 0);
    int errno2 = errno;

    //nothing to read on both but they're still connected
    if (recv_ret1 == -1 && (errno1 == EWOULDBLOCK || errno1 == EAGAIN) && recv_ret2 == -1 && (errno2 == EWOULDBLOCK || errno2 == EAGAIN)) {
      fprintf(stderr, "pairing up %d with %d\n", fd1, fd2);
      std::thread new_game (game::start_game, fd1, fd2);
      new_game.detach();
    } else {
      //can I rewrite this? sure
      //will I? no
      if (recv_ret1 == -1 && (errno1 == EWOULDBLOCK || errno1 == EAGAIN)) {
        fprintf(stderr, "pushed socket %d back into the queue bcs partner %d dropped\n", fd1, fd2);
        add_socket(fd1);
      } else if (recv_ret1 == -1 && !(errno1 == EWOULDBLOCK || errno1 == EAGAIN)) {
        user::recv_send_fail_handler(fd1, "player_queue first client message recv", errno1);
      } else if (recv_ret1 == 0) {
        disconnect_socket(fd1);
      } else {
        fprintf(stderr, "processing the message from %d\n", fd1);
        process_message(fd1, m1);
      }
      if (recv_ret2 == -1 && (errno2 == EWOULDBLOCK || errno2 == EAGAIN)) {
        fprintf(stderr, "pushed socket %d back into the queue bcs partner %d dropped\n", fd2, fd1);
        add_socket(fd2);
      } else if (recv_ret2 == -1 && !(errno2 == EWOULDBLOCK || errno2 == EAGAIN)) {
        user::recv_send_fail_handler(fd1, "player_queue second client message recv", errno2);
      } else if (recv_ret2 == 0) {
        disconnect_socket(fd2);
      } else {
        fprintf(stderr, "processing the message from %d\n", fd2);
        process_message(fd2, m2);
      }
    }
  }
}
void player_queue::read_message(size_t idx_to_read) {
  message m;
  ssize_t recv_retval = recv(events[idx_to_read].data.fd, &m, sizeof(message), 0);
  if (recv_retval == -1 || recv_retval == 0) {
    if (recv_retval == -1) { user::recv_send_fail_handler(events[idx_to_read].data.fd, "player_queue message recv"); }
    else { disconnect_socket(events[idx_to_read].data.fd); }
    return;
  }
  process_message(events[idx_to_read].data.fd, m);
}
void player_queue::process_message(int fd, message m) {
  if (m == message::abort_match) {
    big_poll::add_socket(fd);

    message to_send = message::confirmation;
    ssize_t send_retval = send(fd, &to_send, sizeof(message), 0);
    if (send_retval == -1 || send_retval == 0) {
      if (send_retval == -1) { user::recv_send_fail_handler(fd, "player_queue abort_search confirmation send"); }
      else { disconnect_socket(fd); }
      return;
    }
    fprintf(stderr, "aborted match search for socket %d\n", fd);
  } else {
    //if recieved_message is not valid, it means that the client is compromised and should be removed
    std::cerr << "(command: " << get_message_as_text(m) << ") ";
    fprintf(stderr, "disconnecting socket %d\n", fd);
    if (m == message::quit) {
      message to_send = message::confirmation;
      ssize_t send_retval = send(fd, &to_send, sizeof(message), 0);
    if (send_retval == -1 || send_retval == 0) {
        if (send_retval == -1) { user::recv_send_fail_handler(fd, "player_queue quit confirmation send"); }
        else { disconnect_socket(fd); }
        return;
      }
      fprintf(stderr, "exited socket %d\n", fd);
    }
    disconnect_socket(fd);
  }
}
