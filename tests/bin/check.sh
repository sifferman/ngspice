#!/bin/sh

# set -x

if [ -z "$SPICE_SCRIPTS" ] ; then
    SPICE_SCRIPTS=`dirname $0`
    export SPICE_SCRIPTS
    if [ -z "$ngspice_vpath" ] ; then
        ngspice_vpath=.
        export ngspice_vpath
    fi
fi

# ls -ld $(realpath $SPICE_SCRIPTS) $SPICE_SCRIPTS/spinit
# echo "---ngspice_vpath = $ngspice_vpath"

SPICE=$1
TEST=$2

FILTER="SPARSE|KLU|CPU|Dynamic|Note|Circuit|Trying|Reference|Date|Doing|---|v-sweep|time|est|Error|Warning|Data|Index|trans|acan|oise|nalysis|ole|Total|memory|urrent|Got|Added|BSIM|bsim|B4SOI|b4soi|codemodel|^binary raw file|^ngspice.*done|Operating"

testname=`basename $TEST .cir`
testdir=`dirname $TEST`

HOST_TYPE=`uname -srvm`

# a test with a .err file also compares standard error, without directories
run_spice() {
    if [ -f $testdir/$testname.err ] ; then
        $SPICE --batch $testdir/$testname.cir >$testname.test 2>$testname.err_test
    else
        $SPICE --batch $testdir/$testname.cir >$testname.test
    fi
}

check_stderr() {
    [ -f $testdir/$testname.err ] || return 0
    sed -e 's#from file .*[/\\]#from file #' $testname.err_test |
    diff -u $testdir/$testname.err - && rm $testname.err_test
}

case $HOST_TYPE in
    Linux*|Darwin*|CYGWIN*|MINGW*|MSYS*)
      run_spice
      # contrary to the c standard windows may print floating point values
      #   with three instead of two exponential digits
      sed -e 's/\([.0-9][eE][+-]\?\)0\([0-9]\{2\}\)/\1\2/g' \
          <$testname.test | \
      egrep -v "$FILTER" > $testname.test_tmp
      egrep -v "$FILTER" $testdir/$testname.out > $testname.out_tmp
      if diff -B -w -u $testname.out_tmp $testname.test_tmp && check_stderr; then
          rm $testname.test $testname.test_tmp $testname.out_tmp
          exit 0
      fi
      rm -f $testname.test_tmp $testname.out_tmp
      ;;
    FreeBSD*|SunOS*|OpenBSD*)
      run_spice
      sed -e '/^$/d' $testname.test | egrep -v "$FILTER" > $testname.test_tmp
      sed -e '/^$/d' $testdir/$testname.out | egrep -v "$FILTER" > $testname.out_tmp
      if diff -b -w $testname.out_tmp $testname.test_tmp && check_stderr; then
          rm $testname.test $testname.test_tmp $testname.out_tmp
          exit 0
      fi
      rm -f $testname.test_tmp $testname.out_tmp
      ;;
    *)
      echo Unknown system type!
      echo $HOST_TYPE
      echo ./tests/bin/checks.sh may need updating for your system
      ;;
esac

exit 1
