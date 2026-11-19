/*
** fxfarray.h - FARRAY, "float array" access by byte offset.
**
** FARRAY(p, i) is the float (an lvalue, so it can be read or assigned) that
** is i BYTES after the address p. It treats the memory at p as an array of
** floats and picks one element by its byte offset.
**
** Glide uses it for vertices, which are float arrays whose layout is only
** known at run time: the TSU data lists (gc->tsuDataList) hold byte offsets
** such as (GR_VERTEX_R_OFFSET << 2), so FARRAY(vertex, *dataList) reads
** the next parameter, and FARRAY(vertex, 4) is the Y coordinate.
**
** Because the offset is in bytes, it is added to a char pointer (adding it
** to a float pointer would scale it by 4). The pointer is never converted
** to an integer: casting it to int or long truncates it on 64-bit targets
** (long on 64-bit Windows, int everywhere).
**
** Note that the memory is read as float whatever its declared type, so the
** data must really be floats, suitably aligned.
*/
#ifndef __FXFARRAY_H__
#define __FXFARRAY_H__

#ifndef FARRAY
#define FARRAY(p,i)    (*(float *)((char *)(p) + (i)))
#endif

#endif /* __FXFARRAY_H__ */
