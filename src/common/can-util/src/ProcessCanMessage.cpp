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

extern "C" {
#include "can_codec.h"
}

#include "ProcessCanMessage.hpp"

void ProcessCanMessage::setValue(std::string &name, uint64_t value, double scaled, CanMessage &data_out)
{
  for (auto signal = data_out.signals.begin(); signal != data_out.signals.end(); signal++) {
    if (name.compare(signal->name) == 0) {
      // update the value
      break;
    }
  }
}

/*
 * Copyright (c) 2025, Kiran Kumar Arsam,
 * Copyright (c) 2016, Eduard Bröcker,
 * All rights reserved.
 * Content is simplified to C++ usage.
 */
void ProcessCanMessage::process(uint8_t *can_data, CanMessage &msg, CanMessage &data_out)
{
  uint64_t value = 0;
  double scaled = 0.;
  unsigned int muxerVal = 0;

  if (msg.isMultiplexed) {
    // find multiplexer:
    for (auto signal = msg.signals.begin(); signal != msg.signals.end(); signal++) {
      if (1 == signal->isMultiplexer) {
        muxerVal = Can_Codec_ExtractSignal(can_data, signal->startBit, signal->signalLength,
                                           (bool)signal->is_big_endian, signal->is_signed);
        scaled = Can_Codec_ToPhysicalValue(muxerVal, signal->factor, signal->offset, signal->is_signed);
        ProcessCanMessage::setValue(signal->name, muxerVal, scaled, data_out);
        break;
      }
    }

    for (auto signal = msg.signals.begin(); signal != msg.signals.end(); signal++) {
      // decode not multiplexed signals and signals with correct muxVal
      if (0 == signal->isMultiplexer || (2 == signal->isMultiplexer && signal->muxId == muxerVal)) {
        value = Can_Codec_ExtractSignal(can_data, signal->startBit, signal->signalLength, (bool)signal->is_big_endian,
                                        signal->is_signed);
        scaled = Can_Codec_ToPhysicalValue(value, signal->factor, signal->offset, signal->is_signed);
        ProcessCanMessage::setValue(signal->name, value, scaled, data_out);
      }
    }
  } else {
    for (auto signal = msg.signals.begin(); signal != msg.signals.end(); signal++) {
      value = Can_Codec_ExtractSignal(can_data, signal->startBit, signal->signalLength, (bool)signal->is_big_endian,
                                      signal->is_signed);
      scaled = Can_Codec_ToPhysicalValue(value, signal->factor, signal->offset, signal->is_signed);
      ProcessCanMessage::setValue(signal->name, value, scaled, data_out);
    }
  }
}