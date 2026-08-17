AC_PREREQ([2.69])
AC_INIT(
  [catalyst-evo],
  [0.40.0],
  [https://github.com/dlworrell/evo/issues]
)
AC_CONFIG_SRCDIR([src/population.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects parallel-tests color-tests])
AC_PROG_CC
AM_PROG_AR
LT_INIT([disable-shared])

AC_CHECK_HEADERS(
  [pthread.h sched.h],
  [],
  [AC_MSG_ERROR([POSIX thread headers are required])]
)
evo_saved_LIBS=$LIBS
AC_SEARCH_LIBS(
  [pthread_create],
  [pthread],
  [],
  [AC_MSG_ERROR([a POSIX pthread implementation is required])]
)
AS_CASE(
  [$ac_cv_search_pthread_create],
  [none\ required], [EVO_THREAD_LIBS=""],
  [EVO_THREAD_LIBS="$ac_cv_search_pthread_create"]
)
AC_SUBST([EVO_THREAD_LIBS])
LIBS=$evo_saved_LIBS

AC_CHECK_FUNCS([explicit_bzero])
AS_IF(
  [test "x$ac_cv_func_explicit_bzero" = "xyes"],
  [AC_DEFINE(
    [EVO_HAVE_EXPLICIT_BZERO],
    [1],
    [Define to 1 when explicit_bzero is available.]
  )]
)

AC_ARG_ENABLE(
  [warnings-as-errors],
  [AS_HELP_STRING(
    [--disable-warnings-as-errors],
    [do not promote project warnings to errors]
  )],
  [],
  [enable_warnings_as_errors=yes]
)
AM_CONDITIONAL(
  [WARNINGS_AS_ERRORS],
  [test "x$enable_warnings_as_errors" = "xyes"])

AC_ARG_ENABLE(
  [sanitizers],
  [AS_HELP_STRING(
    [--enable-sanitizers],
    [enable AddressSanitizer and UndefinedBehaviorSanitizer]
  )],
  [],
  [enable_sanitizers=no]
)
AM_CONDITIONAL(
  [SANITIZERS],
  [test "x$enable_sanitizers" = "xyes"])

AC_CONFIG_FILES([Makefile catalyst-evo.pc])
AC_OUTPUT
