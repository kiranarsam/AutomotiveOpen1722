#pragma once

/* System files */
#include <poll.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>

#include <string>
#include <thread>

class Ieee1722Transmitter
{
public:
  Ieee1722Transmitter();
  ~Ieee1722Transmitter();

  void init();

  void start();

  void stop();

private:

  void run();

  std::string m_ifname;

  std::string m_macaddr;

  std::string m_can_ifname;

  int m_eth_fd;

  int m_can_socket;

  bool m_is_initialized;

  bool m_is_can_enabled;

  std::thread m_transmitter_thread;

  bool m_running;

  struct pollfd m_poll_fds;

  struct sockaddr* m_dest_addr;

};