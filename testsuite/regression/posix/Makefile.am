testdir = @XENO_TEST_DIR@

CCLD = $(top_srcdir)/scripts/wrap-link.sh $(CC)

noinst_HEADERS = check.h

test_PROGRAMS = \
	leaks \
	mq_select

CPPFLAGS = $(XENO_USER_CFLAGS) 				\
	-I$(top_srcdir)/include

LDFLAGS = $(XENO_POSIX_WRAPPERS)

LDADD = 					\
	../../../lib/cobalt/libcobalt.la	\
	@XENO_USER_LDADD@			\
	-lpthread -lrt -lm
