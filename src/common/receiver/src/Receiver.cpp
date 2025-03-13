/**
 * Copyright (c) 2025, Kiran Kumar Arsam
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *   * this list of conditions and the following disclaimer in the documentation
 *   * and/or other materials provided with the distribution.
 *
 *   * Neither the name of the Kiran Kumar Arsam nor the names of its
 *   * contributors may be used to endorse or promote products derived from
 *   * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* System files */
#include <linux/if_ether.h>
#include <unistd.h>

extern "C" {
#include "ether_comm_if.h"
#include "can_comm_if.h"
}

#include <iostream>
#include <functional>

#include "Receiver.hpp"
#include "CommonUtils.hpp"
#include "AvtpUtil.hpp"


#define MAX_ETH_PDU_SIZE                1500
#define MAX_CAN_FRAMES_IN_ACF           15

Receiver::Receiver(std::string &ifname, std::string &macaddr)
  : m_ifname{ifname}, m_macaddr{macaddr}
{
  m_is_can_enabled = false;
  init();
}

Receiver::~Receiver()
{

}

void Receiver::init()
{
  if (!m_is_initialized) {
    int res = -1;
    uint8_t addr[ETH_ALEN];

    res = CommonUtils::getMacAddress(m_macaddr, addr);
    if(res < 0) {
      std::cout << "Invalid mac address" << std::endl;
      return;
    }

    m_eth_fd = Comm_Ether_CreateListenerSocket(m_ifname.c_str(), addr, ETH_P_TSN);
    if (m_eth_fd < 0) {
      std::cout << "Failue to create listener ethernet socket" << std::endl;
      return;
    }

    // Open a CAN socket for reading frames
    if(m_is_can_enabled) {
      m_can_writer.init(m_can_ifname, CAN_VARIANT_FD);
    }

    m_is_initialized = true;
    std::cout << "init initialized " << std::endl;
  }
}

void Receiver::setCallbackHandler(DataCallbackHandler &handler)
{
  m_callback_handler = handler;
}

void Receiver::start()
{
  if(m_is_initialized) {
    m_running = true;
    m_receiver_thread = std::thread{std::bind(&Receiver::run, this)};
  }
}

void Receiver::stop()
{
  m_running = false;
  if (std::this_thread::get_id() != m_receiver_thread.get_id()) {
    if (m_receiver_thread.joinable())
    {
      m_receiver_thread.join();
    }
  } else {
    m_receiver_thread.detach();
  }

  if(m_is_can_enabled) {
    m_can_writer.stop();
  }
}

void Receiver::run()
{
  int res = 0;
  uint16_t pdu_length = 0;
  uint8_t num_can_msgs = 0;
  uint8_t exp_cf_seqnum = 0;
  uint8_t pdu[MAX_ETH_PDU_SIZE];
  frame_t can_frames[MAX_CAN_FRAMES_IN_ACF];

  m_poll_fds.fd = m_eth_fd;
  m_poll_fds.events = POLLIN;

  while(m_running)
  {
    // add polling to avoid blocking calls
    res = poll(&m_poll_fds, 1, 500);
    if (res < 0) {
      std::cout <<"Failed poll() call" << std::endl;
      // destroy handler
      return;
    }

    // check for m_running
    if(!m_running) {
      // destroy handler
      std::cout <<"Exited thread " << std::endl;
      return;
    }

    if (res == 0) {
      // timeout expired
      continue;
    }

    if (m_poll_fds.revents & POLLIN) {

      pdu_length = recv(m_eth_fd, pdu, MAX_ETH_PDU_SIZE, 0);
      if (pdu_length < 0 || pdu_length > MAX_ETH_PDU_SIZE) {
          std::cout << "Failed to receive data" << std::endl;
          continue;
      }

      num_can_msgs = AvtpUtil::extractCanFramesFromAvtp(pdu, can_frames, &exp_cf_seqnum);

      exp_cf_seqnum++;

      m_callback_handler.handleCallback(2025);

      if(m_is_can_enabled) {
        m_can_writer.sendData(can_frames, num_can_msgs);
      }
    }
  }
}