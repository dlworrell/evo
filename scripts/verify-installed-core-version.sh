#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <install-prefix>" >&2
    exit 2
fi

prefix=$1
header="$prefix/include/catalyst/evo/evo.h"

if [ ! -f "$header" ]; then
    echo "missing staged public header: $header" >&2
    exit 1
fi

pkg_dirs=
for candidate in "$prefix/lib/pkgconfig" "$prefix/lib64/pkgconfig"; do
    if [ -d "$candidate" ]; then
        if [ -n "$pkg_dirs" ]; then
            pkg_dirs="$pkg_dirs:$candidate"
        else
            pkg_dirs=$candidate
        fi
    fi
done

if [ -z "$pkg_dirs" ]; then
    echo "missing staged pkg-config directory under: $prefix" >&2
    exit 1
fi

export PKG_CONFIG_PATH=
export PKG_CONFIG_LIBDIR=$pkg_dirs

pc_version=$(pkg-config --modversion catalyst-evo)

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/evo-version-parity.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

cat > "$work_dir/version_consumer.c" <<'EOF'
#include <catalyst/evo/evo.h>
#include <stdio.h>

int main(void)
{
    if (printf("%d.%d.%d\n",
               EVO_VERSION_MAJOR,
               EVO_VERSION_MINOR,
               EVO_VERSION_PATCH) < 0) {
        return 1;
    }
    return 0;
}
EOF

cc=${CC:-cc}
# pkg-config output is intentionally word-split into compiler/linker arguments.
# shellcheck disable=SC2046
"$cc" $(pkg-config --cflags catalyst-evo) \
    "$work_dir/version_consumer.c" \
    $(pkg-config --libs --static catalyst-evo) \
    -o "$work_dir/version_consumer"

header_version=$($work_dir/version_consumer)

if [ "$pc_version" != "$header_version" ]; then
    echo "installed catalyst-evo version mismatch:" >&2
    echo "  pkg-config: $pc_version" >&2
    echo "  public API: $header_version" >&2
    exit 1
fi

printf 'installed catalyst-evo core version parity: %s\n' "$pc_version"
