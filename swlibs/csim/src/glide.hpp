/**
 * @file
 * @brief The Glide headers the simulator implements.
 *
 * They are C headers, and most of them do not declare C linkage themselves.
 */
#ifndef CSIM_GLIDE_HPP
#define CSIM_GLIDE_HPP

extern "C" {
#include <3dfx.h>
#include <fxdll.h>
#include <fxvid.h>  // fxhal.h needs FxVideoTimingInfo and SstRegs declared before it
#include <csim.h>
#include <fxhal.h>
#include <h3defs.h>
#include <h3gdefs.h>
}

#endif  // CSIM_GLIDE_HPP
