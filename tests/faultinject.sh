#!/bin/sh

keep_going=
direction=1
valgrind=
show=
quiet=

while test "$1" != "${1#-}"
do
    case "$1" in
    -k)
        keep_going=true
        shift
        ;;
    -r)
        direction="-1"
        shift
        ;;
    -s)
        show=true
        quiet=
        shift
        ;;
    -g)
        valgrind=true
        shift
        ;;
    -q)
        quiet=true
        show=
        shift
        ;;
    --)
        break
        ;;
    esac
done

TPL="$1"

FAULTSTART='' "$@" 2>faultinject.stderr
EXIT_CODE="$?"
if test $EXIT_CODE -gt 127
then
    echo "initial run crashed"
    exit 1
fi

FAULTEND=$(awk '/CDEBUG FAULT INJECTION MAX/{print $5}' faultinject.stderr)

if test -z "$FAULTEND"
then
    echo "couldn't find the maximum number of fault injections"
    exit 1
fi

ulimit -S -c unlimited

export FAULTSTART=1

if test "$direction" != 1
then
    FAULTSTART=$FAULTEND
    FAULTEND=0
fi

while test "$FAULTSTART" -ne "$FAULTEND"
do
    "$@" 2>faultinject.stderr >faultinject.stdout
    EXIT_CODE="$?"
    if test "$EXIT_CODE" -gt 127; then
        case $EXIT_CODE in
        134)
            echo "Faultinject $FAULTSTART crashed with SIGABRT"
            ;;
        139)
            echo "Faultinject $FAULTSTART crashed with SIGSEGV"
            ;;
        *)
            echo "Faultinject $FAULTSTART crashed with $EXIT_CODE"
            ;;
        esac
        if test -z "$quiet" ; then
            gdb -batch -ex 'bt full' "$TPL" core >faultinject$FAULTSTART.bt
            mv faultinject.stderr faultinject$FAULTSTART.stderr
            mv faultinject.stdout faultinject$FAULTSTART.stdout
        fi
        vglog=
        if test "$valgrind" ; then
            vglog=faultinject$FAULTSTART.vg
            valgrind --log-file=$vglog "$@"
        fi
        if test "$show"; then
            less faultinject$FAULTSTART.bt $vglog faultinject$FAULTSTART.stderr faultinject$FAULTSTART.stdout
        fi
        if test -z "$keep_going"; then
            exit 1
        fi
    fi
    rm -f faultinject.stderr faultinject.stdout
    FAULTSTART=$((FAULTSTART + direction))
done
