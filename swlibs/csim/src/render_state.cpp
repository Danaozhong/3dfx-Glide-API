#include "fixed_point.hpp"
#include "render_state.hpp"

namespace csim {

namespace {

/**
 * @brief Decodes the layout of a color or aux buffer.
 * @param[in] addr The colBufferAddr or auxBufferAddr register.
 * @param[in] stride The colBufferStride or auxBufferStride register.
 * @param[in] depth Bytes per pixel.
 */
BufferLayout bufferLayout(FxU32 addr, FxU32 stride, FxU32 depth)
{
  BufferLayout layout;
  layout.base = (addr & SST_BUFFER_BASE_ADDR) >> SST_BUFFER_BASE_SHIFT;
  layout.tiled = (stride & SST_BUFFER_MEMORY_TYPE) != 0;
  layout.stride = (stride & (layout.tiled ? SST_BUFFER_TILE_STRIDE : SST_BUFFER_LINEAR_STRIDE)) >>
                  SST_BUFFER_STRIDE_SHIFT;
  layout.depth = depth;
  return layout;
}

/**
 * @brief Decodes a clip rectangle.
 * @param[in] leftRight The clipLeftRight or clipLeftRight1 register.
 * @param[in] bottomTop The clipBottomTop or clipBottomTop1 register.
 * @param[in] minMask Mask of the xMin and yMin fields.
 */
ClipRect decodeClipRect(FxU32 leftRight, FxU32 bottomTop, FxU32 minMask)
{
  ClipRect rect;
  rect.xMin = static_cast<int>((leftRight >> 16) & minMask);
  rect.xMax = static_cast<int>(leftRight & 0xFFFF);
  rect.yMin = static_cast<int>((bottomTop >> 16) & minMask);
  rect.yMax = static_cast<int>(bottomTop & 0xFFFF);
  return rect;
}

/**
 * @brief Decodes a depth or alpha function.
 * @param[in] reg The register with the function.
 * @param[in] less The less bit of the function.
 * @param[in] equal The equal bit of the function.
 * @param[in] greater The greater bit of the function.
 */
CompareFunc compareFunc(FxU32 reg, FxU32 less, FxU32 equal, FxU32 greater)
{
  return CompareFunc{(reg & less) != 0, (reg & equal) != 0, (reg & greater) != 0};
}

/**
 * @brief Decodes the selects of fbzColorPath.
 * @param[in] fbzCP The fbzColorPath register.
 * @param[out] r Receives the color and alpha selects.
 */
void decodeColorPath(FxU32 fbzCP, RenderState &r)
{
  switch (fbzCP & SST_RGBSELECT) {
  case SST_RGBSEL_TREXOUT: r.otherRgb = RgbSource::Texture; break;
  case SST_RGBSEL_C1: r.otherRgb = RgbSource::Color1; break;
  default: r.otherRgb = RgbSource::Iterated; break;  // also LFB, outside of LFB writes
  }
  switch (fbzCP & SST_ASELECT) {
  case SST_ASEL_TREXOUT: r.otherAlpha = AlphaSource::Texture; break;
  case SST_ASEL_C1: r.otherAlpha = AlphaSource::Color1; break;
  default: r.otherAlpha = AlphaSource::Iterated; break;
  }
  r.localRgb = (fbzCP & SST_LOCALSELECT) ? RgbSource::Color0 : RgbSource::Iterated;
  switch (fbzCP & SST_ALOCALSELECT) {
  case SST_ALOCAL_C0: r.localAlpha = AlphaSource::Color0; break;
  case SST_ALOCAL_Z: r.localAlpha = AlphaSource::Z; break;
  case SST_ALOCAL_W: r.localAlpha = AlphaSource::W; break;
  default: r.localAlpha = AlphaSource::Iterated; break;
  }
  r.mselect7 = RgbSource::Iterated;
}

/**
 * @brief Decodes the selects of combineMode.
 * @param[in] cm The combineMode register.
 * @param[out] r Receives the color and alpha selects.
 */
void decodeCombineMode(FxU32 cm, RenderState &r)
{
  switch (cm & SST_CM_CC_OTHERSELECT) {
  case SST_CM_CC_OTHERSELECT_TRGB: r.otherRgb = RgbSource::Texture; break;
  case SST_CM_CC_OTHERSELECT_C1_RGB: r.otherRgb = RgbSource::Color1; break;
  case SST_CM_CC_OTHERSELECT_IA: r.otherRgb = RgbSource::IteratedAlpha; break;
  case SST_CM_CC_OTHERSELECT_TA: r.otherRgb = RgbSource::TextureAlpha; break;
  case SST_CM_CC_OTHERSELECT_C1_A: r.otherRgb = RgbSource::Color1Alpha; break;
  case SST_CM_CC_OTHERSELECT_ZERO: r.otherRgb = RgbSource::Zero; break;
  default: r.otherRgb = RgbSource::Iterated; break;
  }
  switch (cm & SST_CM_CCA_OTHERSELECT) {
  case SST_CM_CCA_OTHERSELECT_TA: r.otherAlpha = AlphaSource::Texture; break;
  case SST_CM_CCA_OTHERSELECT_C1_A: r.otherAlpha = AlphaSource::Color1; break;
  default: r.otherAlpha = AlphaSource::Iterated; break;
  }
  switch (cm & SST_CM_CC_LOCALSELECT) {
  case SST_CM_CC_LOCALSELECT_C0_RGB: r.localRgb = RgbSource::Color0; break;
  case SST_CM_CC_LOCALSELECT_TRGB: r.localRgb = RgbSource::Texture; break;
  case SST_CM_CC_LOCALSELECT_IA: r.localRgb = RgbSource::IteratedAlpha; break;
  case SST_CM_CC_LOCALSELECT_C0_A: r.localRgb = RgbSource::Color0Alpha; break;
  case SST_CM_CC_LOCALSELECT_TA: r.localRgb = RgbSource::TextureAlpha; break;
  case SST_CM_CC_LOCALSELECT_ZERO:
  case SST_CM_CC_LOCALSELECT_ZERO7: r.localRgb = RgbSource::Zero; break;
  default: r.localRgb = RgbSource::Iterated; break;
  }
  switch (cm & SST_CM_CCA_LOCALSELECT) {
  case SST_CM_CCA_LOCALSELECT_C0_A: r.localAlpha = AlphaSource::Color0; break;
  case SST_CM_CCA_LOCALSELECT_IZ: r.localAlpha = AlphaSource::Z; break;
  case SST_CM_CCA_LOCALSELECT_IW: r.localAlpha = AlphaSource::W; break;
  case SST_CM_CCA_LOCALSELECT_TA: r.localAlpha = AlphaSource::Texture; break;
  case SST_CM_CCA_LOCALSELECT_ZERO: r.localAlpha = AlphaSource::Zero; break;
  default: r.localAlpha = AlphaSource::Iterated; break;
  }
  switch (cm & SST_CM_CC_MSELECT_7) {
  case SST_CM_CC_MSELECT_7_C1_RGB: r.mselect7 = RgbSource::Color1; break;
  case SST_CM_CC_MSELECT_7_IA: r.mselect7 = RgbSource::IteratedAlpha; break;
  case SST_CM_CC_MSELECT_7_C1_A: r.mselect7 = RgbSource::Color1Alpha; break;
  default: r.mselect7 = RgbSource::Iterated; break;
  }
}

}  // namespace

RenderState RenderState::decode(const SstRegs &s, const SstIORegs &io)
{
  RenderState r;
  const FxU32 fbz = s.fbzMode;
  const FxU32 fbzCP = s.fbzColorPath;
  const FxU32 alp = s.alphaMode;
  const FxU32 cm = s.combineMode;
  const FxU32 rm = s.renderMode;
  const FxU32 stencilMode = s.stencilMode;

  switch (rm & SST_RM_3D_MODE) {
  case SST_RM_16BPP: r.format = PixelFormat::Rgb565; break;
  case SST_RM_15BPP: r.format = PixelFormat::Argb1555; break;
  case SST_RM_32BPP: r.format = PixelFormat::Argb8888; break;
  default: r.format = PixelFormat::Unsupported; break;
  }
  const bool is32Bpp = r.format == PixelFormat::Argb8888;
  r.alphaBuffer = (fbz & SST_ENALPHABUFFER) != 0;
  r.color = bufferLayout(s.colBufferAddr, s.colBufferStride, is32Bpp ? 4 : 2);
  r.aux = bufferLayout(s.auxBufferAddr, s.auxBufferStride, is32Bpp ? 4 : (r.alphaBuffer ? 1 : 2));

  r.flipY = (fbz & SST_YORIGIN) != 0;
  r.yOrigin = static_cast<int>((rm & SST_RM_YORIGIN_SELECT)
                                   ? (rm & SST_RM_YORIGIN_TOP) >> SST_RM_YORIGIN_TOP_SHIFT
                                   : (io.miscInit0 & SST_YORIGIN_TOP) >> SST_YORIGIN_TOP_SHIFT);
  r.clip = (fbz & SST_ENRECTCLIP) != 0;
  r.clipRect = decodeClipRect(s.clipLeftRight, s.clipBottomTop, 0xFFFF);
  r.clip1 = (s.clipLeftRight1 & SST_ENRECTCLIP1) != 0;
  r.clip1Exclusive = (s.clipBottomTop1 & SST_RECTCLIP1_EX) != 0;
  r.clip1Rect = decodeClipRect(s.clipLeftRight1, s.clipBottomTop1, 0x0FFF);

  r.clampIterators = (fbzCP & SST_RGBAZ_CLAMP) != 0;
  r.color0 = s.c0;
  r.color1 = s.c1;
  if (cm & SST_CM_USE_COMBINE_MODE) {
    decodeCombineMode(cm, r);
  } else {
    decodeColorPath(fbzCP, r);
  }
  r.localRgbFromTextureAlpha = (fbzCP & SST_LOCALSELECT_OVERRIDE_WITH_ATEX) != 0;
  r.combine.mode = (fbzCP >> SST_CCOMBINE_SHIFT) << SST_TCOMBINE_SHIFT;
  r.combine.rgbReverse = (fbzCP & SST_CC_REVERSE_BLEND) ? 0x00 : 0xFF;
  r.combine.alphaReverse = (fbzCP & SST_CCA_REVERSE_BLEND) ? 0x00 : 0xFF;
  r.combine.combineMode = cm;
  r.combine.color1Alpha = static_cast<FxU8>(s.c1 >> 24);

  r.alphaMask = (fbz & SST_ENALPHAMASK) != 0;
  r.chromaKey = (fbz & SST_ENCHROMAKEY) != 0;
  r.chromaKeyColor = s.chromaKey;
  r.chromaRange = s.chromaRange;
  r.alphaTest = (alp & SST_ENALPHAFUNC) != 0;
  r.alphaFunc = compareFunc(alp, SST_ALPHAFUNC_LT, SST_ALPHAFUNC_EQ, SST_ALPHAFUNC_GT);
  r.alphaRef = (alp & SST_ALPHAREF) >> SST_ALPHAREF_SHIFT;
  r.depthTest = (fbz & SST_ENDEPTHBUFFER) != 0;
  r.depthFunc = compareFunc(fbz, SST_ZFUNC_LT, SST_ZFUNC_EQ, SST_ZFUNC_GT);
  r.depthBias = (fbz & SST_ENZBIAS) != 0;
  r.depthBiasValue = is32Bpp ? signExtend(s.zaColor, 24) : signExtend(s.zaColor, 16) * 256;
  r.depthFromZaColor = (fbz & SST_ZCOMPARE_TO_ZACOLOR) != 0;
  r.zaColor = s.zaColor;

  if (fbz & SST_ENDITHER) r.dither = (fbz & SST_DITHER2x2) ? Dither::Matrix2x2 : Dither::Matrix4x4;
  r.rgbWrite = (fbz & SST_RGBWRMASK) != 0;
  r.auxWrite = (fbz & SST_ZAWRMASK) != 0;
  r.colorWriteMask = ((rm & SST_RM_RED_WMASK) ? 0x00FF0000u : 0) |
                     ((rm & SST_RM_GREEN_WMASK) ? 0x0000FF00u : 0) |
                     ((rm & SST_RM_BLUE_WMASK) ? 0x000000FFu : 0) |
                     ((rm & SST_RM_ALPHA_WMASK) ? 0xFF000000u : 0);
  switch (rm & SST_RM_ALPHAMODE) {
  case SST_RM_ALPHA_ZERO: r.alpha1555 = Alpha1555::Zero; break;
  case SST_RM_ALPHA_ONE: r.alpha1555 = Alpha1555::One; break;
  default: r.alpha1555 = Alpha1555::Source; break;
  }
  r.stencilWriteMask = (stencilMode & SST_STENCIL_WMASK) >> SST_STENCIL_WMASK_SHIFT;
  r.stencilRef = (stencilMode & SST_STENCIL_REF) >> SST_STENCIL_REF_SHIFT;

  r.needsUnsimulated = (fbzCP & SST_ENTEXTUREMAP) || (s.fogMode & SST_ENFOGGING) ||
                       (alp & SST_ENALPHABLEND) || (stencilMode & SST_STENCIL_ENABLE) ||
                       (fbz & (SST_WBUFFER | SST_ENSTIPPLE)) || (s.aaCtrl & SST_AA_CONTROL_AA_ENABLE);
  return r;
}

}  // namespace csim
