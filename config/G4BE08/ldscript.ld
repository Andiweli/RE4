__start = 0x80003100;
ENTRY(__start)

/* Each section starts at the next 32-byte boundary after the one before it, as in the original DOL.
   The matching build lands every section at its original address, and code that grows or shrinks
   moves everything after it instead of overlapping the next section. */
SECTIONS
{
  .init   0x80003100 : { *(.init) *(.init.*) }
  . = ALIGN(32);
  .text   : { *(.text) *(.text.*) }
  . = ALIGN(32);
  /* .ctor/.dtor: split objects (include the null terminators); .ctors/.dtors: compiled objects */
  .ctors  : { *(.ctor) *(.ctors) *(.ctors.*) *(.dtor) *(.dtors) *(.dtors.*) }
  . = ALIGN(32);
  .rodata : { *(.rodata) *(.rodata.*) }
  . = ALIGN(32);
  .data   : { *(.data) *(.data.*) }
  . = ALIGN(32);
  .bss    : { *(.bss) *(.bss.*) *(COMMON) }
  . = ALIGN(32);
  .sdata  : { *(.sdata) *(.sdata.*) }
  . = ALIGN(32);
  .sbss   : { *(.sbss) *(.sbss.*) }
  . = ALIGN(32);
  .sdata2 : { *(.sdata2) *(.sdata2.*) }
  . = ALIGN(32);

  /* The small-data bases, the 16 KB stack and the arena follow the sections (original values in the
     matching build: 0x8031BEE0, 0x8032BEE0, 0x80316CE0, 0x8031ACE0, 0x8031AD00). */
  /* crt0 clears .bss up to .sdata, and .sbss. */
  _bss_start = ADDR(.bss);
  _bss_end = ADDR(.sdata);
  _sbss_start = ADDR(.sbss);
  _sbss_end = ADDR(.sbss) + SIZEOF(.sbss);
  _SDA_BASE_ = ADDR(.sdata) + 0x8000;
  _SDA2_BASE_ = _SDA_BASE_ + 0x10000;
  _stack_end = .;
  _stack_addr = _stack_end + 0x4000;
  __ArenaLo = _stack_addr + 0x20;
  __ArenaHi = 0x81780000;
  /* GX write-gather FIFO (include/gx.h declares it as an extern object). */
  GXWGFifo = 0xCC008000;
  /* MWCC callee-saved FPR helpers: entry points inside _savefpr_14/_restfpr_14 (dct_ac DCT_AcInit saves f27..f31). */
  _savefpr_27 = _savefpr_14 + 0x34;
  _restfpr_27 = _restfpr_14 + 0x34;
}
