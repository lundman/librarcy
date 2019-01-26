AC_INIT([librarcy],[1.0.3], [bug-report@lundman.net])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC

AC_PROG_LIBTOOL

AC_CHECK_PROG(BUILD, build, build)

AC_TYPE_SIGNAL

AC_HEADER_TIME

AC_CHECK_HEADERS([stdarg.h varargs.h sys/time.h sys/types.h dirent.h sys/file.h sys/param.h sys/statfs.h sys/vfs.h sys/statvfs.h errno.h libgen.h sys/syslimits.h])

AC_STRUCT_DIRENT_D_TYPE

AC_CHECK_TYPES(__off64_t)
AC_CHECK_TYPES(ssize_t)
AC_CHECK_TYPES(struct dirent64)

AH_TEMPLATE([NEED_SYS_ERRLIST_DECLARED],
        [Define this if sys_errlist must be declared (if it exists).])

AC_SYS_LARGEFILE


AC_SEARCH_LIBS([dlopen], [dl])
AC_SEARCH_LIBS([open], [c])

if test "x$ac_cv_prog_gcc" = xyes ; then
   CFLAGS="$CFLAGS -Wall -g "
fi


old_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -rdynamic"
AC_TRY_LINK([], [], [RDYNAMIC=-rdynamic], [RDYNAMIC=])
AC_SUBST(RDYNAMIC)
LDFLAGS="$old_LDFLAGS"


AC_CHECK_FUNCS([strtoul strtouq strtoull strtoll])

AM_CONFIG_HEADER(config.h)

AC_CONFIG_FILES([Makefile])

AC_OUTPUT

