noinst_PROGRAMS = renderer showShadowMap

common_SRC = \
    3d.h Algebra.h Base3d.h Camera.h Clock.h Defines.h Exceptions.h \
    Keyboard.h Light.h Scene.h Screen.h Types.h Camera.cc \
    Keyboard.cc Light.cc Rasterizers.cc Screen.cc ScanConverter.h \
    Fillers.h LightingEq.h Base3d.cc Wu.h Wu.cc HelpKeys.h \
    OnlineHelpKeys.h BVH.h BVH.cc Loader.cc Raytracer.cc 
    
renderer_SOURCES = renderer.cc ${common_SRC}
if MLAA_ENABLED
renderer_SOURCES += MLAA.h MLAA.cc
endif

renderer_CPPFLAGS = @SDL_CFLAGS@ -I$(srcdir)/../lib3ds-1.3.0/

renderer_LDADD = @SDL_LIBS@ @TBB_LIBS@ @OPENMP_LIBS@ ../lib3ds-1.3.0/lib3ds/.libs/lib3ds.a @MINGWSTACK@ @WHOLEPROGRAM@

showShadowMap_SOURCES = showShadowMap.cc Keyboard.h Keyboard.cc

showShadowMap_CPPFLAGS = @SDL_CFLAGS@

showShadowMap_LDADD = @SDL_LIBS@

bench:
	@for i in 1 2 3 4 5 ; do SDL_VIDEODRIVER=dummy ./renderer -b -n 500 ../3D-Objects/trainColor.tri | tail -1 | awk '{print substr($$(NF-1),2);}' ; done | perl -e '$$total=0; $$totalSq=0; $$n=0; my @allOfThem; while(<>) { print; chomp; $$total += $$_; $$totalSq += $$_*$$_; $$n++; push @allOfThem, $$_; } my $$variance = ($$totalSq - $$total*$$total/$$n)/($$n-1); my @srted = sort {$$a <=> $$b} @allOfThem; my $$len = scalar(@allOfThem); if ($$len % 2) { $$len++; } my @measurements = ( ["Average value",$$total/$$n], ["Std deviation",sqrt($$variance)], ["Median",$$srted[-1 + $$len/2]], ["Min",$$srted[0]], ["Max",$$srted[-1]]); foreach (@measurements) { printf("%*s: %f\n", 15, $$_->[0], $$_->[1]);}'
