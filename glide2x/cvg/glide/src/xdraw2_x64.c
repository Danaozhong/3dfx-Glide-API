
#include <memory.h>

#include <3dfx.h>

#define FX_DLL_DEFINITION
#include <fxdll.h>
#include <glide.h>
#include "fxglide.h"

#if GLIDE_DISPATCH_SETUP
#include "fxinline.h"
#endif


/*-------------------------------------------------------------------
  Function: _vptrisetup_cull
  Date: 13-Sep-2021
  Implementor(s): Clemens Zangl
  Description:
  Draw triangles
  Arguments:
    va, vb, vb: array of vertices for each triangle.
  Return:
  -------------------------------------------------------------------*/
#ifdef GLIDE_USE_C_TRISETUP
FxI32 FX_CSTYLE
trisetup_cull(const void *va, const void *vb, const void *vc)
{
#define FN_NAME "_vptrisetup_cull"
  GR_BEGIN_NOFIFOCHECK(FN_NAME, 90);
  {


#if 0 /* Clemens this is only used in Glide3 */
    float oow[3];
#endif
    register float oowa;

#if 0 /* Clemens this is only used in Glide3 */
    oow[0] = 1.0f / FARRAY(va, gc->state.vData.wInfo.offset);
#endif
    /*
     * first cut of clip space coordinate code, no optimization.
     */
    

    {
      GR_FLUSH_STATE();
      
      GR_SET_EXPECTED_SIZE(_GlideRoot.curTriSize, 1);
      TRI_STRIP_BEGIN(kSetupStrip, 3, gc->state.vData.vSize, SSTCP_PKT3_BDDBDD);
      
      /* Send x, y of the three vertices. This is unrolled instead of looping
         over an array of the vertex pointers: the vertices are passed in
         registers on x64, so there is no array to walk from &va, and
         building one would only add stores. */
#define SEND_VERTEX_XY(__v) \
      do { \
        TRI_SETF(FARRAY((__v), 0)); \
        TRI_SETF(FARRAY((__v), 4)); \
      } while (0)
      SEND_VERTEX_XY(va);
      SEND_VERTEX_XY(vb);
      SEND_VERTEX_XY(vc);
#undef SEND_VERTEX_XY
      TRI_END;
      GR_CHECK_SIZE();
    }
  }

  _GlideRoot.stats.trisProcessed++;
  return 1;

#undef FN_NAME
} /* _vptrisetup_cull */
#endif


/**
 **  _trisetup_nogradients
 **
 **  This routine does all the setup needed for drawing a triangle.  It
 **  is intended to be an exact specification for the mechanisim used
 **  to pass vertices to the assembly language triangle setup code, and
 **  as such has no optimizations at all.  Whenever a 'shortcut'
 **  routine to draw triangles (such as for antialiasing, fast lines,
 **  fast spans, polygons, etc) is needed, this code should be used as
 **  the starting point.
 **
 */
GR_DDFUNC(trisetup,
          FxI32,
          (const void *va, const void *vb, const void *vc))
{
#define FN_NAME "_trisetup_nogradients"
  GR_DCL_GC;
  //const FxI32 xindex = (gc->state.vData.vertexInfo.offset >> 2);
  //const FxI32 yindex = xindex + 1;

  _GlideRoot.stats.trisProcessed++;

#if 0 //GLIDE_TRI_CULLING
  {
      const float* fa = (const float*)va; // +xindex;
      const float* fb = (const float*)vb; // +xindex;
      const float* fc = (const float*)vc; // +xindex;
    float dxAB, dxBC, dyAB, dyBC;
    
    /* Compute Area */
    dxAB = fa[xindex] - fb[xindex];
    dxBC = fb[xindex] - fc[xindex];
    
    dyAB = fa[yindex] - fb[yindex];
    dyBC = fb[yindex] - fc[yindex];
    
    /* Stash the area in the float pool for easy access */
    _GlideRoot.pool.temp1.f = dxAB * dyBC - dxBC * dyAB;
    
#define FloatVal(__f) (((__f) < 786432.875) ? (__f) : ((__f) - 786432.875))
    {
      const FxU32 culltest = (gc->state.cull_mode << 31UL);
      
      /* Zero-area triangles are BAD!! */
      if ((_GlideRoot.pool.temp1.i & 0x7FFFFFFF) == 0) {
        GDBG_INFO(291, FN_NAME": Culling (%g %g) (%g %g) (%g %g) : (%g : 0x%X : 0x%X)\n",
                  FloatVal(fa[0]), FloatVal(fa[1]), 
                  FloatVal(fb[0]), FloatVal(fb[1]), 
                  FloatVal(fc[0]), FloatVal(fc[1]), 
                  _GlideRoot.pool.temp1.f, gc->state.cull_mode, culltest);
        
        return 0;
      }
      
      /* Backface culling, use sign bit as test */
      if ((gc->state.cull_mode != GR_CULL_DISABLE) &&
          (((FxI32)(_GlideRoot.pool.temp1.i ^ culltest)) >= 0)) {
        GDBG_INFO(291, FN_NAME": Culling (%g %g) (%g %g) (%g %g) : (%g : 0x%X : 0x%X)\n",
                  FloatVal(fa[0]), FloatVal(fa[1]), 
                  FloatVal(fb[0]), FloatVal(fb[1]), 
                  FloatVal(fc[0]), FloatVal(fc[1]), 
                  _GlideRoot.pool.temp1.f, gc->state.cull_mode, culltest);
        
        return -1;
      }
    }
  }
#endif /* GLIDE_TRI_CULLING */

  /* Stuff for using hw tsu */
#if GLIDE_HW_TRI_SETUP
  {
    const float* vectorArray[3];

    /* Load up the real vertices */
    vectorArray[0] = (const float*)va; // +xindex;
    vectorArray[1] = (const float*)vb; // +xindex;
    vectorArray[2] = (const float*)vc; // +xindex;

#if GLIDE_PACKET3_TRI_SETUP
    GR_SET_EXPECTED_SIZE(_GlideRoot.curTriSize, 1);
    TRI_BEGIN();
    {
      int vectorIndex;

      for(vectorIndex = 0; vectorIndex < ((int)sizeof(vectorArray) / (int)sizeof(float*)); vectorIndex++) {
        const float* const vector = vectorArray[vectorIndex];
      
        /* Triangle vertex coordinages (x, y) */
        TRI_SETF(vector[0]);
        TRI_SETF(vector[0]);
      
        /* Other triangle parameters */
        {
          const int* dataList = gc->tsuDataList;

#if GLIDE_PACKED_RGB
          {
            FxBool doColorP = FXFALSE;
            FxU32 packedColor = 0x00;
            
            if (*dataList == (GR_VERTEX_R_OFFSET << 2)) {
              packedColor = (RGBA_COMP_CLAMP(FARRAY(vector, (GR_VERTEX_B_OFFSET << 2)), B) | 
                             RGBA_COMP_CLAMP(FARRAY(vector, (GR_VERTEX_G_OFFSET << 2)), G) |
                             RGBA_COMP_CLAMP(FARRAY(vector, (GR_VERTEX_R_OFFSET << 2)), R));
              dataList++;
              doColorP = FXTRUE;
            }

            if (*dataList == (GR_VERTEX_A_OFFSET << 2)) {
              packedColor |= RGBA_COMP_CLAMP(FARRAY(vector, (GR_VERTEX_A_OFFSET << 2)), A);
              dataList++;
              doColorP = FXTRUE;
            }
            
            if (doColorP) TRI_SET(packedColor);
          }
#endif /* GLIDE_PACKED_RGB */

#if GLIDE_FP_CLAMP_TEX
          while(*dataList != 0) {
            TRI_SETF(FARRAY(vector, *dataList));
            dataList++;
          }

          dataList++;
#endif /* GLIDE_FP_CLAMP_TEX */
         
          while(*dataList != 0) {
            TRI_SETF_CLAMP(FARRAY(vector, *dataList));
            dataList++;
          }
        }         
      }
    }
    TRI_END;
    GR_CHECK_SIZE();
#else /* !GLIDE_PACKET3_TRI_SETUP */
    {
      GR_DCL_HW;
      int vectorIndex;
      FxU32 sMode = (gc->cmdTransportInfo.paramMask >> SSTCP_PKT3_PMASK_SHIFT);
      FxU32 paramMask = (sMode & 0xFF);
      FxU32 paramCount;

      /* Convert packet 3 paramMask into sMode format */
      sMode = (paramMask | ((sMode & 0xF000) << 4));

      {
        const FxBool hasColor = ((sMode & 0x01) != 0);
        const FxBool hasAlpha = ((sMode & 0x02) != 0);
        const FxBool hasZ = ((sMode & 0x04) != 0);
        const FxBool hasWb = ((sMode & 0x08) != 0);
        const FxBool hasW0 = ((sMode & 0x10) != 0);
        const FxBool hasST0 = ((sMode & 0x20) != 0);
        const FxBool hasW1 = ((sMode & 0x40) != 0);
        const FxBool hasST1 = ((sMode & 0x80) != 0);

        /* We always send vertex XY */
        paramCount = 2;
        paramMask = 0x03;

        /* Build parameter data for reg group packet */
#if GLIDE_PACKED_RGB
        if (hasColor || hasAlpha) {
          paramCount += 1;
          paramMask |= 0x04;
        }
#else /* !GLIDE_PACKED_RGB */
        if (hasColor) {
          paramCount += 3;
          paramMask |= 0x38;
        }
        if (hasAlpha) {
          paramCount += 1;
          paramMask |= 0x40;
        }
#endif /* !GLIDE_PACKED_RGB */

        if (hasZ) {
          paramCount += 1;
          paramMask |= 0x80;
        }
        if (hasWb) {
          paramCount += 1;
          paramMask |= 0x100;
        }
        if (hasW0) {
          paramCount += 1;
          paramMask |= 0x200;
        }
        if (hasST0) {
          paramCount += 2;
          paramMask |= 0xC00;
        }
        if (hasW1) {
          paramCount += 1;
          paramMask |= 0x1000;
        }
        if (hasST1) {
          paramCount += 2;
          paramMask |= 0x6000;
        }

        /* Set mode once for teh whole triangle */
        GR_SET_EXPECTED_SIZE(sizeof(FxU32), 1);
        GR_SET(BROADCAST_ID, hw, sSetupMode, sMode);
        GR_CHECK_SIZE();
        
        for(vectorIndex = 0; vectorIndex < sizeof(vectorArray) / sizeof(float*); vectorIndex++) {
          const GrVertex* curVertex = (const GrVertex*)vectorArray[vectorIndex];
          
          REG_GROUP_BEGIN(BROADCAST_ID, sVx, paramCount, paramMask);
          {
            REG_GROUP_SETF(hw, sVx, curVertex->x);
            REG_GROUP_SETF(hw, sVy, curVertex->y);

#if GLIDE_PACKED_RGB
            if (hasColor || hasAlpha) {
              FxU32 packedVal = 0x00;

              if (hasColor) packedVal = ((RGBA_COMP_CLAMP(curVertex->b) << 0UL) |
                                         (RGBA_COMP_CLAMP(curVertex->g) << 8UL) |
                                         (RGBA_COMP_CLAMP(curVertex->r) << 16UL));
              if (hasAlpha) packedVal |= (RGBA_COMP_CLAMP(curVertex->a) << 24UL);
              
              REG_GROUP_SET(hw, sARGB, packedVal);
            }
#else /* !GLIDE_PACKED_RGB */
            if (hasColor) {
              REG_GROUP_SETF(hw, sRed, curVertex->r);
              REG_GROUP_SETF(hw, sGreen, curVertex->g);
              REG_GROUP_SETF(hw, sBlue, curVertex->b);
            }
            if (hasAlpha) REG_GROUP_SETF(hw, sAlpha, curVertex->a);
#endif /* !GLIDE_PACKED_RGB */

            if (hasZ) REG_GROUP_SETF(hw, sVz, curVertex->ooz);
            if (hasWb) REG_GROUP_SETF(hw, sOowfbi, curVertex->oow);

            /* TMU0 */
            if (hasW0) REG_GROUP_SETF(hw, sOow0, curVertex->tmuvtx[0].oow);
            if (hasST0) {
              REG_GROUP_SETF(hw, sSow0, curVertex->tmuvtx[0].sow);
              REG_GROUP_SETF(hw, sTow0, curVertex->tmuvtx[0].tow);
            }

            /* TMU1 */
            if (hasW1) REG_GROUP_SETF(hw, sOow1, curVertex->tmuvtx[1].oow);
            if (hasST1) {
              REG_GROUP_SETF(hw, sSow1, curVertex->tmuvtx[1].sow);
              REG_GROUP_SETF(hw, sTow1, curVertex->tmuvtx[1].tow);
            }
          }
          REG_GROUP_END();

          GR_SET_EXPECTED_SIZE(sizeof(FxU32), 1);
          if (vectorIndex == 0) {
            GR_SET(BROADCAST_ID, hw, sBeginTriCMD, 0);
          } else {
            GR_SET(BROADCAST_ID, hw, sDrawTriCMD, 0);
          }
          GR_CHECK_SIZE();
        }
      }
    }
#endif /* !GLIDE_PACKET3_TRI_SETUP */

    goto __triDrawn;
  }
#endif /* GLIDE_HW_TRI_SETUP */

#if !GLIDE_HW_TRI_SETUP
#if !defined(GLIDE_TRI_CULLING) || !GLIDE_TRI_CULLING
#error "GLIDE_TRI_CULLING must be enabled to sort vertices"
#endif

  {
    GR_DCL_HW;

    TRI_NO_TSU_BEGIN(FXTRUE);
    {
      const float ooa = _GlideRoot.pool.f1 / _GlideRoot.pool.ftemp1;
      volatile FxU32* hwAddr = &hw->FvA.x;
            
      /* Divide the deltas by the area for gradient calculation. */
      dxBC *= ooa;
      dyAB *= ooa;
      dxAB *= ooa;
      dyBC *= ooa;
	    
      GDBG_INFO(85, FN_NAME": No-TSU Triangle area: (%g %g) : (%g %g %g %g)\n",
                _GlideRoot.pool.ftemp1, ooa,
                dxAB, dxBC, dyAB, dyBC);
      
      /* write out X & Y for vertex A */  
      TRI_NO_TSU_SETF(hwAddr++, fa[GR_VERTEX_X_OFFSET]);
      TRI_NO_TSU_SETF(hwAddr++, fa[GR_VERTEX_Y_OFFSET]);

      /* write out X & Y for vertex B */
      TRI_NO_TSU_SETF(hwAddr++, fb[GR_VERTEX_X_OFFSET]);
      TRI_NO_TSU_SETF(hwAddr++, fb[GR_VERTEX_Y_OFFSET]);  
  
      /* write out X & Y for vertex C */
      TRI_NO_TSU_SETF(hwAddr++, fc[GR_VERTEX_X_OFFSET]);
      TRI_NO_TSU_SETF(hwAddr++, fc[GR_VERTEX_Y_OFFSET]);

      /* Send down the starting parameter values, these come from
       * the sorted vertex A.
       */
      {
        int i = GR_VERTEX_R_OFFSET;

        while(i < GR_VERTEX_OOW_OFFSET) {
          TRI_NO_TSU_SETF(hwAddr++, fa[i++]);
        }

        /* Skip fbi oow */
        i++;

        /* FixMe? We currently only do one tmu. */
        while(i < GR_VERTEX_OOW_TMU0_OFFSET) {
          TRI_NO_TSU_SETF(hwAddr++, fa[i]);
          i++;
        }

        TRI_NO_TSU_SETF(hwAddr++, fa[GR_VERTEX_OOW_OFFSET]);
      }

      /* Send the x gradients */
      {
        int i = GR_VERTEX_R_OFFSET;

        while(i < GR_VERTEX_OOW_OFFSET) {
          const float curGrad = (((fa[i] - fb[i]) * dyBC) -
                                 ((fb[i] - fc[i]) * dyAB));
		    
          GDBG_INFO(285, FN_NAME": Gradient 0x%X : %g %g %g : %g\n",
                    ((FxU32)hwAddr - (FxU32)hw) >> 2,
                    fa[i], fb[i], fc[i], curGrad);
		    
          TRI_NO_TSU_SETF(hwAddr++, curGrad);
          i++;
        }

        /* Skip fbi oow */
        i++; 

        /* FixMe? We currently only do one tmu. */
        while(i < GR_VERTEX_OOW_TMU0_OFFSET) {
          const float curGrad = (((fa[i] - fb[i]) * dyBC) -
                                 ((fb[i] - fc[i]) * dyAB));
		    
          GDBG_INFO(285, FN_NAME": Gradient 0x%X : %g %g %g : %g\n",
                    ((FxU32)hwAddr - (FxU32)hw) >> 2,
                    fa[i], fb[i], fc[i], curGrad);
		    
          TRI_NO_TSU_SETF(hwAddr++, curGrad);
          i++;
        }

        {
          const float curGrad = (((fa[GR_VERTEX_OOW_OFFSET] - fb[GR_VERTEX_OOW_OFFSET]) * dyBC) -
                                 ((fb[GR_VERTEX_OOW_OFFSET] - fc[GR_VERTEX_OOW_OFFSET]) * dyAB));
                    
          TRI_NO_TSU_SETF(hwAddr++, curGrad);
        }
      }

      /* Send the y gradients */
      {
        int i = GR_VERTEX_R_OFFSET;

        while(i < GR_VERTEX_OOW_OFFSET) {
          const float curGrad = (((fb[i] - fc[i]) * dxAB) -
                                 ((fa[i] - fb[i]) * dxBC));
		    
          GDBG_INFO(285, FN_NAME": Gradient 0x%X : %g %g %g : %g\n",
                    ((FxU32)hwAddr - (FxU32)hw) >> 2,
                    fa[i], fb[i], fc[i], curGrad);

          TRI_NO_TSU_SETF(hwAddr++, curGrad);
          i++;
        }

        /* Skip fbi oow */
        i++;

        /* FixMe? We currently only do one tmu. */
        while(i < GR_VERTEX_OOW_TMU0_OFFSET) {
          const float curGrad = (((fb[i] - fc[i]) * dxAB) -
                                 ((fa[i] - fb[i]) * dxBC));
		    
          GDBG_INFO(285, FN_NAME": Gradient 0x%X : %g %g %g : %g\n",
                    ((FxU32)hwAddr - (FxU32)hw) >> 2,
                    fa[i], fb[i], fc[i], curGrad);
		    
          TRI_NO_TSU_SETF(hwAddr++, curGrad);
          i++;
        }

        {
          const float curGrad = (((fb[GR_VERTEX_OOW_OFFSET] - fc[GR_VERTEX_OOW_OFFSET]) * dxAB) -
                                 ((fa[GR_VERTEX_OOW_OFFSET] - fb[GR_VERTEX_OOW_OFFSET]) * dxBC));
                    
          TRI_NO_TSU_SETF(hwAddr++, curGrad);
        }                
      }      
	  
      /* Draw the triangle by writing the area to the triangleCMD register */
      TRI_NO_TSU_SETF(hwAddr, _GlideRoot.pool.ftemp1);
      _GlideRoot.stats.trisDrawn++;
    }
    TRI_NO_TSU_END();
  }

  goto __triDrawn;
#endif /* !GLIDE_HW_TRI_SETUP */

__triDrawn:
  /* If we made it this far then we drew the triangle */
  _GlideRoot.stats.trisDrawn++;
  GR_CHECK_SIZE();

  return 1;
    
#undef FN_NAME
} /* _trisetup_nogradients */



FxI32 FX_CSTYLE trisetup_rgb(const void* va, const void* vb, const void* vc)
{
    return trisetup_cull(va, vb, vc);
}

FxI32 FX_CSTYLE trisetup_cull_rgb(const void* va, const void* vb, const void* vc)
{
    return trisetup_cull(va, vb, vc);
}

FxI32 FX_CSTYLE trisetup_argb(const void* va, const void* vb, const void* vc)
{
    return trisetup_cull(va, vb, vc);
}

FxI32 FX_CSTYLE trisetup_cull_argb(const void* va, const void* vb, const void* vc)
{
    return trisetup_cull(va, vb, vc);
}
