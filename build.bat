@echo off
del *.pdb > NUL 2> NUL
del *.rdi > NUL 2> NUL
cl hexdump.c -nologo -Fe:hexdump.exe -Z7 -link -incremental:no -opt:ref
del *.ilk > NUL 2> NUL
del *.obj > NUL 2> NUL
