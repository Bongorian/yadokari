# FatFs host simulator

FatFs R0.16 (ChaN), copied from Circle commit
7a81e1b480fe7d19fc1fc695b89bff0a0848f44b `addon/fatfs/`.
The permissive license is retained at the top of ff.c, ff.h and diskio.h.
ff.c/ff.h/diskio.h are unchanged. ffconf.h enables mkfs and 4 explicitly mapped
MBR partitions, and enables stack-based long names with host-only ASCII conversion, disabling RTC and reentrant OS locks for the
single-threaded host SD simulator. The firmware uses Circle's normal FatFs
with its long names and task locks; it never enables runtime formatting.
