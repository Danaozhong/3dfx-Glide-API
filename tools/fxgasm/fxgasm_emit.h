/*
 * Output macros shared by the fxgasm generators (fxgasm.c of each Glide
 * variant). fxgasm.c produces two headers:
 *   - fxgasm.h   (main block): struct offsets for the NASM sources
 *   - fxinline.h (-inline block): offsets for C and inline assembly
 *
 * The offsets must match the ABI of the Glide DLL being built, so fxgasm.c
 * has to be compiled with exactly the same compiler, flags and defines as
 * the DLL. A target binary can not always be executed on the build machine
 * (e.g. MinGW cross builds), so with FXGASM_EMIT_SECTIONS the file is only
 * compiled: every output line is written at compile time into the object
 * sections ".fxgasm" and ".fxinl", from which cmake/GlideFxGasm.cmake
 * extracts the headers. Values are printed in decimal in that mode.
 *
 * Arguments that become part of the output text must be string literals.
 */
#ifndef FXGASM_EMIT_H
#define FXGASM_EMIT_H

#include <stddef.h>
#include <stdio.h>

#ifdef FXGASM_EMIT_SECTIONS

#if !defined(__GNUC__)
#error "FXGASM_EMIT_SECTIONS requires a GCC compatible compiler"
#endif

/* Each output section is terminated by this marker, everything after it
 * (e.g. section alignment padding) is discarded by the build. */
#define FXGASM_END_MARKER "@@FXGASM_END@@"

#if defined(_WIN32)
/* COFF: no section stack, so return to .text explicitly. This requires the
 * file to be compiled with -O0 and without -ffunction-sections. */
#define FXGASM_SECTION_BEGIN(sect) ".section " sect ",\"dr\"\n\t"
#define FXGASM_SECTION_END         "\n\t.text"
#else
#define FXGASM_SECTION_BEGIN(sect) ".pushsection " sect ",\"\",@progbits\n\t"
#define FXGASM_SECTION_END         "\n\t.popsection"
#endif

#define FXGASM_EMIT_TEXT(sect, text) \
  __asm__ __volatile__ (FXGASM_SECTION_BEGIN(sect) \
                        ".ascii \"" text "\"" FXGASM_SECTION_END)

#define FXGASM_EMIT_VALUE(sect, prefix, value, suffix) \
  __asm__ __volatile__ (FXGASM_SECTION_BEGIN(sect) \
                        ".ascii \"" prefix "%c0" suffix "\"" FXGASM_SECTION_END \
                        : : "i" (value))

#define NEWLINE           FXGASM_EMIT_TEXT(".fxgasm", "\\n")
#define COMMENT           FXGASM_EMIT_TEXT(".fxgasm", \
  ";----------------------------------------------------------------------\\n")
#define HEADER(str)       NEWLINE; COMMENT; \
  FXGASM_EMIT_TEXT(".fxgasm", "; Assembler offsets for " str " struct\\n"); \
  COMMENT; NEWLINE
#define OFFSET(p,o,pname) FXGASM_EMIT_VALUE(".fxgasm", pname "\tequ ", \
                                            offsetof(__typeof__(p), o), "\\n")
#define SIZEOF(p,pname)   FXGASM_EMIT_VALUE(".fxgasm", "SIZEOF_" pname "\tequ ", \
                                            sizeof(p), "\\n")
#define FXGASM_DONE()     FXGASM_EMIT_TEXT(".fxgasm", FXGASM_END_MARKER)

#define INLINE_TEXT(str)  FXGASM_EMIT_TEXT(".fxinl", str "\\n")
#define INLINE_DEFINE(name, value, suffix) \
  FXGASM_EMIT_VALUE(".fxinl", "#define " name " ", value, suffix "\\n")
#define INLINE_DONE()     FXGASM_EMIT_TEXT(".fxinl", FXGASM_END_MARKER)

#else /* !FXGASM_EMIT_SECTIONS */

#define NEWLINE printf("\n")
#define COMMENT printf(";----------------------------------------------------------------------\n")

#define HEADER(str)     NEWLINE; COMMENT; \
                        printf("; Assembler offsets for %s struct\n",str);\
                        COMMENT; NEWLINE

#define FXGASM_OFFSET_OF(p,o) ((unsigned long)((char*)&p.o - (char*)&p))

#define OFFSET(p,o,pname) if (hex) \
        printf("%s\tequ %08lxh\n",pname,FXGASM_OFFSET_OF(p,o)); \
    else printf("%s\tequ %10lu\n",pname,FXGASM_OFFSET_OF(p,o))

#define SIZEOF(p,pname) if (hex) \
        printf("SIZEOF_%s\tequ %08lxh\n",pname,(unsigned long)sizeof(p)); \
    else printf("SIZEOF_%s\tequ %10lu\n",pname,(unsigned long)sizeof(p))

#define FXGASM_DONE()

#define INLINE_TEXT(str) printf("%s\n", str)
#define INLINE_DEFINE(name, value, suffix) \
  printf("#define %s 0x%lX%s\n", name, (unsigned long)(value), suffix)
#define INLINE_DONE()

#endif /* !FXGASM_EMIT_SECTIONS */

#endif /* FXGASM_EMIT_H */
