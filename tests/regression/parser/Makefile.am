## Process this file with automake to produce Makefile.in


TESTS = minus-minus.cir xpressn-1.cir xpressn-2.cir xpressn-3.cir bxpressn-1.cir double-quoted-numbers.cir double-quoted-expressions.cir text-after-number.cir include-last-line.cir numparam-mil.cir

TESTS_ENVIRONMENT = ngspice_vpath=$(srcdir) $(SHELL) $(top_srcdir)/tests/bin/check.sh $(top_builddir)/src/ngspice

EXTRA_DIST = \
	double-quoted-expressions.sp \
	$(TESTS) \
	$(TESTS:.cir=.out) \
	text-after-number.err \
	include-last-line.err \
	include-last-line.lib

MAINTAINERCLEANFILES = Makefile.in
