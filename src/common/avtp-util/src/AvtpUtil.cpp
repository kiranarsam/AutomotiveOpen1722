/*
 * Copyright (c) 2025, Kiran Kumar Arsam,
 * Copyright (c) 2024, COVESA,
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of COVESA, Kiran Kumar Arsam nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * Modified the below usage to use in C++ applications
 */
#include "AvtpUtil.hpp"

extern "C" {
#include <string.h>

#include "avtp/CommonHeader.h"
#include "avtp/acf/Ntscf.h"
#include "avtp/acf/Tscf.h"
}

#include <iostream>

int AvtpUtil::extractCanFramesFromAvtp(uint8_t* pdu, CanFrame* can_frames, uint8_t* exp_cf_seqnum)
{
  uint8_t* cf_pdu = nullptr;
  uint8_t* acf_pdu = nullptr;
  uint8_t* udp_pdu = nullptr;
  uint8_t seq_num = 0;
  uint8_t i = 0;
  uint16_t proc_bytes = 0;
  uint16_t msg_length = 0;
  uint64_t stream_id;

  cf_pdu = pdu;

  // Accept only TSCF and NTSCF formats allowed
  uint8_t subtype = Avtp_CommonHeader_GetSubtype((Avtp_CommonHeader_t*)cf_pdu);
  if (subtype == AVTP_SUBTYPE_TSCF) {
    proc_bytes += AVTP_TSCF_HEADER_LEN;
    msg_length += Avtp_Tscf_GetStreamDataLength((Avtp_Tscf_t*)cf_pdu) + AVTP_TSCF_HEADER_LEN;
    stream_id = Avtp_Tscf_GetStreamId((Avtp_Tscf_t*)cf_pdu);
    seq_num = Avtp_Tscf_GetSequenceNum((Avtp_Tscf_t*)cf_pdu);
  } else if (subtype == AVTP_SUBTYPE_NTSCF) {
    proc_bytes += AVTP_NTSCF_HEADER_LEN;
    msg_length += Avtp_Ntscf_GetNtscfDataLength((Avtp_Ntscf_t*)cf_pdu) + AVTP_NTSCF_HEADER_LEN;
    stream_id = Avtp_Ntscf_GetStreamId((Avtp_Ntscf_t*)cf_pdu);
    seq_num = Avtp_Ntscf_GetSequenceNum((Avtp_Ntscf_t*)cf_pdu);
  } else {
    return -1;
  }

  // Check for stream id
  if (stream_id != STREAM_ID) {
    return -1;
  }

  // Check sequence numbers.
  if (seq_num != *exp_cf_seqnum) {
    std::cout << "Incorrect sequence num. Expected:" << *exp_cf_seqnum << " Recd.: " << seq_num << std::endl;

    *exp_cf_seqnum = seq_num;
  }

  while (proc_bytes < msg_length) {
    acf_pdu = &pdu[proc_bytes];

    if (!AvtpUtil::isValidAcfPacket(acf_pdu)) {
      return -1;
    }

    canid_t can_id = Avtp_Can_GetCanIdentifier((Avtp_Can_t*)acf_pdu);
    uint8_t* can_payload = Avtp_Can_GetPayload((Avtp_Can_t*)acf_pdu);
    uint16_t acf_msg_length = Avtp_Can_GetAcfMsgLength((Avtp_Can_t*)acf_pdu) * 4;
    uint16_t can_payload_length = Avtp_Can_GetCanPayloadLength((Avtp_Can_t*)acf_pdu);
    proc_bytes += acf_msg_length;

    // Need to work on multiple can frames
    CanFrame* frame = &(can_frames[i++]);

    if (Avtp_Can_GetFdf((Avtp_Can_t*)acf_pdu)) {
      frame->type = CanVariant::CAN_VARIANT_FD;
    } else {
      frame->type = CanVariant::CAN_VARIANT_CC;
    }

    // Handle EFF Flag
    if (Avtp_Can_GetEff((Avtp_Can_t*)acf_pdu)) {
      can_id |= CAN_EFF_FLAG;
    } else if (can_id > 0x7FF) {
      std::cout << "Error: CAN ID is > 0x7FF but the EFF bit is not set." << std::endl;
      return -1;
    }

    // Handle RTR Flag
    if (Avtp_Can_GetRtr((Avtp_Can_t*)acf_pdu)) {
      can_id |= CAN_RTR_FLAG;
    }

    if (frame->type == CanVariant::CAN_VARIANT_FD) {
      if (Avtp_Can_GetBrs((Avtp_Can_t*)acf_pdu)) {
        frame->data.fd.flags |= CANFD_BRS;
      }
      if (Avtp_Can_GetFdf((Avtp_Can_t*)acf_pdu)) {
        frame->data.fd.flags |= CANFD_FDF;
      }
      if (Avtp_Can_GetEsi((Avtp_Can_t*)acf_pdu)) {
        frame->data.fd.flags |= CANFD_ESI;
      }

      frame->data.fd.can_id = can_id;
      frame->data.fd.len = can_payload_length;
      memcpy(frame->data.fd.data, can_payload, can_payload_length);
    } else {
      frame->data.cc.can_id = can_id;
      frame->data.cc.len = can_payload_length;
      memcpy(frame->data.cc.data, can_payload, can_payload_length);
    }

    std::cout << "canid = " << can_id << ", length = " << can_payload_length << std::endl;
  }

  return i;
}

int AvtpUtil::insertCanFramesToAvtp(uint8_t* pdu, CanFrame* can_frames, uint8_t num_acf_msgs, uint8_t cf_seq_num)
{
  // Pack into control formats
  uint8_t* cf_pdu = nullptr;
  uint16_t pdu_length = 0;
  uint16_t cf_length = 0;
  int res = -1;

  // TODO
  int use_tscf = 0;

  // Prepare the control format: TSCF/NTSCF
  cf_pdu = pdu + pdu_length;
  res = AvtpUtil::initCfPdu(cf_pdu, use_tscf, cf_seq_num++);
  pdu_length += res;
  cf_length += res;

  int i = 0;
  while (i < num_acf_msgs) {
    uint8_t* acf_pdu = pdu + pdu_length;
    res = AvtpUtil::prepareAcfPacket(acf_pdu, &(can_frames[i]));
    pdu_length += res;
    cf_length += res;
    i++;
  }

  // Update the length of the PDU
  AvtpUtil::updateCfLength(cf_pdu, cf_length, use_tscf);

  return pdu_length;
}

int AvtpUtil::isValidAcfPacket(uint8_t* acf_pdu)
{
  Avtp_AcfCommon_t* pdu = (Avtp_AcfCommon_t*)acf_pdu;
  uint8_t acf_msg_type = Avtp_AcfCommon_GetAcfMsgType(pdu);
  if (acf_msg_type != AVTP_ACF_TYPE_CAN) {
    return 0;
  }

  return 1;
}

int AvtpUtil::initCfPdu(uint8_t* pdu, int use_tscf, int seq_num)
{
  int res;
  if (use_tscf) {
    Avtp_Tscf_t* tscf_pdu = (Avtp_Tscf_t*)pdu;
    memset(tscf_pdu, 0, AVTP_TSCF_HEADER_LEN);
    Avtp_Tscf_Init(tscf_pdu);
    Avtp_Tscf_SetField(tscf_pdu, AVTP_TSCF_FIELD_TU, 0U);
    Avtp_Tscf_SetField(tscf_pdu, AVTP_TSCF_FIELD_SEQUENCE_NUM, seq_num);
    Avtp_Tscf_SetField(tscf_pdu, AVTP_TSCF_FIELD_STREAM_ID, STREAM_ID);
    res = AVTP_TSCF_HEADER_LEN;
  } else {
    Avtp_Ntscf_t* ntscf_pdu = (Avtp_Ntscf_t*)pdu;
    memset(ntscf_pdu, 0, AVTP_NTSCF_HEADER_LEN);
    Avtp_Ntscf_Init(ntscf_pdu);
    Avtp_Ntscf_SetField(ntscf_pdu, AVTP_NTSCF_FIELD_SEQUENCE_NUM, seq_num);
    Avtp_Ntscf_SetField(ntscf_pdu, AVTP_NTSCF_FIELD_STREAM_ID, STREAM_ID);
    res = AVTP_NTSCF_HEADER_LEN;
  }
  return res;
}

int AvtpUtil::updateCfLength(uint8_t* cf_pdu, uint64_t length, int use_tscf)
{
  if (use_tscf) {
    uint64_t payloadLen = length - AVTP_TSCF_HEADER_LEN;
    Avtp_Tscf_SetField((Avtp_Tscf_t*)cf_pdu, AVTP_TSCF_FIELD_STREAM_DATA_LENGTH, payloadLen);
  } else {
    uint64_t payloadLen = length - AVTP_NTSCF_HEADER_LEN;
    Avtp_Ntscf_SetField((Avtp_Ntscf_t*)cf_pdu, AVTP_NTSCF_FIELD_NTSCF_DATA_LENGTH, payloadLen);
  }
  return 0;
}

int AvtpUtil::prepareAcfPacket(uint8_t* acf_pdu, CanFrame* frame)
{
  struct timespec now;
  canid_t can_id = 0U;
  uint8_t can_payload_length = 0U;

  // Clear bits
  Avtp_Can_t* pdu = (Avtp_Can_t*)acf_pdu;
  memset(pdu, 0, AVTP_CAN_HEADER_LEN);

  // Prepare ACF PDU for CAN
  Avtp_Can_Init(pdu);
  clock_gettime(CLOCK_REALTIME, &now);
  Avtp_Can_SetMessageTimestamp(pdu, (uint64_t)now.tv_nsec + (uint64_t)(now.tv_sec * 1e9));
  Avtp_Can_EnableMtv(pdu);

  // Set required CAN Flags
  if (frame->type == CanVariant::CAN_VARIANT_FD) {
    can_id = frame->data.fd.can_id;
    can_payload_length = frame->data.fd.len;
  } else {
    can_id = frame->data.cc.can_id;
    can_payload_length = frame->data.cc.len;
  }

  if (can_id & CAN_EFF_FLAG) {
    Avtp_Can_EnableEff(pdu);
  }

  if (can_id & CAN_RTR_FLAG) {
    Avtp_Can_EnableRtr(pdu);
  }

  if (frame->type == CanVariant::CAN_VARIANT_FD) {
    if (frame->data.fd.flags & CANFD_BRS) {
      Avtp_Can_EnableBrs(pdu);
    }
    if (frame->data.fd.flags & CANFD_FDF) {
      Avtp_Can_EnableFdf(pdu);
    }
    if (frame->data.fd.flags & CANFD_ESI) {
      Avtp_Can_EnableEsi(pdu);
    }
  }

  // Copy payload to ACF CAN PDU
  if (frame->type == CanVariant::CAN_VARIANT_FD) {
    Avtp_Can_CreateAcfMessage(pdu, can_id & CAN_EFF_MASK, frame->data.fd.data, can_payload_length,
                              (Avtp_CanVariant_t)frame->type);
  } else {
    Avtp_Can_CreateAcfMessage(pdu, can_id & CAN_EFF_MASK, frame->data.cc.data, can_payload_length,
                              (Avtp_CanVariant_t)frame->type);
  }

  return Avtp_Can_GetAcfMsgLength(pdu) * 4;
}
