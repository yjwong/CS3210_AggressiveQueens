AC_INIT([Aggressive Queens], 1.0, [wong.yong.jie@nus.edu.sg])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_SRCDIR(src/findAQ.c)

AM_INIT_AUTOMAKE

# Compiler checks.
AX_PROG_CC_MPI(true, [], [
    AC_MSG_FAILURE([An MPI compiler is required.])
])

AC_PROG_CC_C99

if test x"$ac_cv_prog_cc_c99" = x"no"; then
    AC_MSG_FAILURE([A compiler supporting C99 is required.])
fi

# Library checks.
AC_CHECK_LIB(m, ceil)
AC_CHECK_LIB(m, floor)
AC_CHECK_HEADERS([stdio.h math.h])

AC_OUTPUT(Makefile src/Makefile)
