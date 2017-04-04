GCC="${CROSS_COMPILE}gcc"
NM="${CROSS_COMPILE}nm"

OUT="offsets.h"

write_cpp()
{
    echo "$1" >> "$OUT"
}

resolve_symbol()
{
    local r=1

    off="0x$("$NM" "$1" 2>/dev/null | grep __dl_dlopen | cut -d ' ' -f 1)"
    if [ "$off" != "0x" ]; then
        write_cpp "#define DLOPEN_OFF $off"
        r=0
    fi

    return $r
}

resolve_remote_symbol()
{
    local r=1

    tmp="$(mktemp -q)"
    if adb pull "$1" "$tmp" &>/dev/null; then
        if resolve_symbol "$tmp"; then
            r=0
        fi
    fi
    rm -fr "$tmp"
    return $r
}

write_header()
{
    write_cpp "#ifndef _OFFSETS_H_"
    write_cpp "#define _OFFSETS_H_"

    write_cpp "#if defined(__aarch64__)"
    if ! resolve_remote_symbol "/system/bin/linker64"; then
        write_cpp "#error \"Unsupported architecture\""
    fi

    write_cpp "#elif defined(__arm__)"
    if ! resolve_remote_symbol "/system/bin/linker"; then
        write_cpp "#error \"Unsupported architecture\""
    fi

    write_cpp "#else"
    write_cpp "#error \"Unsupported architecture\""
    write_cpp "#endif"
    write_cpp "#endif /* _OFFSETS_H_ */"
}


main()
{
    local r=1

    if which adb &>/dev/null; then
        if [ -n "$CROSS_COMPILE" ]; then
            rm -f "$OUT"
            write_header
            r=$?
        else
            echo "Please set CROSS_COMPILE before continuing"
        fi
    else
        echo "Cannot find adb in PATH"
    fi

    return $r
}

main "@"
