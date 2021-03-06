dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.62)

# The config/version file defines the general version information as:
# <major>.<minor>.<subrev>, giving the full Xenomai version stamp.
# config/apirev defines the revision level of the user API we
# implement (which actually expresses the revision level of the
# Copperplate library).  The kernel ABI is Cobalt-specific and is
# defined for each architecture in the asm/features.h file.
AC_INIT([Xenomai],m4_normalize(m4_include([config/version])),xenomai@xenomai.org)

CONFIG_XENO_VERSION_MAJOR=`expr $PACKAGE_VERSION : '\([[0-9]]*\)'`
CONFIG_XENO_VERSION_MINOR=`expr $PACKAGE_VERSION : '[[0-9]]*\.\([[0-9]]*\)'`
CONFIG_XENO_REVISION_LEVEL=`expr $PACKAGE_VERSION : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
CONFIG_XENO_UAPI_LEVEL=`cat $srcdir/config/apirev`
topdir=`cd $srcdir && pwd`

dnl Find out whether we build for Cobalt core, i.e. dual kernel mode,
dnl or Mercury (single image kernel). Defaults to Cobalt.
rtcore_type=cobalt
AC_MSG_CHECKING([whether we build for Cobalt or Mercury core])
AC_ARG_WITH(core,
    AS_HELP_STRING([--with-core=<cobalt | mercury>],[build for dual kernel or single image]),
    [
	case "$withval" in
	"" | y | ye | yes | n | no)
	    AC_MSG_ERROR([You must supply an argument to --with-core])
	  ;;
	cobalt|mercury)
	   rtcore_type=$withval
	   ;;
	*)
	    AC_MSG_ERROR([--with-core=<cobalt | mercury>])
	esac
    ])
AC_MSG_RESULT($rtcore_type)

AM_CONDITIONAL(XENO_COBALT,[test x$rtcore_type = xcobalt])
test x$rtcore_type = xcobalt && AC_DEFINE(CONFIG_XENO_COBALT,1,[config])
AM_CONDITIONAL(XENO_MERCURY,[test x$rtcore_type = xmercury])
test x$rtcore_type = xmercury && AC_DEFINE(CONFIG_XENO_MERCURY,1,[config])
XENO_TARGET_CORE=$rtcore_type

if test "x$CFLAGS" = "x"; then
	XENO_EMPTY_CFLAGS=true
else
	XENO_EMPTY_CFLAGS=false
fi

if eval test $includedir = /usr/include; then
  AC_MSG_ERROR([Using /usr/include as includedir is not supported. Please change your --prefix or specify another --includedir])
fi

AC_CONFIG_HEADERS(include/xeno_config.h)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR([config])
AC_CONFIG_SRCDIR(lib/cobalt/thread.c)
AC_PREFIX_DEFAULT(/usr/xenomai)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_PROG_INSTALL

AC_ARG_WITH(cc,
    AS_HELP_STRING([--with-cc=compiler],[use specific C compiler]),
    [
	case "$withval" in
	"" | y | ye | yes | n | no)
	    AC_MSG_ERROR([You must supply an argument to --with-cc])
	  ;;
	esac
	CC="$withval"
    ])
AC_PROG_CC

# Do not let autoconf set the default value of CFLAGS
if $XENO_EMPTY_CFLAGS; then
	CFLAGS=""
fi

AC_PROG_CC_FOR_BUILD

AC_DEFINE_UNQUOTED(CONFIG_XENO_BUILD_STRING,"$build",[Build system alias])
XENO_BUILD_STRING="$build"
AC_DEFINE_UNQUOTED(CONFIG_XENO_HOST_STRING,"$host",[Host system alias])
XENO_HOST_STRING="$host"
AC_DEFINE_UNQUOTED(CONFIG_XENO_COMPILER,"`$CC -v 2>&1 | tail -n 1`",[Compiler])

AM_INIT_AUTOMAKE([foreign no-exeext dist-bzip2])
AM_MAINTAINER_MODE
AM_PROG_AS
AM_PROG_LEX

AC_MSG_CHECKING([for target architecture])

if test x$host_alias = x; then
  build_for=$host
else
  build_for=$host_alias
fi

use_tls=no
case "$build_for" in
 i*86*-*)
	use_tls=yes
	XENO_TARGET_ARCH=x86
	CONFIG_XENO_DEFAULT_PERIOD=100000
	;;
 ppc-*|powerpc-*|powerpc64-*|ppc64-*)
	use_tls=yes
	XENO_TARGET_ARCH=powerpc
	CONFIG_XENO_DEFAULT_PERIOD=100000
	;;
 bfin-*|bfinnommu-*|blackfin-*)
	XENO_TARGET_ARCH=blackfin
	CONFIG_XENO_DEFAULT_PERIOD=100000
	;;
 arm*-*)
	XENO_TARGET_ARCH=arm
	CONFIG_XENO_DEFAULT_PERIOD=1000000
	;;
 x86_64-*|amd64-*)
	use_tls=yes
	XENO_TARGET_ARCH=x86
	CONFIG_XENO_DEFAULT_PERIOD=100000
	;;
 nios2-*)
	XENO_TARGET_ARCH=nios2
	CONFIG_XENO_DEFAULT_PERIOD=10000000
	;;
 sh-*|sh4-*)
	XENO_TARGET_ARCH=sh
	CONFIG_XENO_DEFAULT_PERIOD=100000
	;;
 *) echo ""
   echo "*******************************************"
   echo "Still unsupported target: $build_for -- Sorry."
   echo "*******************************************"
   echo ""
   exit 1;;
esac

AC_MSG_RESULT([$XENO_TARGET_ARCH])
AM_CONDITIONAL(XENO_TARGET_ARCH_X86,[test $XENO_TARGET_ARCH = x86])

case "$XENO_TARGET_ARCH" in
 nios2)
	AC_DISABLE_SHARED
	;;
 *)
	AC_ENABLE_SHARED
	;;
esac
AC_PROG_LIBTOOL

dnl
dnl Parse options
dnl

dnl Debug build (default: off, no symbols)

debug_mode=
debug_symbols=
AC_MSG_CHECKING(whether to enable debug mode)
AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug], [Enable debug mode in programs]),
	[case "$enableval" in
	symbols)
		debug_symbols=y
		;;
	y | yes | partial)
	    	debug_mode=partial
	    	debug_symbols=y
		;;
	full)
		debug_mode=full
	    	debug_symbols=y
		;;
	n | no)
		debug_mode=
		debug_symbols=
		;;
	*)
		 AC_MSG_ERROR([invalid debug level $enableval])
		 ;;
	esac])
AC_MSG_RESULT(${debug_mode:-no})
AM_CONDITIONAL(XENO_DEBUG,[test \! x$debug_mode = x])
test \! x$debug_mode = x && AC_DEFINE(CONFIG_XENO_DEBUG,1,[config])
AM_CONDITIONAL(XENO_DEBUG_FULL,[test x$debug_mode = xfull])
test x$debug_mode = xfull && AC_DEFINE(CONFIG_XENO_DEBUG_FULL,1,[config])

dnl Low resolution clock (default: off)

unset lores_clock
AC_MSG_CHECKING(whether to enable the low resolution clock)
AC_ARG_ENABLE(lores-clock,
	AS_HELP_STRING([--enable-lores-clock], [Enable low resolution clock]),
	[case "$enableval" in
	y | yes) lores_clock=y ;;
	*) unset lores_clock ;;
	esac])
AC_MSG_RESULT(${lores_clock:-no})
if test x$lores_clock = x; then
	AC_DEFINE(CONFIG_XENO_LORES_CLOCK_DISABLED,1,[config])
fi

dnl Raw monotonic clock (default: cobalt=on, mercury=off)

if test x$rtcore_type = xcobalt; then
   raw_monotonic_clock=y
else
   raw_monotonic_clock=
fi
AC_MSG_CHECKING(whether we may use CLOCK_MONOTONIC_RAW)
AC_ARG_ENABLE(clock-monotonic-raw,
	AS_HELP_STRING([--enable-clock-monotonic-raw], [Use CLOCK_MONOTONIC_RAW for timings]),
	[case "$enableval" in
	y | yes) raw_monotonic_clock=y ;;
	*) unset raw_monotonic_clock ;;
	esac])
AC_MSG_RESULT(${raw_monotonic_clock:-no})
if test x$raw_monotonic_clock = xy; then
	AC_DEFINE(CONFIG_XENO_RAW_CLOCK_ENABLED,1,[config])
fi

checkflags="-nostdinc -isystem \$(SYSROOT)/usr/include -Wbitwise -Wno-transparent-union -D_GNU_SOURCE -D_XOPEN_SOURCE=500 -D_REENTRANT \$(DEFS) \$(DEFAULT_INCLUDES) \$(INCLUDES) \$(AM_CPPFLAGS) \$(CPPFLAGS) -I\$(top_srcdir)/include -isystem \$(shell \$(CC) -print-file-name=include) -include \$(top_builddir)/include/xeno_config.h \$(shell \$(CC) -dM -E -xc /dev/null|sed -e 's/^\\#define /-D/' -e \"s/ /=\'/\" -e \"s/\$\$/\'/\")"

dnl Used with sparse
AC_SUBST(CHECKFLAGS, $checkflags)

dnl Enable assertions (default: depends on debug mode)

test x$debug_mode = x || use_assert=y
AC_MSG_CHECKING(whether assertions should be enabled)
AC_ARG_ENABLE(assert,
	AS_HELP_STRING([--enable-assert], [Enable runtime assertions]),
	[case "$enableval" in
	y | yes) use_assert=y ;;
	*) unset use_assert ;;
	esac])
AC_MSG_RESULT(${use_assert:-no})

dnl Enable asynchronous cancellation (default: on)

async_cancel=y
AC_MSG_CHECKING(whether asynchronous cancellation of threads is enabled)
AC_ARG_ENABLE(async-cancel,
	AS_HELP_STRING([--enable-async-cancel], [Enable asynchronous cancellation]),
	[case "$enableval" in
	y | yes) async_cancel=y ;;
	n | no) unset async_cancel ;;
	esac])
AC_MSG_RESULT(${async_cancel:-no})

if test x$async_cancel = xy; then
	AC_DEFINE(CONFIG_XENO_ASYNC_CANCEL,1,[config])
fi

dnl Enable shared multi-processing (default: off)

use_pshared=
AC_MSG_CHECKING(whether shared multi-processing should be supported)
AC_ARG_ENABLE(pshared,
	AS_HELP_STRING([--enable-pshared], [Enable shared multi-processing for capable skins]),
	[case "$enableval" in
	y | yes) use_pshared=y ;;
	*) unset use_pshared ;;
	esac])
AC_MSG_RESULT(${use_pshared:-no})

if test x$use_pshared = xy; then
	AC_DEFINE(CONFIG_XENO_PSHARED,1,[config])
fi
AM_CONDITIONAL(XENO_PSHARED,[test x$use_pshared = xy])

dnl Registry support in user-space (FUSE-based, default: off)

use_registry=
AC_MSG_CHECKING(whether the registry should be enabled)
AC_ARG_ENABLE(registry,
	AS_HELP_STRING([--enable-registry], [Export real-time objects to a registry]),
	[case "$enableval" in
	y | yes) use_registry=y ;;
	*) unset use_registry ;;
	esac])
AC_MSG_RESULT(${use_registry:-no})

if test x$use_registry = xy; then
	fuse_cflags="-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=25"
	fuse_ldflags="-lfuse"
	AC_LANG_SAVE
	AC_LANG_C
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS=$fuse_cflags
	AC_CHECK_HEADER(fuse.h,,
		AC_MSG_ERROR([Cannot find fuse.h (part of libfuse development package) on this system])
	)
	AC_CHECK_LIB(fuse, fuse_new,,
		AC_MSG_ERROR([Cannot find libfuse (FUSE development package) on this system]),
		[-lpthread -lrt -ldl]
	)
	CPPFLAGS="$save_CPPFLAGS"
	AC_LANG_RESTORE
	AC_DEFINE(CONFIG_XENO_REGISTRY,1,[config])
	AC_DEFINE(_FILE_OFFSET_BITS,64,[config])
	AC_DEFINE(FUSE_USE_VERSION,25,[config])
fi
AM_CONDITIONAL(XENO_REGISTRY,[test x$use_registry = xy])

dnl SMP support (default: off)

CONFIG_SMP=
if test $XENO_TARGET_ARCH = x86; then
	CONFIG_SMP=y
fi
AC_MSG_CHECKING(for SMP support)
AC_ARG_ENABLE(smp,
	AS_HELP_STRING([--enable-smp], [Enable SMP support]),
	[case "$enableval" in
	y | yes) CONFIG_SMP=y ;;
	*) unset CONFIG_SMP ;;
	esac])
AC_MSG_RESULT(${CONFIG_SMP:-no})

dnl VSYSCALL (default: enabled) for x86

CONFIG_XENO_X86_VSYSCALL=y

if test $XENO_TARGET_ARCH = x86; then
  AC_MSG_CHECKING(for x86 VSYSCALL availability)
  AC_ARG_ENABLE(x86-vsyscall,
	AS_HELP_STRING([--enable-x86-vsyscall], [Assume VSYSCALL enabled for issuing syscalls]),
	[case "$enableval" in
	y | yes) CONFIG_XENO_X86_VSYSCALL=y ;;
	*) unset CONFIG_XENO_X86_VSYSCALL ;;
	esac])
  AC_MSG_RESULT(${CONFIG_XENO_X86_VSYSCALL:-no})
fi

dnl ARCH support for ARM (default: 4)

if test $XENO_TARGET_ARCH = arm ; then

  unset CONFIG_XENO_ARM_SA1100
  unset CONFIG_XENO_CPU_XSC3
  CONFIG_XENO_ARM_TSC_TYPE=__XN_TSC_TYPE_KUSER

  AC_MSG_CHECKING(for ARM tsc type)
  AC_ARG_ENABLE(arm-tsc,
	AS_HELP_STRING([--enable-arm-tsc], [You only need this option
  if you get the following message when starting an application:
Xenomai: incompatible feature set
(userland requires "kuser_tsc ...", kernel provides "...", missing="kuser_tsc").
In that case, this option allows to select the machine for which a
user-space tsc emulation should be generated. Valid machines are at91,
kuser, imx, ixp4xx, omap, orion, pxa, s3c24x, sa1100. [default=kuser]]),
	[case "$enableval" in
	at91*)	    CONFIG_XENO_ARM_TSC_TYPE=__XN_TSC_TYPE_FREERUNNING_FAST_WRAP;;

	integrator) unset tsc_type;;

	ixp4xx)	    CONFIG_XENO_ARM_TSC_TYPE=__XN_TSC_TYPE_FREERUNNING;;

	imx*|mx*)   CONFIG_XENO_ARM_TSC_TYPE=__XN_TSC_TYPE_FREERUNNING;;

	generic|kuser)	    CONFIG_XENO_ARM_TSC_TYPE=__XN_TSC_TYPE_KUSER;;

	omap*)	    CONFIG_XENO_ARM_TSC_TYPE=__XN_TSC_TYPE_FREERUNNING;;

	orion)	    CONFIG_XENO_ARM_TSC_TYPE=__XN_TSC_TYPE_FREERUNNING_COUNTDOWN;;

	pxa*)	    CONFIG_XENO_CPU_XSC3=y
		    CONFIG_XENO_ARM_TSC_TYPE=__XN_TSC_TYPE_FREERUNNING;;

	s3c24*)	    CONFIG_XENO_ARM_TSC_TYPE=__XN_TSC_TYPE_DECREMENTER;;

	sa1100)	    CONFIG_XENO_ARM_SA1100=y
		    CONFIG_XENO_ARM_TSC_TYPE=__XN_TSC_TYPE_FREERUNNING;;

	n | no)	    unset CONFIG_XENO_ARM_TSC_TYPE;;

	*)AC_MSG_ERROR([Invalid SOC selected: "$enableval"]);;
	esac])
  AC_MSG_RESULT(${enable_arm_tsc:-kuser})

  AC_ARG_ENABLE(arm-quirks,
	AS_HELP_STRING([--enable-arm-quirks], [Enable quirks for
	specific systems. Currently supported quirks are for sa1100 and
	xscale3.]),
	[case "$enableval" in
	sa1100)	 CONFIG_XENO_ARM_SA1100=y;;
	xscale3) CONFIG_XENO_CPU_XSC3=y;;
	*)AC_MSG_ERROR([Invalid SOC selected: "$enableval"]);;
	esac])

  AC_MSG_CHECKING(for ARM SA1100 architecture)
  AC_MSG_RESULT(${CONFIG_XENO_ARM_SA1100:-no})

  AC_MSG_CHECKING(for ARM Xscale3 architecture)
  AC_MSG_RESULT(${CONFIG_XENO_CPU_XSC3:-no})
fi

dnl Documentation package.

if test x"$docdir" = x; then
	docdir=$datadir/doc/$PACKAGE-$VERSION
fi
AC_SUBST(docdir)

if test \! -d $srcdir/doc ; then
    unset XENO_MAYBE_DOCDIR
else
    XENO_MAYBE_DOCDIR=doc
    AC_MSG_CHECKING(whether installing documentation)
    AC_ARG_ENABLE(doc-install,
       AS_HELP_STRING([--disable-doc-install], [Do not install documentation]),
       [case "$enableval" in
       y | yes);;
       *) unset XENO_MAYBE_DOCDIR;;
       esac])
    AC_MSG_RESULT(${XENO_MAYBE_DOCDIR:-no})
fi
AC_SUBST(XENO_MAYBE_DOCDIR)

AC_MSG_CHECKING(for Doxygen documentation)
AC_ARG_ENABLE(dox-doc,
	AS_HELP_STRING([--enable-dox-doc], [Build Doxygen documentation]),
	[case "$enableval" in
	y | yes) CONFIG_XENO_DOC_DOX=y ;;
	*) unset CONFIG_XENO_DOC_DOX ;;
	esac])

if test \! -d $srcdir/doc ; then
    if test x$CONFIG_XENO_DOC_DOX = xy ;
    then
	AC_MSG_ERROR([documentation tree is missing])
    fi
    AC_MSG_RESULT([not present])
else
    AC_MSG_RESULT(${CONFIG_XENO_DOC_DOX:-no})
fi

AC_CHECK_PROG(DOXYGEN, doxygen, doxygen)

if test x${CONFIG_XENO_DOC_DOX} = xy -a x"$DOXYGEN" = x ; then
   AC_MSG_ERROR([Missing the Doxygen tools to generate the documentation])
fi

AC_CHECK_PROG(DOXYGEN_HAVE_DOT, dot, YES, NO)
if test x"$DOXYGEN_HAVE_DOT" = xYES ; then
   DOXYGEN_SHOW_INCLUDE_FILES=NO
else
   DOXYGEN_SHOW_INCLUDE_FILES=YES
fi

case "$CONFIG_XENO_DOC_DBX" in
y | ye | yes)
    DBX_DOC=yes ;;
*)
    DBX_DOC="" ;;
esac
DBX_NET="$CONFIG_XENO_DBX_NET"
DBX_ROOT="$CONFIG_XENO_DBX_ROOT"
DBX_XSL_ROOT="$CONFIG_XENO_DBX_XSL_ROOT"
DBX_DOC_INIT(doc/docbook, doc/generated, 4.2)

AC_MSG_CHECKING(whether using LaTeX non-stop mode)
AC_ARG_ENABLE(verbose-latex,
	AS_HELP_STRING([--enable-verbose-latex], [Uses LaTeX non-stop mode]),
	[case "$enableval" in
	y | yes) CONFIG_XENO_DOC_LATEX_NONSTOP=y ;;
	*) unset CONFIG_XENO_DOC_LATEX_NONSTOP ;;
	esac])
AC_MSG_RESULT(${CONFIG_XENO_DOC_LATEX_NONSTOP:-no})

if test x${CONFIG_XENO_DOC_LATEX_NONSTOP} = xy; then
   LATEX_BATCHMODE=NO
   LATEX_MODE=nonstopmode
else
   LATEX_BATCHMODE=YES
   LATEX_MODE=batchmode
fi

AC_MSG_CHECKING(for asciidoc documentation)
AC_ARG_ENABLE(asciidoc,
	AS_HELP_STRING([--enable-asciidoc], [Build Asciidoc documentation]),
	[case "$enableval" in
	y | yes) CONFIG_XENO_ASCIIDOC=y ;;
	*) unset CONFIG_XENO_ASCIIDOC ;;
	esac])

if test \! -d $srcdir/doc ; then
    if test x$CONFIG_XENO_ASCIIDOC = xy ;
    then
	AC_MSG_ERROR([documentation tree is missing])
    fi
    AC_MSG_RESULT([not present])
else
    AC_MSG_RESULT(${CONFIG_XENO_ASCIIDOC:-no})
fi

AC_CHECK_PROG(ASCIIDOC, asciidoc, asciidoc)
if test x${CONFIG_XENO_ASCIIDOC} = xy -a x"$ASCIIDOC" = x ; then
   AC_MSG_ERROR([Missing the asciidoc tool to generate the documentation])
fi
AC_CHECK_PROG(A2X, a2x, a2x)
if test x${CONFIG_XENO_ASCIIDOC} = xy -a x"$A2X" = x ; then
   AC_MSG_ERROR([Missing the a2x tool to generate the documentation])
fi
AC_CHECK_PROG(W3M, w3m, w3m)
if test x${CONFIG_XENO_ASCIIDOC} = xy -a x"$W3M" = x ; then
   AC_MSG_ERROR([Missing the w3m tool to generate the documentation])
fi

dnl Determine whether some specific headers are present on this system.
AC_CHECK_HEADERS(mqueue.h)

dnl Check for presence of some routines we need
save_LIBS="$LIBS"
LIBS="$LIBS -lrt -lpthread"
AC_CHECK_FUNCS([pthread_mutexattr_setprotocol pthread_condattr_setclock pthread_spin_lock fork])
LIBS="$save_LIBS"

dnl
dnl Produce the info needed to build xeno_config.h
dnl

AC_DEFINE_UNQUOTED(CONFIG_XENO_VERSION_MAJOR,$CONFIG_XENO_VERSION_MAJOR,[config])
AC_DEFINE_UNQUOTED(CONFIG_XENO_VERSION_MINOR,$CONFIG_XENO_VERSION_MINOR,[config])
AC_DEFINE_UNQUOTED(CONFIG_XENO_REVISION_LEVEL,$CONFIG_XENO_REVISION_LEVEL,[config])
AC_DEFINE_UNQUOTED(CONFIG_XENO_UAPI_LEVEL,$CONFIG_XENO_UAPI_LEVEL,[config])
AC_DEFINE_UNQUOTED(CONFIG_XENO_PREFIX,"$prefix",[config])

dnl
dnl Features we enabled and likely want to find at kernel level.
dnl When applicable, we reuse the kernel option symbol so that we
dnl don't need to make particular cases with kernel code which may
dnl also be compiled in user-space libs.
dnl

test x$CONFIG_XENO_X86_VSYSCALL = xy && AC_DEFINE(CONFIG_XENO_X86_VSYSCALL,1,[config])
test x$CONFIG_SMP = xy && AC_DEFINE(CONFIG_SMP,1,[config])
test x$CONFIG_XENO_ARM_SA1100 = xy && AC_DEFINE(CONFIG_XENO_ARM_SA1100,1,[config])
test x$CONFIG_XENO_CPU_XSC3 = xy && AC_DEFINE(CONFIG_XENO_CPU_XSC3,1,[config])
test $CONFIG_XENO_ARM_TSC_TYPE && AC_DEFINE_UNQUOTED(CONFIG_XENO_ARM_TSC_TYPE,$CONFIG_XENO_ARM_TSC_TYPE,[config])

dnl
dnl Userland may want to know about MMU availability on the target.
dnl For now, we assume that having fork() means having an MMU.
dnl
test x$ac_cv_func_fork = xyes && AC_DEFINE(CONFIG_MMU,1,[config])

AM_CONDITIONAL(CONFIG_XENO_SHARED,[test "$enable_shared" = 'yes'])

# Default sampling period (ns) used in various tests
AC_DEFINE_UNQUOTED(CONFIG_XENO_DEFAULT_PERIOD,$CONFIG_XENO_DEFAULT_PERIOD,[config])

dnl
dnl Produce automake conditionals.
dnl

dnl Documentation.
AM_CONDITIONAL(CONFIG_XENO_DOC_DOX,[test "$CONFIG_XENO_DOC_DOX" = y])
AM_CONDITIONAL(CONFIG_XENO_ASCIIDOC,[test "$CONFIG_XENO_ASCIIDOC" = y])

dnl Allocator for Copperplate
dnl Note: in dual kernel mode, we don't want malloc, no matter what.
dnl We switch to malloc only over the Mercury core in debug mode, to ease
dnl debugging with valgrind, instrumented glibc etc.
AM_CONDITIONAL(XENO_TLSF,[test x$rtcore_type = xcobalt -o x$debug_mode = x])
test x$rtcore_type = xcobalt -o x$debug_mode = x && AC_DEFINE(CONFIG_XENO_TLSF,1,[config])

AC_MSG_CHECKING([for sched_setaffinity])
AC_CACHE_VAL(ac_cv_sched_setaffinity,
 save_CFLAGS="$CFLAGS"
 CFLAGS="-Wimplicit-function-declaration -Werror -D_GNU_SOURCE"
 [AC_TRY_LINK([#include <sched.h>],
 [cpu_set_t cpu_set; sched_setaffinity(0, sizeof(cpu_set), &cpu_set);],
 [ac_cv_sched_setaffinity=new],
 [ac_cv_sched_setaffinity=bad])]
 if test $ac_cv_sched_setaffinity = bad ; then
   [AC_TRY_LINK([#include <sched.h>],
   [cpu_set_t cpu_set; sched_setaffinity(0, &cpu_set);],
   [ac_cv_sched_setaffinity=old],
   [ac_cv_sched_setaffinity=no])]
 fi
 if test $ac_cv_sched_setaffinity = old ; then
     AC_MSG_RESULT(old)
     AC_DEFINE(HAVE_OLD_SETAFFINITY,1,[config])
 elif test $ac_cv_sched_setaffinity = new ; then
     AC_MSG_RESULT(ok)
     AC_DEFINE(HAVE_RECENT_SETAFFINITY,1,[config])
 else
     AC_MSG_RESULT(no)
 fi
 CFLAGS="$save_CFLAGS")

dnl Check for atomic builtins. For now we only check for the legacy
dnl interface, i.e. __sync_*.

AC_CACHE_CHECK([whether the compiler provides atomic builtins], ac_cv_atomic_builtins, [
LIBS=
AC_TRY_LINK([
int atomic_sub(int i) { return __sync_sub_and_fetch(&i, 1); }
int atomic_add(int i) { return __sync_add_and_fetch(&i, 1); }
], [], ac_cv_atomic_builtins="yes")
])
if test "$ac_cv_atomic_builtins" != "yes"; then
   AC_MSG_ERROR([compiler does not support atomic builtins])
fi

dnl Exported CFLAGS and LDFLAGS, shared with internal flags
XENO_USER_APP_CFLAGS="-D_GNU_SOURCE -D_REENTRANT"
XENO_USER_APP_LDFLAGS=

if test x$use_registry = xy; then
   XENO_FUSE_CFLAGS=$fuse_cflags
   XENO_USER_APP_LDFLAGS="$XENO_USER_APP_LDFLAGS $fuse_ldflags"
fi

dnl Internal CFLAGS and LDFLAGS, may be enhanced per-arch below
XENO_USER_CFLAGS="$XENO_USER_APP_CFLAGS -pipe -fstrict-aliasing \
-Wall -Wstrict-prototypes -Wmissing-prototypes -Wno-long-long -Wno-unused-parameter -Werror"
XENO_USER_LDADD="$XENO_USER_APP_LDFLAGS"

if test x$debug_mode = xpartial; then
   XENO_USER_CFLAGS="-g -O2 -D__XENO_DEBUG__ $XENO_USER_CFLAGS"
elif test x$debug_mode = xfull; then
   XENO_USER_CFLAGS="-g -O0 -D__XENO_DEBUG__ -D__XENO_DEBUG_FULL__ $XENO_USER_CFLAGS"
elif test x$debug_symbols = xy; then
   XENO_USER_CFLAGS="-g -O2 $XENO_USER_CFLAGS"
else
   XENO_USER_CFLAGS="-O2 $XENO_USER_CFLAGS"
fi

if test x$use_assert = x; then
   XENO_USER_CFLAGS="-DNDEBUG $XENO_USER_CFLAGS"
fi

XENO_USER_CFLAGS="$XENO_USER_CFLAGS -D__XENO__ -D__IN_XENO__ -I$topdir/include/$rtcore_type"

AC_MSG_CHECKING([whether ld supports @file])
AC_CACHE_VAL(ac_cv_ld_file_option,
  AC_LANG_SAVE
  AC_LANG_C
  save_LDFLAGS="$LDFLAGS"
  [LDFLAGS="-Wl,@/dev/null"]
  AC_LINK_IFELSE([AC_LANG_SOURCE([main(){}])],
    [ac_cv_ld_file_option=yes],
    [ac_cv_ld_file_option=no])
  LDFLAGS="$save_LDFLAGS"
  AC_LANG_RESTORE)
AC_MSG_RESULT(${ac_cv_ld_file_option:-no})
LD_FILE_OPTION=$ac_cv_ld_file_option
AC_SUBST(LD_FILE_OPTION)

AC_MSG_CHECKING(whether to enable dlopening of Xenomai libraries)
AC_ARG_ENABLE(dlopen-libs,
	AC_HELP_STRING([--enable-dlopen-libs], [Allow dynamic loading of Xenomai libraries]),
	[case "$enableval" in
	y | yes) CONFIG_XENO_LIBS_DLOPEN=y ;;
	*) CONFIG_XENO_LIBS_DLOPEN=$enableval ;;
	esac])
AC_MSG_RESULT(${CONFIG_XENO_LIBS_DLOPEN:-no})
if test x$CONFIG_XENO_LIBS_DLOPEN = xy; then
	AC_DEFINE(CONFIG_XENO_LIBS_DLOPEN,1,[config])
	AC_DEFINE_UNQUOTED(CONFIG_XENO_TLS_MODEL,"global-dynamic",[TLS model])
else
	AC_DEFINE_UNQUOTED(CONFIG_XENO_TLS_MODEL,"initial-exec",[TLS model])
	XENO_LIB_LDFLAGS="-Wl,-z -Wl,nodlopen"
fi

AC_MSG_CHECKING(whether to enable TLS support)
AC_ARG_ENABLE([tls],
	    AC_HELP_STRING([--enable-tls],
			   [Enable thread local storage]),
	    [use_tls=$enableval])
AC_MSG_RESULT($use_tls)

dnl Check whether the compiler supports the __thread keyword.
if test "x$use_tls" != xno; then
	AC_CACHE_CHECK([for __thread keyword], libc_cv_gcc_tls,
	[cat > conftest.c <<\EOF
__thread int a __attribute__ ((tls_model ("initial-exec"))) = 42;
__thread int b __attribute__ ((tls_model ("global-dynamic"))) = 12;
EOF
	if AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS -c -Werror conftest.c >&AS_MESSAGE_LOG_FD]); then
		libc_cv_gcc_tls=yes
	else
		libc_cv_gcc_tls=no
	fi
	rm -f conftest*])
	if test "$libc_cv_gcc_tls" = yes; then
		AC_DEFINE(HAVE_TLS,1,[config])
	fi
fi

AC_MSG_CHECKING(location for test binaries)
AC_ARG_WITH(testdir,
    AS_HELP_STRING([--with-testdir=<test-binaries-dir>],[location for test binaries (defaults to $bindir)]),
    [
	case "$withval" in
	"" | y | ye | yes | n | no)
	    AC_MSG_ERROR([You must supply an argument to --with-testdir])
	  ;;
	esac
	XENO_TEST_DIR="$withval"
    ], [XENO_TEST_DIR=$bindir])
AC_MSG_RESULT($XENO_TEST_DIR)

AC_MSG_CHECKING([for test source generation])
AC_RUN_IFELSE([AC_LANG_PROGRAM([[ ]], [[ ]])],
    [AC_MSG_RESULT(ok)], [AC_MSG_RESULT(failed)], [AC_MSG_RESULT(untestable)])

dnl CAUTION: We need to have the CONFIG_XENO_XX symbols always
dnl defined when the configuration header is read, but we want the
dnl Autoconf-produced symbols to be defined only when compiling
dnl Xenomai. This way, we won't pollute the namespace with the latter
dnl when our configuration header is indirectly included by a client
dnl application. To achieve this, we ask autoheader to produce the
dnl following header structure:
dnl #define CONFIG_XX
dnl #define CONFIG_XX ...
dnl #ifdef __IN_XENO__
dnl <Autoconf-defined symbols>
dnl #endif /* __IN_XENO__ */
dnl This is quite a hack since we have to rely on the fact that
dnl all Autoconf-generated symbols are lexicographically sorted
dnl after CONFIG_XENO_XX ones, but, well...
dnl Use a key which will cause the verbatim string to be put after
dnl all CONFIG_XENO_XX symbols, but still before any Autoconf-generated
dnl symbol, hence CONFIG_XENO___.
AH_VERBATIM(CONFIG_XENO___,[#ifdef __IN_XENO__])

dnl Now we can close the conditional section, right after all
dnl Autoconf-generated symbols have been listed.
AH_BOTTOM([#endif /* __IN_XENO__ */])

if test x$rtcore_type = xcobalt; then
   XENO_USER_CFLAGS="-I$topdir/lib/cobalt/arch/$XENO_TARGET_ARCH/include -I$topdir/kernel/cobalt/arch/$XENO_TARGET_ARCH/include $XENO_USER_CFLAGS"
   XENO_COBALT_CFLAGS="$XENO_USER_CFLAGS"
   case "$build_for" in
    i*86*-*) XENO_COBALT_CFLAGS="$XENO_COBALT_CFLAGS -fno-omit-frame-pointer";;
    esac

dnl Build wrapping information. XENO_POSIX_WRAPPERS lists all wrapping
dnl directives in a format the linker understands, for building the
dnl in-tree executables which require POSIX symbol wrapping.

   cobalt_wrappers="$topdir/lib/cobalt/cobalt.wrappers"
   if [[ $ac_cv_ld_file_option = yes ]]; then
	XENO_POSIX_WRAPPERS="-Wl,@$cobalt_wrappers"
   else
	XENO_POSIX_WRAPPERS=`while read wrap_option symbol ; do \
			echo -n "-Wl,$wrap_option,$symbol " ; \
		done < $cobalt_wrappers`
   fi

   AC_SUBST(XENO_POSIX_WRAPPERS)
   AC_SUBST([CONFIG_STATUS_DEPENDENCIES], ["$cobalt_wrappers"])
fi

unset want_fortify
AC_MSG_CHECKING(for fortify support)
AC_ARG_ENABLE([fortify], 
	      AC_HELP_STRING([--enable-fortify],
			     [Enable support for applications compiled
			     with _FORTIFY_SOURCE]),
	      [case "$enableval" in
              y | yes) want_fortify=yes;;
	      *) want_fortify=no;;
	      esac])
AC_MSG_RESULT(${want_fortify:-autodetect})
if test x"$want_fortify" != xno; then
   AC_CHECK_FUNC(__vfprintf_chk,
	      [AC_DEFINE(CONFIG_XENO_FORTIFY, 1 ,[config])],
	      [if test x"$want_fortify" = "xyes"; then
	      AC_MSG_ERROR([Fortify support enabled but not available in *libc])
	       fi])
fi

dnl
dnl Build the Makefiles
dnl

AC_SUBST(DOXYGEN_SHOW_INCLUDE_FILES)
AC_SUBST(DOXYGEN_HAVE_DOT)
AC_SUBST(DOXYGEN)
AC_SUBST(LATEX_BATCHMODE)
AC_SUBST(LATEX_MODE)

AC_SUBST(ASCIIDODC)
AC_SUBST(A2X)
AC_SUBST(W3M)

AC_SUBST(XENO_TARGET_CORE)
AC_SUBST(XENO_TARGET_ARCH)
AC_SUBST(XENO_BUILD_STRING)
AC_SUBST(XENO_HOST_STRING)
AC_SUBST(XENO_COBALT_CFLAGS)
AC_SUBST(XENO_LIB_LDFLAGS)
AC_SUBST(XENO_USER_CFLAGS)
AC_SUBST(XENO_USER_LDADD)
AC_SUBST(XENO_USER_APP_CFLAGS)
AC_SUBST(XENO_USER_APP_LDFLAGS)
AC_SUBST(XENO_FUSE_CFLAGS)
AC_SUBST(XENO_TEST_DIR)

AC_CONFIG_FILES([ \
	Makefile \
	config/Makefile \
	scripts/Makefile \
	scripts/xeno-config:scripts/xeno-config-$rtcore_type.in \
	scripts/xeno \
	lib/Makefile \
	lib/boilerplate/Makefile \
	lib/cobalt/Makefile \
	lib/cobalt/arch/Makefile \
	lib/cobalt/arch/arm/Makefile \
	lib/cobalt/arch/arm/include/Makefile \
	lib/cobalt/arch/arm/include/asm/Makefile \
	lib/cobalt/arch/arm/include/asm/xenomai/Makefile \
	lib/cobalt/arch/powerpc/Makefile \
	lib/cobalt/arch/powerpc/include/Makefile \
	lib/cobalt/arch/powerpc/include/asm/Makefile \
	lib/cobalt/arch/powerpc/include/asm/xenomai/Makefile \
	lib/cobalt/arch/blackfin/Makefile \
	lib/cobalt/arch/blackfin/include/Makefile \
	lib/cobalt/arch/blackfin/include/asm/Makefile \
	lib/cobalt/arch/blackfin/include/asm/xenomai/Makefile \
	lib/cobalt/arch/x86/Makefile \
	lib/cobalt/arch/x86/include/Makefile \
	lib/cobalt/arch/x86/include/asm/Makefile \
	lib/cobalt/arch/x86/include/asm/xenomai/Makefile \
	lib/cobalt/arch/nios2/Makefile \
	lib/cobalt/arch/nios2/include/Makefile \
	lib/cobalt/arch/nios2/include/asm/Makefile \
	lib/cobalt/arch/nios2/include/asm/xenomai/Makefile \
	lib/cobalt/arch/sh/Makefile \
	lib/cobalt/arch/sh/include/Makefile \
	lib/cobalt/arch/sh/include/asm/Makefile \
	lib/cobalt/arch/sh/include/asm/xenomai/Makefile \
	lib/copperplate/Makefile \
	lib/copperplate/regd/Makefile \
	lib/alchemy/Makefile \
	lib/vxworks/Makefile \
	lib/psos/Makefile \
	lib/analogy/Makefile \
	testsuite/Makefile \
	testsuite/latency/Makefile \
	testsuite/cyclic/Makefile \
	testsuite/switchtest/Makefile \
	testsuite/clocktest/Makefile \
	testsuite/unit/Makefile \
	testsuite/xeno-test/Makefile \
	testsuite/regression/Makefile \
	testsuite/regression/posix/Makefile \
	utils/Makefile \
	utils/can/Makefile \
	utils/analogy/Makefile \
	utils/ps/Makefile \
	utils/slackspot/Makefile \
	include/Makefile \
	include/nocore/Makefile \
	include/cobalt/uapi/Makefile \
	include/cobalt/uapi/asm-generic/Makefile \
	include/cobalt/uapi/kernel/Makefile \
	include/cobalt/uapi/rtdm/Makefile \
	include/cobalt/Makefile \
	include/cobalt/sys/Makefile \
	include/cobalt/kernel/Makefile \
	include/cobalt/kernel/rtdm/Makefile \
	include/cobalt/boilerplate/Makefile \
	include/rtdm/Makefile \
	include/rtdm/uapi/Makefile \
	include/analogy/Makefile \
	include/mercury/Makefile \
	include/mercury/boilerplate/Makefile \
	include/boilerplate/Makefile \
	include/copperplate/Makefile \
	include/alchemy/Makefile \
	include/vxworks/Makefile \
	include/psos/Makefile \
	])

if test \! x$XENO_MAYBE_DOCDIR = x ; then
      AC_CONFIG_FILES([ \
	doc/Makefile \
	doc/txt/Makefile \
	doc/man/Makefile \
	doc/man/clocktest.man \
	doc/man/cyclictest.man \
	doc/man/latency.man \
	doc/man/rtcanconfig.man \
	doc/man/rtcanrecv.man \
	doc/man/rtcansend.man \
	doc/man/switchbench.man \
	doc/man/switchtest.man \
	doc/man/xeno.man \
	doc/man/xeno-config.man \
	doc/doxygen/Makefile \
	doc/doxygen/Doxyfile-common \
	doc/doxygen/Doxyfile \
	doc/doxygen/Doxyfile-nucleus \
	doc/doxygen/Doxyfile-cobalt \
	doc/doxygen/Doxyfile-rtdm \
	doc/asciidoc/Makefile \
	doc/docbook/Makefile \
	doc/docbook/catalog \
	doc/docbook/custom-stylesheets/Makefile \
	doc/docbook/custom-stylesheets/xsl/Makefile \
	doc/docbook/custom-stylesheets/xsl/common/Makefile \
	doc/docbook/custom-stylesheets/xsl/fo/Makefile \
	doc/docbook/custom-stylesheets/xsl/html/Makefile \
	doc/docbook/custom-stylesheets/xsl/html/chunk.xsl \
	doc/docbook/custom-stylesheets/xsl/html/onechunk.xsl \
      ])
fi

AC_OUTPUT()
