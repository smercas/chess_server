#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>

#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <iostream>
#include <queue>

#include "../../common/utils.h"
#include "user.h"
#include "big_poll.h"
#include "player_queue.h"

int get_bound_socket(const char *);

int main(void) {
  const char *port = "2048";
  big_poll::set_listening_socket(get_bound_socket(port));
  if (listen(big_poll::get_listening_socket(), 100) == -1) { error_print("listen"); exit(EXIT_FAILURE); }
  std::thread big_poll_thread(big_poll::poll_users);
  big_poll_thread.detach();
  std::thread player_queue_poll_thread(player_queue::poll_users);
  player_queue_poll_thread.detach();
  std::thread player_queue_actual_queue_thread(player_queue::queue_work);
  while (true) {}
}

int get_bound_socket(const char *port) {
  //setting up the hints field for getaddrinfo()
  struct addrinfo hints; {
    //node == NULL -> uses INADDR_LOOPBACK
    //AI_NUMERICSERV -> skips the service name resolution, needs a string of numbers
    hints.ai_flags = AI_PASSIVE | AI_V4MAPPED | AI_ALL | AI_ADDRCONFIG | AI_NUMERICSERV;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
  }
  //pointers for storage and traversal of the return from getaddrinfo()
  addrinfo *addrinfos, *elem;
  int gai_retval = getaddrinfo(NULL, port, &hints, &addrinfos);
  if (gai_retval != 0) {
    fprintf(stderr, "getaddrinfo");
    if (gai_retval == EAI_SYSTEM) {
      fprintf(stderr, " (%s): %s", strerrorname_np(errno), strerrordesc_np(errno));
    } else {
      fprintf(stderr, "%d: %s", gai_retval, gai_strerror(gai_retval));
    }
    exit(EXIT_FAILURE);
  }
  int sfd;
  for (elem = addrinfos; elem != NULL; elem = elem->ai_next) {
    sfd = socket(elem->ai_family, elem->ai_socktype, elem->ai_protocol);
    if (sfd == -1) {
      continue;
    }
    int reuse = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        error_print("Setsockopt failed");
        close(sfd);
        exit(EXIT_FAILURE);
    }
    if (bind(sfd, elem->ai_addr, elem->ai_addrlen) == 0) {
      break;
    }
    close(sfd);
  }
  freeaddrinfo(addrinfos);
  if (elem == NULL) {
    fprintf(stderr, "Could not bind.\n");
    exit(EXIT_FAILURE);
  }
  return sfd;
}
