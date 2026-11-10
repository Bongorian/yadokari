// Original host-only ASCII conversion for boot filenames and test assets.
// Non-ASCII paths are rejected; this is not a Unicode filesystem emulator.
#include "ff.h"
WCHAR ff_oem2uni(WCHAR c,WORD page){(void)page;return c<128?c:0;}
WCHAR ff_uni2oem(DWORD c,WORD page){(void)page;return c<128?(WCHAR)c:0;}
DWORD ff_wtoupper(DWORD c){return c>='a'&&c<='z'?c-'a'+'A':c;}
