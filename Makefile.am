BUILT_SOURCES = script-lexer.c script-parser.c
bin_PROGRAMS = bm-watch-subdirs bm-fbx-convert bm-script-convert bm-texture-convert
noinst_LIBRARIES = libPVRTools.a

AM_CPPFLAGS = -Ifbx/include -IPVRTC -IPVRTexLib -IPVRTools -IPVRTools/OGLES2

AM_YFLAGS = -d
AM_CFLAGS = -g -Wall -pthread -std=c99
AM_CXXFLAGS = -g -Wall -pthread -std=c++0x

bm_watch_subdirs_SOURCES = watch-subdirs.c 
 
bm_fbx_convert_CXXFLAGS = $(AM_CXXFLAGS) -Wno-reorder -Wno-sign-compare -Wno-strict-aliasing
bm_fbx_convert_SOURCES = \
  fbx-convert.cc \
  fbx-convert-binary.cc \
  fbx-convert-json.c
bm_fbx_convert_LDADD = -Lfbx/lib/gcc4/x64 -L. -ldl -lfbxsdk-2013.1-static -lPVRTools

bm_script_convert_SOURCES = \
  array.c array.h \
  arena.c arena.h \
  script-lexer.l \
  script-parser.y \
  script-optimize.cc \
  script-binary.cc \
  script-html.c \
  script.c

bm_texture_convert_SOURCES = \
  texture-convert.cc \
  png-wrapper.c
bm_texture_convert_LDADD = -lpng -LPVRTC/Linux_x86_64 -LPVRTexLib/Linux_x86_64 -lPVRTC -lPVRTexLib

libPVRTools_a_SOURCES = \
  PVRTools/PVRTTriStrip.cpp PVRTools/PVRTTriStrip.h

clean-local:
	rm -f $(BUILT_SOURCES)
