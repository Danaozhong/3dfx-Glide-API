#include <cstring>

#include "chip.hpp"

namespace csim {

namespace {

constexpr FxU32 k3dRegisterSpace = 0x4000;  ///< Registers of all chips.

/**
 * @brief Stores a float into a float register.
 * @param[out] reg The register.
 * @param[in] value The value.
 */
void setFloat(volatile FxU32 &reg, float value)
{
  FxU32 bits;
  std::memcpy(&bits, &value, sizeof(bits));
  reg = bits;
}

}  // namespace

void Chip::attach(FxU8 *base, FxU32 fbSize, CsimStats &stats)
{
  context_.base = base;
  context_.fbSize = fbSize;
  context_.stats = &stats;
}

void Chip::storeRegister(FxU32 offset, FxU32 data)
{
  if ((offset >= SST_3D_OFFSET) && (offset < SST_3D_OFFSET + k3dRegisterSpace)) {
    store3d(offset - SST_3D_OFFSET, data);
  } else if (offset + 4 <= SST_MAX_LEGAL_OFFSET) {
    std::memcpy(context_.base + offset, &data, sizeof(data));  // e.g. 2D registers
  }
}

void Chip::storeFrameBuffer(FxU32 fbOffset, FxU32 data, FxU32 byteMask)
{
  frameBuffer_.storeBytes(fbOffset, data, byteMask);
}

void Chip::store3d(FxU32 offset, FxU32 data)
{
  const FxU32 chip = offset >> kChipFieldShift;
  const FxU32 reg = offset & kRegisterMask;
  bool stored = false;
  for (int t = 0; t < kNumTmus; t++) {
    if ((chip != 0) && !(chip & (kChipTmu0 << t))) continue;
    storeTmu(t, reg, data);
    stored = stored || (chip == (kChipTmu0 << t));
  }
  if (chip != 0) {
    // Other chip fields keep the write in plain memory, for reading back.
    if (!stored) std::memcpy(context_.base + SST_3D_OFFSET + offset, &data, sizeof(data));
    if (!(chip & kChipFbi)) return;
  }

  SstRegs &s = context_.sst();
  switch (reg) {
  case offsetof(SstRegs, colBufferAddr):
  case offsetof(SstRegs, auxBufferAddr):
    // The secondary buffers of anti-aliasing.
    if (data & SST_BUFFER_BASE_SELECT) return;
    break;
  case offsetof(SstRegs, sARGB):
    setFloat(s.sAlpha, static_cast<float>((data >> 24) & 0xFF));
    setFloat(s.sRed, static_cast<float>((data >> 16) & 0xFF));
    setFloat(s.sGreen, static_cast<float>((data >> 8) & 0xFF));
    setFloat(s.sBlue, static_cast<float>(data & 0xFF));
    break;
  }
  std::memcpy(context_.base + SST_3D_OFFSET + reg, &data, sizeof(data));

  switch (reg) {
  case offsetof(SstRegs, nopCMD): go(Command::Nop); break;
  case offsetof(SstRegs, triangleCMD): go(Command::Triangle); break;
  case offsetof(SstRegs, fastfillCMD): go(Command::FastFill); break;
  case offsetof(SstRegs, swapbufferCMD): go(Command::SwapBuffer); break;
  case offsetof(SstRegs, sBeginTriCMD): go(Command::BeginTriangle); break;
  case offsetof(SstRegs, sDrawTriCMD): go(Command::DrawTriangle); break;
  case offsetof(SstRegs, FtriangleCMD):
    context_.stats->unsimulated++;  // needs the float to fixed conversion of the Fv* registers
    break;
  }
}

void Chip::storeTmu(int tmu, FxU32 reg, FxU32 data)
{
  // Palette entries go through nccTable0[4] to nccTable0[11] with bit 31 set.
  constexpr FxU32 kPaletteStart = offsetof(SstRegs, nccTable0) + 4 * sizeof(FxU32);
  constexpr FxU32 kPaletteEnd = offsetof(SstRegs, nccTable0) + 12 * sizeof(FxU32);
  if ((reg >= kPaletteStart) && (reg < kPaletteEnd) && (data & 0x80000000u)) {
    textureUnits_[tmu].storePalette(static_cast<int>((reg - offsetof(SstRegs, nccTable0)) / sizeof(FxU32)),
                                    data);
    return;
  }
  std::memcpy(context_.base + SST_3D_OFFSET + ((kChipTmu0 << tmu) << kChipFieldShift) + reg, &data,
              sizeof(data));
}

RenderState Chip::renderState() const
{
  return RenderState::decode(context_.sst(), context_.io());
}

void Chip::go(Command cmd)
{
  SstRegs &s = context_.sst();

  switch (cmd) {
  case Command::Nop:
    if (s.nopCMD & SST_NOP_RESET_PIXEL_STATS) {
      s.stats.fbiPixelsIn = 0;
      s.stats.fbiChromaFail = 0;
      s.stats.fbiZfuncFail = 0;
      s.stats.fbiAfuncFail = 0;
      s.stats.fbiPixelsOut = 0;
      s.fbiStencilFail = 0;
    }
    if (s.nopCMD & SST_NOP_RESET_TRIANGLE_STATS) s.fbiTrianglesOut = 0;
    break;

  case Command::FastFill: {
    RenderState state = renderState();
    if ((s.fastfillCMD & SST_FASTFILL_DISABLE_DITHER) &&
        !(context_.io().dramInit1 & SST_MCTL_TYPE_SDRAM)) {
      state.dither = Dither::None;
    }
    rasterizer_.fastFill(state);
    break;
  }

  case Command::SwapBuffer:
    swapBuffer();
    break;

  case Command::BeginTriangle:
    setupUnit_.beginTriangle();
    break;

  case Command::DrawTriangle:
    if (setupUnit_.drawTriangle(z_)) rasterizer_.triangle(renderState(), z_);
    break;

  case Command::Triangle:
    rasterizer_.triangle(renderState(), z_);
    break;
  }
}

void Chip::swapBuffer()
{
  const SstRegs &s = context_.sst();
  if (!(s.swapbufferCMD & SST_SWAP_DONT_SWAP)) {
    context_.io().vidCurrOverlayStartAddr = s.leftOverlayBuf & SST_VIDEO_START_ADDR;
    const RenderState state = renderState();
    frameBuffer_.captureImage(state.color, state.format, s.clipLeftRight & 0xFFFF,
                              s.clipBottomTop & 0xFFFF);
  }
  context_.stats->swaps++;
}

}  // namespace csim
