OBJS=main.obj pipe.obj
BIN=injectdll.exe
PDB=injectdll.pdb

# Yeap, "/Ge" is deprecated.
CFLAGS=/nologo /O2 /Wall /wd4054 /wd4055 /wd4255 /wd4668 /wd4710 /wd4711 \
	/wd4820 /wd4917 /MTd /GS /Zi

# You can build 64bit binary using "/MACHINE:X64" below.
LDFLAGS=/nologo /DEBUG /DYNAMICBASE /NXCOMPAT /PDB:$(PDB) \
	/SAFESEH /WX /MACHINE:X86 /DEFAULTLIB:Psapi.lib /DEFAULTLIB:Shlwapi.lib

$(BIN): $(OBJS)
	link $(OBJS) /OUT:$(BIN) $(LDFLAGS)

clean:
	del /F *.ilk *.obj *.exe *.pdb

