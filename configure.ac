# Process this file with autoconf to produce a configure script.
# TODO: Unknown whether 2.64 works; was a bit of a milestone release, though
AC_PREREQ([2.64])
# TODO: required versions for automake, libtool?

AC_INIT([llama], [0.0.0], [BUG-REPORT-ADDRESS])

# Set the default C and C++ compilation flags to nothing.
# Do this immediately after AC_INIT, as other macros might invoke
# AC_PROG_CC or AC_PROG_CXX and stomp our defaults.
: ${CFLAGS=''}
: ${CXXFLAGS=''}

## FIXME: Argh, there appears to be no way to distinguish between the
## flags set by mingw64-configure and ones set by the user...
#AC_MSG_NOTICE([Default CPPFLAGS: $CPPFLAGS])
#AC_MSG_NOTICE([Default CFLAGS:   $CFLAGS])
#AC_MSG_NOTICE([Default CXXFLAGS: $CXXFLAGS])
#AC_MSG_NOTICE([Default LDFLAGS:  $LDFLAGS])

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign subdir-objects])
LT_INIT

#
# metadata
#

AM_CONDITIONAL([BUILD_MINGW], [test "x$IS_MINGW" = "xyes"])

#
# common
#

AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_INSTALL

PKG_PROG_PKG_CONFIG
AS_IF([test "x$enable_shared" != "xyes"], [PKG_CONFIG="$PKG_CONFIG --static"])

AC_LANG([C++])
AX_CXX_COMPILE_STDCXX_17([noext], [mandatory])
AX_APPEND_COMPILE_FLAGS([-W -Wall -Wextra -Wnon-virtual-dtor -pedantic -pipe], [LLAMA_CXXFLAGS])

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug],
    [build in debug mode, with symbols and no optimization])])

AS_IF([test "x$enable_debug" = "xyes"], [OFLAGS='-Og'], [OFLAGS='-O3 -mtune=intel'])

AC_ARG_ENABLE([symbols],
  [AS_HELP_STRING([--enable-symbols],
    [build with debugging symbols])],
  [enable_symbols=yes],
  [enable_symbols=no])

AS_IF([test "x$enable_symbols" != "xno"], [SYMBOLS='-g'], [SYMBOLS=''])

AX_APPEND_COMPILE_FLAGS([$SYMBOLS $OFLAGS], [LLAMA_CXXFLAGS])

AC_MSG_NOTICE([LLAMA_CXXFLAGS: $LLAMA_CXXFLAGS])
AC_MSG_NOTICE([CPPFLAGS: $CPPFLAGS])
AC_MSG_NOTICE([CXXFLAGS: $CXXFLAGS])
AC_MSG_NOTICE([CFLAGS: $CFLAGS])
AC_MSG_NOTICE([LDFLAGS: $LDFLAGS])

PKG_CHECK_MODULES([GPT], [libprofiler], [], [ : ])

# FIXME: mingw defaults to -g -O2, along with a bunch of other flags which
# we want to keep.
case "$host" in
*-*-mingw*)
  LG_REMOVE_FLAGS([CFLAGS], ['-O2'])
  LG_REMOVE_FLAGS([CXXFLAGS], ['-O2'])
  ;;
esac

#
# Boost
#
if test "x$with_boost" = "xno"; then
  AC_MSG_ERROR([--without-boost specified, but Boost is mandatory.])
else
  case "$host" in
  *-*-mingw*)
    # AX_BOOST_BASE doesn't find Boost for mingw, we help it out
    if test "$with_boost" = ""; then
      with_boost="/usr/${host}/sys-root/mingw"
      AC_MSG_NOTICE([--with-boost not set. We are guessing ${with_boost}.])
    fi
    ;;
  esac

  AX_BOOST_BASE([1.49.0],
    [],
    [AC_MSG_ERROR([Failed to find usable Boost headers.])])
fi

AX_BOOST_DATE_TIME
if test "x$ax_cv_boost_date_time" != "xyes"; then
  AC_MSG_ERROR([Failed to find Boost date_time library.])
fi

AX_BOOST_PROGRAM_OPTIONS
if test "x$ax_cv_boost_program_options" != "xyes"; then
  AC_MSG_ERROR([Failed to find Boost program_options library.])
fi

AX_BOOST_ASIO
if test "x$ax_cv_boost_asio" != "xyes"; then
  AC_MSG_ERROR([Failed to find Boost asio library.])
fi

case "$host" in
*-*-mingw*)
  # Boost ASIO needs ws2_32 and mswsock on Windows
  BOOST_ASIO_LIB="-lws2_32 -lmswsock"
  AC_SUBST([BOOST_ASIO_LIB])

  # Boost has issues unless WIN32_LEAN_AND_MEAN is specified
  # This tells windows to not default #include extra libraries, including
  # the (older) winsock.h, which cannot be included before winsock2.h
  AX_APPEND_COMPILE_FLAGS([-DWIN32_LEAN_AND_MEAN], [LLAMA_CPPFLAGS])
  ;;
esac

#
# libtsk
#
AX_PKG_CHECK_MODULES([TSK], [], [tsk])

TSK_CPPFLAGS="$TSK_CFLAGS"
TSK_CFLAGS=""

AC_SUBST([TSK_CPPFLAGS])

#
# libarchive
#
AX_PKG_CHECK_MODULES([LIBARCHIVE], [], [libarchive])

LIBARCHIVE_CPPFLAGS="$LIBARCHIVE_CFLAGS"
LIBARCHIVE_CFLAGS=""

AC_SUBST([LIBARCHIVE_CPPFLAGS])

#
# libhasher
#
AX_PKG_CHECK_MODULES([HASHER], [], [hasher])

HASHER_CPPFLAGS="$HASHER_CFLAGS"
HASHER_CFLAGS=""

AC_SUBST([HASHER_CPPFLAGS])

#
# liblightgrep
#
AX_PKG_CHECK_MODULES([LG], [], [lightgrep])

LG_CPPFLAGS="$LG_CFLAGS"
LG_CFLAGS=""

AC_SUBST([LG_CPPFLAGS])

#
# libyara
#
AX_PKG_CHECK_MODULES([YARA], [], [yara])

YARA_CPPFLAGS="$YARA_CFLAGS"
YARA_CFLAGS=""

AC_SUBST([YARA_CPPFLAGS])

#
# jsoncons
#
AC_CHECK_HEADERS([jsoncons/json.hpp], [], [AC_MSG_ERROR([Couldn't find jsoncons/json.hpp])])

#
# duckdb
#
AC_CHECK_HEADERS([duckdb.h], [], [AC_MSG_ERROR([Couldn't find duckdb.h])])

TMP_LIBS="$LIBS"
AC_CHECK_LIB([duckdb], [duckdb_library_version], [], [AC_MSG_ERROR([Couldn't find duckdb_static library])])
DUCKDB_LIBS="$LIBS"
LIBS="$TMP_LIBS"

AC_SUBST([DUCKDB_LIBS])

#
# libasan
#
AC_ARG_WITH([asan],
  [AS_HELP_STRING([--with-asan=DIR], [build with libasan])],
  [],
  [with_asan=no])

if test "x$with_asan" != "xno"; then
  ASAN_CFLAGS="-fsanitize=address -fno-omit-frame-pointer"
  ASAN_CXXFLAGS="$ASAN_CFLAGS"
  ASAN_LDFLAGS="$ASAN_CFLAGS"
  AC_SUBST([ASAN_CFLAGS])
  AC_SUBST([ASAN_CXXFLAGS])
  AC_SUBST([ASAN_LDFLAGS])
fi

#
# Threading
#
case "$host" in
*-*-mingw*)
  AX_APPEND_COMPILE_FLAGS([-mthreads], [LLAMA_CPPLAGS])
  AX_APPEND_LINK_FLAGS([-mthreads], [LLAMA_LDFLAGS])
  ;;
*-apple-darwin*)
  ;;
*)
  AX_APPEND_COMPILE_FLAGS([-pthread], [LLAMA_CPPFLAGS])
  AX_APPEND_LINK_FLAGS([-pthread], [LLAMA_LDFLAGS])
esac

AC_SUBST([LLAMA_CPPFLAGS])
AC_SUBST([LLAMA_CXXFLAGS])
AC_SUBST([LLAMA_LDFLAGS])

#
# C++ library
#
case "$host" in
*-apple-darwin*)
  # echo " ** MacOS X builds are problematic as compilers are in transition."
  # echo " ** We presume you're using a recent clang and libc++."
  # echo " ** And we recommend only building the library and not unit tests."
  # echo " ** If you build the unit tests, you are entering a world of pain."
  AX_APPEND_COMPILE_FLAGS([-stdlib=libc++], [STDCXX_CXXFLAGS])
  # TODO: Actually linking against libc++ will require everything else down
  # the chain to have been linked with libc++, including Boost, ICU, etc.
  # So, don't do this just yet.
  # STDCXX_LIB='-lc++'
  STDCXX_LIB='-lstdc++'
  ;;
*)
  STDCXX_LIB='-lstdc++'
  ;;
esac

AC_SUBST([STDCXX_LIB])
AC_SUBST([STDCXX_CXXFLAGS])

# FIXME: Is this really the right way to do this?!
# Build dynamically-linked executables if we build dynamic libs
if test "x$enable_shared" != "xyes"; then
  CXXLD="$CXX -all-static"
  CCLD="$CC -all-static"
else
  CXXLD="$CXX"
  CCLD="$CC"
fi

AC_SUBST([CXXLD])
AC_SUBST([CCLD])

#
# Test Dependencies
#

AX_PKG_CHECK_MODULES([CATCH2], [], [catch2-with-main], [], [with_catch2=no])

CATCH2_CPPFLAGS="$CATCH2_CFLAGS"
CATCH2_CFLAGS=""

AC_SUBST([CATCH2_CPPFLAGS])

if test "x$with_catch2" = 'xno'; then
  AC_MSG_WARN([])
  AC_MSG_WARN([You will be unable to compile and run the tests because:])
  AC_MSG_WARN([])
  if test "x$with_catch2" = 'xno'; then
    AC_MSG_WARN([  * Catch2 is unavailable])
  fi
  AC_MSG_WARN([])
fi

# Support for check with valgrind
AX_VALGRIND_DFLT([sgcheck], [off])
AX_VALGRIND_DFLT([helgrind], [off])
AX_VALGRIND_DFLT([drd], [off])
AX_VALGRIND_DFLT([memcheck], [on])
AX_VALGRIND_CHECK

#
# Cleanup flags
#

# TODO: libtre has a nice display we can steal
AC_MSG_NOTICE([BOOST_CPPFLAGS: $BOOST_CPPFLAGS])
AC_MSG_NOTICE([BOOST_CXXFLAGS: $BOOST_CXXFLAGS])

AC_MSG_NOTICE([ASAN_CFLAGS: $ASAN_CFLAGS])
AC_MSG_NOTICE([ASAN_CXXFLAGS: $ASAN_CXXFLAGS])
AC_MSG_NOTICE([ASAN_LDFLAGS: $ASAN_LDFLAGS])

AC_MSG_NOTICE([LG_CPPFLAGS: $LG_CPPFLAGS])
AC_MSG_NOTICE([LG_LIBS:     $LG_LIBS])

AC_MSG_NOTICE([HASHER_CPPFLAGS: $HASHER_CPPFLAGS])
AC_MSG_NOTICE([HASHER_LIBS:     $HASHER_LIBS])

AC_MSG_NOTICE([TSK_CPPFLAGS: $TSK_CPPFLAGS])
AC_MSG_NOTICE([TSK_LIBS:     $TSK_LIBS])

AC_MSG_NOTICE([LIBARCHIVE_CPPFLAGS: $LIBARCHIVE_CPPFLAGS])
AC_MSG_NOTICE([LIBARCHIVE_LIBS:     $LIBARCHIVE_LIBS])

AC_MSG_NOTICE([YARA_CPPFLAGS: $YARA_CPPFLAGS])
AC_MSG_NOTICE([YARA_LIBS:     $YARA_LIBS])

AC_MSG_NOTICE([DUCKDB_LIBS:   $DUCKDB_LIBS])

AC_MSG_NOTICE([LLAMA_CPPFLAGS: $LLAMA_CPPFLAGS])
AC_MSG_NOTICE([LLAMA_CXXFLAGS: $LLAMA_CXXFLAGS])
AC_MSG_NOTICE([LLAMA_LDFLAGS: $LLAMA_LDFLAGS])

AC_MSG_NOTICE([CC:       $CC])
AC_MSG_NOTICE([CXX:      $CXX])
AC_MSG_NOTICE([CPPFLAGS: $CPPFLAGS])
AC_MSG_NOTICE([CFLAGS:   $CFLAGS])
AC_MSG_NOTICE([CXXFLAGS: $CXXFLAGS])
AC_MSG_NOTICE([LDFLAGS:  $LDFLAGS])
AC_MSG_NOTICE([LIBS:     $LIBS])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
