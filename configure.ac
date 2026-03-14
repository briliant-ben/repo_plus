AC_INIT([rpp], [0.1.0], [support@example.com])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])

AC_PROG_CC

dnl Standard headers
AC_CHECK_HEADERS([stdio.h stdlib.h string.h unistd.h errno.h])
AC_CHECK_HEADERS([sys/stat.h sys/wait.h sys/types.h fcntl.h signal.h])
AC_CHECK_HEADERS([getopt.h pthread.h])

dnl expat XML library (required for manifest parsing)
AC_CHECK_LIB([expat], [XML_ParserCreate],
    [AC_DEFINE([HAVE_LIBEXPAT], [1], [Have libexpat])
     EXPAT_LIBS="-lexpat"],
    [AC_MSG_ERROR([libexpat not found. Install libexpat-dev.])])
AC_SUBST([EXPAT_LIBS])

dnl pthreads (required for parallel sync)
AC_CHECK_LIB([pthread], [pthread_create],
    [PTHREAD_LIBS="-lpthread"],
    [AC_MSG_ERROR([pthreads not found.])])
AC_SUBST([PTHREAD_LIBS])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])
AC_OUTPUT
