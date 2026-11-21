#!/data/user/0/com.androidharness.app/files/linux/bin/bash
# Host-side build helper for the C++ unit tests on Android.
# The clang driver ships without a usable default sysroot here, so every
# invocation passes the bionic and libc++ include paths explicitly and links
# through lld.
set -e
CLANG="$PREFIX/bin/clang-21"
SYS="--target=aarch64-linux-android29"
INC="-isystem $PREFIX/include/c++/v1 -isystem $PREFIX/lib/clang/21/include -isystem $PREFIX/include -isystem $PREFIX/include/aarch64-linux-android"
COMMON="$SYS -x c++ -std=c++11 $INC -DPORTAL_HOST -O1 -g"

case "$1" in
  compile-c)
    shift
    out="$1"; shift
    "$CLANG" $SYS -x c -std=c11 -isystem "$PREFIX/lib/clang/21/include" -isystem "$PREFIX/include" -isystem "$PREFIX/include/aarch64-linux-android" -DPORTAL_HOST -O1 -g -c "$@" -o "$out"
    ;;
  compile)
    shift
    out="$1"; shift
    "$CLANG" $COMMON -c "$@" -o "$out"
    ;;
  link)
    shift
    out="$1"; shift
    "$CLANG" $SYS -B"$PREFIX/bin" -fuse-ld=lld -L"$PREFIX/lib" "$@" -o "$out" -lpthread "$PREFIX/lib/libc++_shared.so" "$PREFIX/lib/libunwind.a"
    ;;
  build)
    shift
    out="$1"; shift
    objs=()
    for src in "$@"; do
      obj="build/$(basename "${src%.*}").host.o"
      "$CLANG" $COMMON -c "$src" -o "$obj"
      objs+=("$obj")
    done
    "$CLANG" $SYS -B"$PREFIX/bin" -fuse-ld=lld -L"$PREFIX/lib" "${objs[@]}" -o "$out" -lpthread "$PREFIX/lib/libc++_shared.so" "$PREFIX/lib/libunwind.a"
    ;;
  *)
    echo "usage: hostcc.sh compile|link|build <output> <sources...>" >&2
    exit 2
    ;;
esac
