#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_INIT([pim6sd], [2.1.0-beta1], [https://github.com/troglobit/pim6sd/issues],, [https://github.com/troglobit/pim6sd])
AM_INIT_AUTOMAKE([1.11 foreign])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile
		 man/Makefile
		 include/Makefile
		 src/Makefile])

# Checks for programs.
AC_PROG_CC
AC_PROG_YACC
AC_PROG_LEX([noyywrap])

# Checks for header files.
AC_HEADER_STDBOOL

AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h linux/netlink.h net/if_dl.h netinet/in.h paths.h stdarg.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h syslog.h])
AC_CHECK_HEADERS([sys/queue.h netinet/in.h net/if.h net/if_var.h netinet6/in6_var.h],,,
[[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_NET_IF_VAR_H
#include <net/if_var.h>
#endif
]])
AC_CHECK_HEADERS([netinet6/pim6.h],,,
[[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
]])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

AC_CHECK_TYPES([struct mld_hdr],,,[[
 #include <sys/types.h>
 #include <netinet/in.h>
 #include <netinet/icmp6.h>
]])

# Checks for libraries.
# We don't need libfl at the moment, see '%option noyywrap'
#AC_CHECK_LIB([fl], [main])

# FIXME: Replace `main' with a function in `-ll':
AC_CHECK_LIB([l], [main])
AC_CHECK_LIB([y], [main])

# Checks for RFC3542
AC_CHECK_LIB([c], [inet6_opt_init],
	[AC_DEFINE([HAVE_RFC3542], [1], [Define to 1 if you support RFC3542])
	 have_rfc3542=yes], [have_rfc3542=no])

# Checks for RFC2292
AC_CHECK_LIB([c], [inet6_option_append],
	[AC_DEFINE([HAVE_RFC2292], [1], [Define to 1 if you support RFC2292])
	 have_rfc2292=yes], [have_rfc2292=no])

# Checks for library functions.
AC_CHECK_FUNCS([bzero dup2 gettimeofday isascii memset select socket strchr strdup strerror strrchr])

# Check for usually missing API's, which we can replace
AC_REPLACE_FUNCS([strlcpy strlcat])
AC_CONFIG_LIBOBJ_DIR([lib])

# Check for MLDv2 router/host side support
AC_ARG_ENABLE([mldv2router],
	AS_HELP_STRING([--disable-mldv2router], [disable router-side MLDv2 functionality]),
	[], [enable_mldv2router=yes])
AS_IF([test "x$enable_mldv2router" = "xyes"],
	[AC_DEFINE([HAVE_MLDV2], [1], [Define to 1 if you enable router-side MLDv2 functionality])])

AC_ARG_ENABLE([mldv2host],
	AS_HELP_STRING([--enable-mldv2host], [enable host-side MLDv2 functionality]),
	[], [enable_mldv2host=no])
AS_IF([test "x$enable_mldv2host" = "xyes"],
	[AC_DEFINE([HAVE_MLDV2HOST], [1], [Define to 1 if you have host-side MLDv2 functionality])])

# Build routesock.o or netlink.o to provide init_routesock()
AM_CONDITIONAL(BSD,   [test "x$ac_cv_header_net_if_dl_h"     = "xyes"])
AM_CONDITIONAL(LINUX, [test "x$ac_cv_header_linux_netlink_h" = "xyes"])

# Detect platform for summary
AS_IF([test "x$ac_cv_header_linux_netlink_h" = "xyes"],
	[platform=Linux],
	[test "x$ac_cv_header_net_if_dl_h" = "xyes"],
	[platform=BSD],
	[platform=Unknown])

# Generate all files
AC_OUTPUT

cat <<EOF

------------------ Summary ------------------
 $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix................: $prefix
  Sysconfdir............: `eval echo $sysconfdir`
  Localstatedir.........: `eval echo $localstatedir`
  C Compiler............: $CC $CFLAGS $CPPFLAGS $LDFLAGS $LIBS

Platform:
  Target................: $platform

System capabilities:
  RFC3542 (Advanced API): $have_rfc3542
  RFC2292 (Legacy API)..: $have_rfc2292

Optional features:
  Router-side MLDv2.....: $enable_mldv2router
  Host-side MLDv2.......: $enable_mldv2host

------------- Compiler version --------------
$($CC --version || true)
---------------------------------------------

Check the above options and compile with:
 ${MAKE-make}

EOF
