AC_INIT([renderer], [2.3f], [ttsiodras@gmail.com])

AC_CONFIG_HEADERS([src/config.h])

AC_CONFIG_MACRO_DIR([ac-macros])

# Don't clutter this dir, store in build-aux
AC_CONFIG_AUX_DIR([build-aux])

AC_ARG_ENABLE(tbb,
    [AS_HELP_STRING([--enable-tbb],[use Intel Threading Building Blocks if available (automatic, use --disable-tbb to disable it)])],
    [enable_tbb="$enableval"],
    [enable_tbb="yes"])

AC_ARG_ENABLE(openmp,
    [AS_HELP_STRING([--enable-openmp],[use OpenMP Multithreading if available (automatic, use --disable-openmp to disable it)])],
    [enable_openmp="$enableval"],
    [enable_openmp="yes"])

AC_ARG_ENABLE(brakes,
    [AS_HELP_STRING([--enable-brakes],[Handle the raytracing mode with freeze-frame (enabled by default, use --disable-brakes to disable it)])],
    [enable_raytracer_brakes="$enableval"],
    [enable_raytracer_brakes="yes"])

AC_ARG_ENABLE(mlaa,
    [AS_HELP_STRING([--enable-mlaa],[Use Intel SSE implementation of MLAA (disabled by default, use --enable-mlaa to enable it)])],
    [enable_mlaa="$enableval"],
    [enable_mlaa="no"])

if test x"${enable_mlaa}" = xyes ; then
    AC_DEFINE([MLAA_ENABLED], 1, [Define this to use Intel SSE implementation of MLAA.])
else
    AC_MSG_NOTICE([************************************************************])
    AC_MSG_NOTICE([** Intel MLAA antialiasing is disabled! use --enable-mlaa **])
    AC_MSG_NOTICE([** (provided your CPU has SSE2 instructions...            **])
    AC_MSG_NOTICE([************************************************************])
fi
AM_CONDITIONAL([MLAA_ENABLED], [test x"${enable_mlaa}" = xyes])

# Detect the canonical host and target build environment
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([-Wall -Werror])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

if test x"${CXXFLAGS}" = x ; then
# We are using templates that rely on compile-time constant elimination, use O3
    CXXFLAGS="-O3 -g -Wall"
fi

AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB

AC_LANG(C++)

# Check for SDL (minimum: 1.2.0)
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION, :,
            AC_MSG_ERROR([*** SDL version $SDL_VERSION or later was not found!]))
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)

# Check for Intel Thread Building Blocks (unless not desired)
TBB_LIBS=""
if test x"${enable_tbb}" = xyes ; then
    AC_MSG_CHECKING(for Intel TBB support)
    HAVE_TBB=no
    AC_TRY_COMPILE([
     #include "tbb/blocked_range.h"
     #include "tbb/parallel_for.h"
    ],[
    ],[
    HAVE_TBB=yes
    ])
    AC_MSG_RESULT($HAVE_TBB)
else
    AC_MSG_NOTICE([Instructed to disable TBB. Disabled...])
fi

UNDERMINGW=no
MINGWSTACK=""
AC_MSG_CHECKING(for stingy MS stack under MinGW)
case ${host_os} in
  *mingw*)
     MINGWSTACK="-Wl,--stack,16777216"
     UNDERMINGW=yes
     CXXFLAGS="$CXXFLAGS -mstackrealign"
  ;;
esac
AC_MSG_RESULT($UNDERMINGW)
AC_SUBST(MINGWSTACK)

# Check for OpenMP (unless not desired)
OPENMP_CFLAGS=""
OPENMP_LIBS=""
HAVE_OPENMP=no
if test x"${enable_openmp}" = xyes ; then
    AX_OPENMP([HAVE_OPENMP=yes])
else
    AC_MSG_NOTICE([Instructed to disable OpenMP. Disabled...])
fi
if test x"${HAVE_OPENMP}" = xyes ; then
    OPENMP_LIBS=${OPENMP_CXXFLAGS}
fi

if test x"${enable_raytracer_brakes}" = xyes ; then
    AC_DEFINE([HANDLERAYTRACER], 1, [Define this to use freeze-frame in raytracing mode .])
else
    AC_MSG_NOTICE([Instructed to disable special handling of raytracer mode...])
fi

if test x"${HAVE_OPENMP}" = xyes ; then
    AC_MSG_CHECKING(for GCC maturity in OpenMP support)
    AC_RUN_IFELSE([AC_LANG_SOURCE([#include <stdlib.h>
int main(void) {
#if defined(__GNUC__)
#if __GNUC__ < 4 || ((__GNUC__ == 4) && (__GNUC_MINOR__ < 3))
    return -1;
#endif
#endif
    return 0;
}
    ])], [GOODGCC=yes], [GOODGCC=no])
    if test x"${GOODGCC}" = xyes ; then
	AC_MSG_RESULT($GOODGCC)
	AC_DEFINE([USE_OPENMP], 1, [Define this to use OpenMP.])
	CPPFLAGS="$CPPFLAGS $OPENMP_CXXFLAGS"
	AC_SUBST(OPENMP_LIBS)
    else
	AC_MSG_RESULT($GOODGCC)
	AC_MSG_NOTICE([**********************************************************])
	AC_MSG_NOTICE([** You have an old version of GCC, that has OpenMP bugs **])
	AC_MSG_NOTICE([**        OpenMP disabled to avoid crashes...           **])
	AC_MSG_NOTICE([**********************************************************])
	HAVE_OPENMP=no
    fi
fi

if test x"${HAVE_OPENMP}" = xyes ; then
    if test x"${HAVE_TBB}" = xyes ; then
	AC_MSG_NOTICE([**********************************************************])
	AC_MSG_NOTICE([*** You have both OpenMP and TBB, using only OpenMP... ***])
	AC_MSG_NOTICE([***      (if you prefer TBB, use --disable-openmp)     ***])
	AC_MSG_NOTICE([**********************************************************])
	HAVE_TBB=no
    fi
fi

if test x"${HAVE_OPENMP}" != xyes ; then
    if test x"${HAVE_TBB}" != xyes ; then
	AC_MSG_NOTICE([*****************************************************************])
	AC_MSG_NOTICE([*** You miss both OpenMP and TBB: Multi-cores are wasted...   ***])
	AC_MSG_NOTICE([*****************************************************************])
	HAVE_TBB=no
    fi
fi

if test x"${HAVE_TBB}" = xyes ; then
    AC_DEFINE([USE_TBB], 1, [Define this to use Intel TBB.])
    CPPFLAGS="$CPPFLAGS"
    TBB_LIBS="-ltbb"
    AC_SUBST(TBB_LIBS)
fi

AC_CONFIG_SUBDIRS([lib3ds-1.3.0])

# Check for standard C++ library
AC_CHECK_LIB(stdc++, main)

# Stuff provided by autoscan
AC_CHECK_FUNCS([atexit])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([sqrt])
AC_CHECK_FUNCS([strrchr])
AC_CHECK_HEADERS([memory.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([getopt.h])
AC_C_CONST
AC_C_INLINE
AC_HEADER_STDBOOL
AC_HEADER_STDC
AC_TYPE_SIZE_T
AC_FUNC_MALLOC

# Best optimization flags for our compiler
if test x"${enable_debug}" = xyes ; then
	AC_MSG_NOTICE([Disabling optimizations])
	CXXFLAGS=`echo "$CXXFLAGS" | sed 's,-O3,,g'`
else
        AX_CXXFLAGS_GCC_OPTION(-Wall)
        AX_CXXFLAGS_GCC_OPTION(-Wextra)
#         AX_CXXFLAGS_GCC_OPTION(-pedantic)
        AX_CXXFLAGS_GCC_OPTION(-fomit-frame-pointer)
        AX_CXXFLAGS_GCC_OPTION(-ffast-math)
        AX_CXXFLAGS_GCC_OPTION(-funsafe-math-optimizations)
        AX_CXXFLAGS_GCC_OPTION(-mtune=native)

	case ${host_os} in
	  *mingw*)
	    WHOLEPROGRAM=""
	  ;;
	  *)
	    AX_CXXFLAGS_GCC_OPTION(-flto)
	    AX_CHECK_LINKER_FLAGS(-fwhole-program,[WHOLEPROGRAM="-fwhole-program"],[])
	  ;;
	esac
        AC_SUBST(WHOLEPROGRAM)

	# SIMD detection - core logic from mplayer

	AC_MSG_CHECKING(for SSE)
	AC_RUN_IFELSE([AC_LANG_SOURCE([#include <stdlib.h>
#include <signal.h>
void catchme(int) { exit(1); }
int main(void) {
signal(SIGILL, catchme);
__asm__ volatile ("xorps %%xmm0, %%xmm0":::"memory"); return 0;
}
	])], [HAVESSE=yes], [HAVESSE=no])
	AC_MSG_RESULT($HAVESSE)
	if test x"${HAVESSE}" = xyes ; then
	    AC_DEFINE([SIMD_SSE], 1, [Define this to use SSE intrinsics.])
	fi

	AC_MSG_CHECKING(for SSE2)
	AC_RUN_IFELSE([AC_LANG_SOURCE([#include <stdlib.h>
#include <signal.h>
void catchme(int) { exit(1); }
int main(void) {
signal(SIGILL, catchme);
__asm__ volatile ("xorpd %%xmm0, %%xmm0":::"memory"); return 0;
}
	])], [HAVESSE2=yes], [HAVESSE2=no])
	AC_MSG_RESULT($HAVESSE2)
	if test x"${HAVESSE2}" = xyes ; then
	    AC_DEFINE([SIMD_SSE2], 1, [Define this to use SSE2 intrinsics.])
	fi

	AC_MSG_CHECKING(for SSSE3)
	AC_RUN_IFELSE([AC_LANG_SOURCE([#include <stdlib.h>
#include <signal.h>
void catchme(int) { exit(1); }
int main(void) {
signal(SIGILL, catchme);
__asm__ volatile ("pabsd %%xmm0, %%xmm0":::"memory"); return 0;
}
	])], [HAVESSSE3=yes], [HAVESSSE3=no])
	AC_MSG_RESULT($HAVESSSE3)

	if test x"${HAVESSE}" = xyes ; then
	    AX_CXXFLAGS_GCC_OPTION(-msse)
	    AX_CXXFLAGS_GCC_OPTION(-mrecip)
	    AX_CXXFLAGS_GCC_OPTION(-mfpmath=sse)
	fi
	if test x"${HAVESSE2}" = xyes ; then
	    AX_CXXFLAGS_GCC_OPTION(-msse2)
	fi
	if test x"${HAVESSSE3}" = xyes ; then
	    AX_CXXFLAGS_GCC_OPTION(-mssse3)
	fi
        CXXFLAGS="$CXXFLAGS -DNDEBUG"
fi

# Finally create all the generated files
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
