#include <cstddef>

#include "command_fifo.hpp"

namespace csim {

void CommandFifo::init(FxU32 fifoStart, FxU32 size)
{
  state_ = kNewPacket;
  wordCount_ = 0;
  bogusCount_ = 0;
  start_ = fifoStart;
  end_ = fifoStart + size;
  readPtr_ = fifoStart;
}

void CommandFifo::write(FxU32 fbOffset, FxU32 data)
{
  // The chip executes the words in the order of the FIFO, and Glide writes
  // them in that order. A word elsewhere means that this model is wrong.
  if (fbOffset != readPtr_) {
    context_.stats->fifoErrors++;
    readPtr_ = fbOffset;
  }
  readPtr_ += 4;
  execute(data);
}

void CommandFifo::execute(FxU32 data)
{
  switch (state_) {
  case kNewPacket:
    decodeHeader(data);
    break;

  case kBogusDiscard:  // padding after the packet
    bogusCount_--;
    break;

  case SSTCP_PKT0:  // the second word of JMP_AGP
    packet0(data, true);
    wordCount_--;
    break;

  case SSTCP_PKT1:  // the next register of the block
    bus_.storeRegister(registerPtr_, data);
    if (packetWord_ & SSTCP_INC) registerPtr_ += 4;
    wordCount_--;
    break;

  case SSTCP_PKT2:
  case SSTCP_PKT4:
    maskedWord(data);
    break;

  case SSTCP_PKT3:
    vertexWord(data);
    break;

  case SSTCP_PKT5:
    frameBufferWord(data);
    break;

  case SSTCP_PKT6:  // the AGP transfer is not simulated, its words are skipped
    wordCount_--;
    break;

  default:
    context_.stats->fifoErrors++;
    break;
  }

  // The packet ends when its payload and its padding are consumed.
  if ((bogusCount_ == 0) && ((state_ == kBogusDiscard) || (wordCount_ == 0))) {
    state_ = kNewPacket;
  }
}

void CommandFifo::decodeHeader(FxU32 data)
{
  CsimStats &stats = *context_.stats;

  packetWord_ = data;
  state_ = static_cast<int>(data & SSTCP_PKT);
  bogusCount_ = 0;

  // wordCount_ is the number of payload words, or for packets 2 and 4 the mask
  // of the registers that follow.
  switch (state_) {
  case SSTCP_PKT0:
    packet0(data, false);
    break;
  case SSTCP_PKT1:
    registerPtr_ = ((data & SSTCP_REGBASE) >> SSTCP_REGBASE_SHIFT) * 4;
    registerPtr_ += (data & SSTCP_PKT1_2D) ? SST_2D_OFFSET : SST_3D_OFFSET;
    wordCount_ = (data & SSTCP_PKT1_NWORDS) >> SSTCP_PKT1_NWORDS_SHIFT;
    break;
  case SSTCP_PKT2:  // bit 0 of the mask is clip0min, the first 2D register after status
    registerPtr_ = SST_2D_OFFSET + offsetof(SstGRegs, clip0min);
    wordCount_ = (data & SSTCP_PKT2_MASK) >> SSTCP_PKT2_MASK_SHIFT;
    break;
  case SSTCP_PKT3:
    packet3Header(data);
    break;
  case SSTCP_PKT4:
    registerPtr_ = ((data & SSTCP_REGBASE) >> SSTCP_REGBASE_SHIFT) * 4;
    registerPtr_ += (data & SSTCP_PKT4_2D) ? SST_2D_OFFSET : SST_3D_OFFSET;
    wordCount_ = (data & SSTCP_PKT4_MASK) >> SSTCP_PKT4_MASK_SHIFT;
    bogusCount_ = (data & SSTCP_BOGUS_WORDS) >> SSTCP_BOGUS_WORDS_SHIFT;
    break;
  case SSTCP_PKT5:
    // The payload is the start address, then the data words; vertexCount_
    // counts the payload words.
    wordCount_ = (data & SSTCP_PKT5_NWORDS) >> SSTCP_PKT5_NWORDS_SHIFT;
    vertexCount_ = 0;
    registerPtr_ = 0;
    break;
  case SSTCP_PKT6:
    wordCount_ = 4;
    stats.unsimulated++;
    break;
  default:  // packet 7 does not exist
    stats.fifoErrors++;
    state_ = kBogusDiscard;
    wordCount_ = 0;
    break;
  }

  // Packets 0 and 3 count their words differently.
  if ((state_ != SSTCP_PKT0) && (state_ != SSTCP_PKT3) && (state_ > 0) && (wordCount_ == 0)) {
    stats.fifoErrors++;  // 0 length packets are illegal
  }
}

void CommandFifo::maskedWord(FxU32 data)
{
  // Skip the registers whose mask bit is clear, the word goes to the next one
  // that is set.
  while ((wordCount_ & 1) == 0) {
    registerPtr_ += 4;
    wordCount_ >>= 1;
  }
  bus_.storeRegister(registerPtr_, data);
  wordCount_ >>= 1;
  registerPtr_ += 4;
  if (wordCount_ == 0) state_ = kBogusDiscard;
}

void CommandFifo::vertexWord(FxU32 data)
{
  // wordCount_ is the index of the vertex register in vertexData_.
  bus_.storeRegister(SST_3D_OFFSET + vertexData_[wordCount_], data);
  wordCount_++;
  if (vertexData_[wordCount_] != 0) return;

  // The vertex is complete: sBeginTriCMD starts a new strip or fan with it,
  // sDrawTriCMD adds it and draws a triangle once there are 3 vertices.
  FxU32 reg = offsetof(SstRegs, sDrawTriCMD);
  switch (packetWord_ & SSTCP_PKT3_CMD) {
  case SSTCP_PKT3_BDDBDD:  // independent triangles
    if ((vertexCount_ % 3) == 0) reg = offsetof(SstRegs, sBeginTriCMD);
    break;
  case SSTCP_PKT3_BDDDDD:  // a new strip or fan
    if (vertexCount_ == 0) reg = offsetof(SstRegs, sBeginTriCMD);
    break;
  default:  // SSTCP_PKT3_DDDDDD continues the strip or fan of the last packet
    break;
  }
  bus_.storeRegister(SST_3D_OFFSET + reg, 0);
  if (++vertexCount_ == vertexLimit_) state_ = kBogusDiscard;
  wordCount_ = 1;
}

void CommandFifo::frameBufferWord(FxU32 data)
{
  if (vertexCount_ == 0) {
    registerPtr_ = data & SSTCP_PKT5_BASEADDR;
  } else {
    // The header has active low byte enables for the first data word and the
    // last one; the words in between are written whole.
    FxU32 byteMask = 0;
    if (vertexCount_ == 1) {
      byteMask = (packetWord_ & SSTCP_PKT5_BYTEN_W2) >> SSTCP_PKT5_BYTEN_W2_SHIFT;
    } else if (wordCount_ == 1) {
      byteMask = (packetWord_ & SSTCP_PKT5_BYTEN_WN) >> SSTCP_PKT5_BYTEN_WN_SHIFT;
    }
    if ((packetWord_ & SSTCP_PKT5_SPACE) == SSTCP_PKT5_LFB) {
      bus_.storeFrameBuffer(registerPtr_, data, byteMask);
    } else {
      context_.stats->unsimulated++;  // YUV, 3D LFB and texture port
    }
    registerPtr_ += 4;
    wordCount_--;
  }
  vertexCount_++;
}

void CommandFifo::packet0(FxU32 data, bool secondWord)
{
  CsimStats &stats = *context_.stats;

  if (secondWord) {  // high address bits of JMP_AGP
    state_ = kBogusDiscard;
    bogusCount_ = 0;
    readPtr_ = (data << 25) | jmpAddr_;
    stats.fifoWraps++;
    if ((readPtr_ < start_) || (readPtr_ >= end_)) stats.fifoErrors++;
    return;
  }

  registerPtr_ = 0;
  wordCount_ = 0;
  const FxU32 func = data & (SSTCP_PKT0_FUNC | SSTCP_PKT);
  if (func != SSTCP_PKT0_JMP_AGP) {
    state_ = kBogusDiscard;
    bogusCount_ = 0;
  }

  const FxU32 addr = (data & SSTCP_PKT0_ADDR) >> (SSTCP_PKT0_ADDR_SHIFT - 2);
  switch (func) {
  case SSTCP_PKT0_NOP:
    break;
  case SSTCP_PKT0_JMP_LOCAL:
    readPtr_ = addr;
    stats.fifoWraps++;
    if ((addr < start_) || (addr >= end_)) stats.fifoErrors++;
    break;
  case SSTCP_PKT0_JMP_AGP:
    jmpAddr_ = addr;
    wordCount_ = 1;
    break;
  default:  // JSR and RET are not used by Glide
    stats.fifoErrors++;
    break;
  }
}

void CommandFifo::packet3Header(FxU32 data)
{
  registerPtr_ = SST_3D_OFFSET + offsetof(SstRegs, sSetupMode);
  vertexLimit_ = (data & SSTCP_PKT3_NUMVERTEX) >> SSTCP_PKT3_NUMVERTEX_SHIFT;
  bogusCount_ = (data & SSTCP_BOGUS_WORDS) >> SSTCP_BOGUS_WORDS_SHIFT;
  const FxU32 smode = ((data & SSTCP_PKT3_SMODE) >> (SSTCP_PKT3_SMODE_SHIFT - 16)) |
                      ((data & SSTCP_PKT3_PMASK) >> SSTCP_PKT3_PMASK_SHIFT);
  if (vertexLimit_ == 0) context_.stats->fifoErrors++;

  bus_.storeRegister(registerPtr_, smode);

  // The registers of one vertex, in packet order.
  FxU32 n = 1;
  wordCount_ = n;
  vertexCount_ = 0;
  vertexData_[n++] = offsetof(SstRegs, sVx);
  vertexData_[n++] = offsetof(SstRegs, sVy);
  const bool packed = (data & SSTCP_PKT3_PACKEDCOLOR) != 0;
  if (smode & SST_SETUP_RGB) {
    if (packed) {
      vertexData_[n++] = offsetof(SstRegs, sARGB);
    } else {
      vertexData_[n++] = offsetof(SstRegs, sRed);
      vertexData_[n++] = offsetof(SstRegs, sGreen);
      vertexData_[n++] = offsetof(SstRegs, sBlue);
    }
  }
  if (smode & SST_SETUP_A) {
    if (!packed) {
      vertexData_[n++] = offsetof(SstRegs, sAlpha);
    } else if (!(smode & SST_SETUP_RGB)) {
      vertexData_[n++] = offsetof(SstRegs, sARGB);
    }
  }
  if (smode & SST_SETUP_Z) vertexData_[n++] = offsetof(SstRegs, sVz);
  if (smode & SST_SETUP_Wfbi) vertexData_[n++] = offsetof(SstRegs, sOowfbi);
  if (smode & SST_SETUP_W0) vertexData_[n++] = offsetof(SstRegs, sOow0);
  if (smode & SST_SETUP_ST0) {
    vertexData_[n++] = offsetof(SstRegs, sSow0);
    vertexData_[n++] = offsetof(SstRegs, sTow0);
  }
  if (smode & SST_SETUP_W1) vertexData_[n++] = offsetof(SstRegs, sOow1);
  if (smode & SST_SETUP_ST1) {
    vertexData_[n++] = offsetof(SstRegs, sSow1);
    vertexData_[n++] = offsetof(SstRegs, sTow1);
  }
  vertexData_[n] = 0;
}

}  // namespace csim
