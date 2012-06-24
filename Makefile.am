BUILT_SOURCES = script-lexer.c script-parser.c
bin_PROGRAMS = bm-watch-subdirs bm-fbx-convert bm-script-convert

AM_CPPFLAGS = -Ifbx/include

AM_YFLAGS = -d
AM_CFLAGS = -g -Wall -pthread -std=c99
AM_CXXFLAGS = -g -Wall -pthread -std=c++0x

bm_watch_subdirs_SOURCES = watch-subdirs.c 
 
bm_fbx_convert_CXXFLAGS = $(AM_CXXFLAGS) -Wno-reorder -Wno-sign-compare -Wno-strict-aliasing
bm_fbx_convert_SOURCES = \
  fbx-convert.cc \
  fbx-convert-binary.c \
  fbx-convert-json.c
bm_fbx_convert_LDADD = -Lfbx/lib/gcc4/x64 -ldl -lfbxsdk-2013.1-static

bm_script_convert_SOURCES = \
  array.c array.h \
  arena.c arena.h \
  script-lexer.l \
  script-parser.y \
  script-binary.c \
  script-html.c \
  script.c

clean-local:
	rm -f $(BUILT_SOURCES)
