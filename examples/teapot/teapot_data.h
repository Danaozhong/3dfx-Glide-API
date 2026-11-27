// The Utah teapot of Martin Newell (1975), in the form of the GLAUX and GLUT
// libraries of Silicon Graphics: 10 bicubic Bezier patches of 16 control
// points each, z up. The rim, body, lid and bottom patches are mirrored in
// x and y, the handle and spout in y only, for the 32 patches of the teapot.
// The tables are unchanged from TEAPOT.H, which comes with this notice:
/*
 * (c) Copyright 1993, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED 
 * Permission to use, copy, modify, and distribute this software for 
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that 
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission. 
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * US Government Users Restricted Rights 
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(TM) is a trademark of Silicon Graphics, Inc.
 */
#ifndef CSIM_TEAPOT_TEAPOT_DATA_H
#define CSIM_TEAPOT_TEAPOT_DATA_H

namespace teapot {

constexpr int kPatchCount = 10;
constexpr int kMirroredPatches = 6;  // the first 6 are mirrored in x and y

constexpr int kPatches[kPatchCount][16] = {
    {102, 103, 104, 105, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},  // rim
    {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},  // body
    {24, 25, 26, 27, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40},  // body
    {96, 96, 96, 96, 97, 98, 99, 100, 101, 101, 101, 101, 0, 1, 2, 3},  // lid
    {0, 1, 2, 3, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117},  // lid
    {118, 118, 118, 118, 124, 122, 119, 121, 123, 126, 125, 120, 40, 39, 38, 37},  // bottom
    {41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56},  // handle
    {53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 28, 65, 66, 67},  // handle
    {68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83},  // spout
    {80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95},  // spout
};

constexpr float kControlPoints[][3] = {
    {0.2f, 0.0f, 2.7f}, {0.2f, -0.112f, 2.7f}, {0.112f, -0.2f, 2.7f}, {0.0f, -0.2f, 2.7f},
    {1.3375f, 0.0f, 2.53125f}, {1.3375f, -0.749f, 2.53125f}, {0.749f, -1.3375f, 2.53125f},
    {0.0f, -1.3375f, 2.53125f}, {1.4375f, 0.0f, 2.53125f}, {1.4375f, -0.805f, 2.53125f},
    {0.805f, -1.4375f, 2.53125f}, {0.0f, -1.4375f, 2.53125f}, {1.5f, 0.0f, 2.4f},
    {1.5f, -0.84f, 2.4f}, {0.84f, -1.5f, 2.4f}, {0.0f, -1.5f, 2.4f},
    {1.75f, 0.0f, 1.875f}, {1.75f, -0.98f, 1.875f}, {0.98f, -1.75f, 1.875f},
    {0.0f, -1.75f, 1.875f}, {2.0f, 0.0f, 1.35f}, {2.0f, -1.12f, 1.35f},
    {1.12f, -2.0f, 1.35f}, {0.0f, -2.0f, 1.35f}, {2.0f, 0.0f, 0.9f}, {2.0f, -1.12f, 0.9f},
    {1.12f, -2.0f, 0.9f}, {0.0f, -2.0f, 0.9f}, {-2.0f, 0.0f, 0.9f}, {2.0f, 0.0f, 0.45f},
    {2.0f, -1.12f, 0.45f}, {1.12f, -2.0f, 0.45f}, {0.0f, -2.0f, 0.45f},
    {1.5f, 0.0f, 0.225f}, {1.5f, -0.84f, 0.225f}, {0.84f, -1.5f, 0.225f},
    {0.0f, -1.5f, 0.225f}, {1.5f, 0.0f, 0.15f}, {1.5f, -0.84f, 0.15f},
    {0.84f, -1.5f, 0.15f}, {0.0f, -1.5f, 0.15f}, {-1.6f, 0.0f, 2.025f},
    {-1.6f, -0.3f, 2.025f}, {-1.5f, -0.3f, 2.25f}, {-1.5f, 0.0f, 2.25f},
    {-2.3f, 0.0f, 2.025f}, {-2.3f, -0.3f, 2.025f}, {-2.5f, -0.3f, 2.25f},
    {-2.5f, 0.0f, 2.25f}, {-2.7f, 0.0f, 2.025f}, {-2.7f, -0.3f, 2.025f},
    {-3.0f, -0.3f, 2.25f}, {-3.0f, 0.0f, 2.25f}, {-2.7f, 0.0f, 1.8f},
    {-2.7f, -0.3f, 1.8f}, {-3.0f, -0.3f, 1.8f}, {-3.0f, 0.0f, 1.8f},
    {-2.7f, 0.0f, 1.575f}, {-2.7f, -0.3f, 1.575f}, {-3.0f, -0.3f, 1.35f},
    {-3.0f, 0.0f, 1.35f}, {-2.5f, 0.0f, 1.125f}, {-2.5f, -0.3f, 1.125f},
    {-2.65f, -0.3f, 0.9375f}, {-2.65f, 0.0f, 0.9375f}, {-2.0f, -0.3f, 0.9f},
    {-1.9f, -0.3f, 0.6f}, {-1.9f, 0.0f, 0.6f}, {1.7f, 0.0f, 1.425f},
    {1.7f, -0.66f, 1.425f}, {1.7f, -0.66f, 0.6f}, {1.7f, 0.0f, 0.6f},
    {2.6f, 0.0f, 1.425f}, {2.6f, -0.66f, 1.425f}, {3.1f, -0.66f, 0.825f},
    {3.1f, 0.0f, 0.825f}, {2.3f, 0.0f, 2.1f}, {2.3f, -0.25f, 2.1f},
    {2.4f, -0.25f, 2.025f}, {2.4f, 0.0f, 2.025f}, {2.7f, 0.0f, 2.4f},
    {2.7f, -0.25f, 2.4f}, {3.3f, -0.25f, 2.4f}, {3.3f, 0.0f, 2.4f}, {2.8f, 0.0f, 2.475f},
    {2.8f, -0.25f, 2.475f}, {3.525f, -0.25f, 2.49375f}, {3.525f, 0.0f, 2.49375f},
    {2.9f, 0.0f, 2.475f}, {2.9f, -0.15f, 2.475f}, {3.45f, -0.15f, 2.5125f},
    {3.45f, 0.0f, 2.5125f}, {2.8f, 0.0f, 2.4f}, {2.8f, -0.15f, 2.4f},
    {3.2f, -0.15f, 2.4f}, {3.2f, 0.0f, 2.4f}, {0.0f, 0.0f, 3.15f}, {0.8f, 0.0f, 3.15f},
    {0.8f, -0.45f, 3.15f}, {0.45f, -0.8f, 3.15f}, {0.0f, -0.8f, 3.15f},
    {0.0f, 0.0f, 2.85f}, {1.4f, 0.0f, 2.4f}, {1.4f, -0.784f, 2.4f}, {0.784f, -1.4f, 2.4f},
    {0.0f, -1.4f, 2.4f}, {0.4f, 0.0f, 2.55f}, {0.4f, -0.224f, 2.55f},
    {0.224f, -0.4f, 2.55f}, {0.0f, -0.4f, 2.55f}, {1.3f, 0.0f, 2.55f},
    {1.3f, -0.728f, 2.55f}, {0.728f, -1.3f, 2.55f}, {0.0f, -1.3f, 2.55f},
    {1.3f, 0.0f, 2.4f}, {1.3f, -0.728f, 2.4f}, {0.728f, -1.3f, 2.4f}, {0.0f, -1.3f, 2.4f},
    {0.0f, 0.0f, 0.0f}, {1.425f, -0.798f, 0.0f}, {1.5f, 0.0f, 0.075f},
    {1.425f, 0.0f, 0.0f}, {0.798f, -1.425f, 0.0f}, {0.0f, -1.5f, 0.075f},
    {0.0f, -1.425f, 0.0f}, {1.5f, -0.84f, 0.075f}, {0.84f, -1.5f, 0.075f},
};

}  // namespace teapot

#endif  // CSIM_TEAPOT_TEAPOT_DATA_H
