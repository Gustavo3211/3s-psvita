BUILD:

$ mkdir build
$ cd build
$ make

you'll need SF33RD.AFS (3sx extracts it from the iso), extract the files (AFSPacker is what i use), and place them in a folder named "resources" next to the EBOOT

CONFIGS:
-press start at boot to disable backgrounds

TO-DO:

src/Game/texcash.c:
-modify MTSBase mts_base, life16 and life32 so it doesn't slow down with certain animations

src/Game/PPGFile.c:
-remove and fix the u * tex->width, v * tex->height
-fix ppgWriteQuadUseTrans

src/Game/menu.c:
-SaveMove()
-SaveInit(,)

src/Game/Sound3rd.c:
-it's sound, it has to be ported with care, specificaly adxt stuff
-BGM_Server()
