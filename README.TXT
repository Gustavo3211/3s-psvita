BUILD:

$ mkdir build
$ cd build
$ make

then extract the files on SF33RD.AFS and place them in resources/ next to the EBOOT

CONFIGS:
-press start at boot to disable backgrounds
-press select at boot to enable experimental background cashing
-press up at boot for 30fps mode

TO-DO:

src/Game/PPGFile.c:
-remove and fix the u * tex->width, v * tex->height

src/Game/menu.c:
-SaveMove()
-SaveInit(,)

src/Game/Sound3rd.c:
-it's sound, it has to be ported with care, specificaly adxt stuff
-BGM_Server()
