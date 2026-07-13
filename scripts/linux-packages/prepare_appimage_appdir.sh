#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <AppDir>" >&2
    exit 2
fi

appdir="$1"
usr_dir="$appdir/usr"
app_binary="$usr_dir/bin/TherionStudio"
qt_bundle_lib_dir="$usr_dir/lib"
qt_bundle_plugin_dir="$usr_dir/plugins"
app_multiarch="${THERION_STUDIO_APPIMAGE_MULTIARCH:-}"

test -x "$app_binary"
mkdir -p "$qt_bundle_lib_dir"
mkdir -p "$qt_bundle_plugin_dir"
shopt -s nullglob

if [[ -z "$app_multiarch" ]] && command -v dpkg-architecture >/dev/null 2>&1; then
    app_multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || true)"
fi

copy_library_family() {
    local lib_path="$1"
    local dest_dir="$2"
    local source_dir
    local dest_abs
    local soname
    local family_base
    local -a matches
    local copied_any=1
    local match

    source_dir="$(cd "$(dirname "$lib_path")" && pwd)"
    dest_abs="$(cd "$dest_dir" && pwd)"
    if [[ "$source_dir" == "$dest_abs" ]]; then
        return 1
    fi

    soname="$(basename "$lib_path")"
    family_base="${soname%%.so*}"
    if [[ "$family_base" == "$soname" ]]; then
        return 1
    fi

    matches=("$source_dir"/"${family_base}.so"*)
    if [[ ${#matches[@]} -eq 0 ]]; then
        echo "Unable to locate library family for $lib_path" >&2
        exit 1
    fi
    for match in "${matches[@]}"; do
        if [[ ! -e "$dest_dir/$(basename "$match")" ]]; then
            cp -a "$match" "$dest_dir/"
            copied_any=0
        fi
    done

    return "$copied_any"
}

copy_qt_library_family() {
    copy_library_family "$@" || true
}

collect_linked_library_paths() {
    local binary="$1"
    local field1
    local field2
    local field3
    local rest
    local library_path="$qt_bundle_lib_dir"

    if [[ -n "$app_multiarch" ]]; then
        library_path="$library_path:$qt_bundle_lib_dir/$app_multiarch"
    fi
    if [[ -n "${LD_LIBRARY_PATH:-}" ]]; then
        library_path="$library_path:$LD_LIBRARY_PATH"
    fi

    while read -r field1 field2 field3 rest; do
        if [[ "$field2" == "=>" && "$field3" == /* ]]; then
            printf "%s\n" "$field3"
        elif [[ "$field1" == /* ]]; then
            printf "%s\n" "$field1"
        fi
    done < <(LD_LIBRARY_PATH="$library_path" ldd "$binary" 2>/dev/null || true)
}

should_bundle_runtime_library() {
    local lib_path="$1"
    local soname

    soname="$(basename "$lib_path")"
    case "$soname" in
        libb2.so* | \
        libblkid.so* | \
        libbrotlicommon.so* | \
        libbrotlidec.so* | \
        libdouble-conversion.so* | \
        libcom_err.so* | \
        libcrypto.so* | \
        libcurl-gnutls.so* | \
        libffi.so* | \
        libgio-2.0.so* | \
        libglib-2.0.so* | \
        libgmodule-2.0.so* | \
        libgmp.so* | \
        libgnutls.so* | \
        libgomp.so* | \
        libgssapi_krb5.so* | \
        libgraphite2.so* | \
        libgobject-2.0.so* | \
        libharfbuzz.so* | \
        libhogweed.so* | \
        libicu*.so* | \
        libidn2.so* | \
        libk5crypto.so* | \
        libkeyutils.so* | \
        libkrb5.so* | \
        libkrb5support.so* | \
        liblber.so* | \
        libldap.so* | \
        libmd4c.so* | \
        libmount.so* | \
        libnettle.so* | \
        libnghttp2.so* | \
        libnghttp3.so* | \
        libngtcp2.so* | \
        libngtcp2_crypto_gnutls.so* | \
        libp11-kit.so* | \
        libpcre2-16.so* | \
        libpcre2-8.so* | \
        libpng16.so* | \
        libproxy.so* | \
        libpsl.so* | \
        libpxbackend-*.so* | \
        librtmp.so* | \
        libsasl2.so* | \
        libselinux.so* | \
        libssh2.so* | \
        libsqlite3.so* | \
        libssl.so* | \
        libtasn1.so* | \
        libduktape.so* | \
        libunistring.so* | \
        libzstd.so*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

collect_qt_dependencies() {
    local binary="$1"
    local lib_path

    while IFS= read -r lib_path; do
        if [[ "$lib_path" == */libQt6*.so* ]]; then
            printf "%s\n" "$lib_path"
        fi
    done < <(collect_linked_library_paths "$binary")
}

append_qt_plugin_root() {
    local plugin_root="$1"
    local roots_file="$2"

    if [[ -d "$plugin_root" ]] && ! grep -Fxq "$plugin_root" "$roots_file"; then
        printf "%s\n" "$plugin_root" >> "$roots_file"
    fi
}

collect_qt_plugin_roots() {
    local roots_file="$1"
    local multiarch

    : > "$roots_file"

    if [[ -n "${THERION_STUDIO_QT_PLUGIN_DIR:-}" ]]; then
        append_qt_plugin_root "$THERION_STUDIO_QT_PLUGIN_DIR" "$roots_file"
    fi
    if command -v qtpaths6 >/dev/null 2>&1; then
        append_qt_plugin_root "$(qtpaths6 --plugin-dir 2>/dev/null || true)" "$roots_file"
    fi
    if command -v qtpaths >/dev/null 2>&1; then
        append_qt_plugin_root "$(qtpaths --plugin-dir 2>/dev/null || true)" "$roots_file"
    fi
    if command -v qmake6 >/dev/null 2>&1; then
        append_qt_plugin_root "$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)" "$roots_file"
    fi
    if command -v dpkg-architecture >/dev/null 2>&1; then
        multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || true)"
        if [[ -n "$multiarch" ]]; then
            append_qt_plugin_root "/usr/lib/$multiarch/qt6/plugins" "$roots_file"
        fi
    fi

    append_qt_plugin_root "/usr/lib/x86_64-linux-gnu/qt6/plugins" "$roots_file"
    append_qt_plugin_root "/usr/lib/aarch64-linux-gnu/qt6/plugins" "$roots_file"
    append_qt_plugin_root "/usr/lib/qt6/plugins" "$roots_file"
}

copy_qt_plugin_groups() {
    local roots_file="$1"
    local plugin_root
    local group
    local source_dir
    local -a plugin_groups=(
        iconengines
        imageformats
        platforminputcontexts
        platforms
        tls
        xcbglintegrations
    )

    while IFS= read -r plugin_root; do
        for group in "${plugin_groups[@]}"; do
            source_dir="$plugin_root/$group"
            if [[ -d "$source_dir" ]]; then
                mkdir -p "$qt_bundle_plugin_dir/$group"
                cp -a "$source_dir"/. "$qt_bundle_plugin_dir/$group/"
            fi
        done
    done < "$roots_file"
}

copy_runtime_library_pattern() {
    local pattern="$1"
    local root
    local match
    local found=1
    local -a roots=()

    if [[ -n "$app_multiarch" ]]; then
        roots+=("/usr/lib/$app_multiarch" "/lib/$app_multiarch")
    fi
    roots+=("/usr/lib" "/lib" "/usr/local/lib")

    for root in "${roots[@]}"; do
        [[ -d "$root" ]] || continue
        while IFS= read -r match; do
            copy_library_family "$match" "$qt_bundle_lib_dir" || true
            cp -aL "$match" "$qt_bundle_lib_dir/$(basename "$match")"
            echo "Bundled required AppImage runtime library: $match" >&2
            found=0
        done < <(find "$root" -maxdepth 7 \( -type f -o -type l \) -name "$pattern" -print 2>/dev/null | sort)
    done

    return "$found"
}

print_runtime_library_candidates() {
    local pattern="$1"
    local root
    local -a roots=()

    if [[ -n "$app_multiarch" ]]; then
        roots+=("/usr/lib/$app_multiarch" "/lib/$app_multiarch")
    fi
    roots+=("/usr/lib" "/lib" "/usr/local/lib")

    echo "Searched AppImage runtime library roots for $pattern:" >&2
    for root in "${roots[@]}"; do
        echo "  $root" >&2
        [[ -d "$root" ]] || continue
        find "$root" -maxdepth 7 \( -type f -o -type l \) -name "$pattern" -print 2>/dev/null | sort >&2 || true
    done
}

copy_required_runtime_libraries() {
    local pattern
    local -a patterns=(
        libpxbackend-*.so*
    )

    for pattern in "${patterns[@]}"; do
        if ! copy_runtime_library_pattern "$pattern"; then
            echo "Unable to locate required AppImage runtime library: $pattern" >&2
            print_runtime_library_candidates "$pattern"
            exit 1
        fi
    done
}

plugin_roots="$(mktemp)"
dependency_roots="$(mktemp)"
trap 'rm -f "$plugin_roots" "$dependency_roots"' EXIT

collect_qt_plugin_roots "$plugin_roots"
copy_qt_plugin_groups "$plugin_roots"

find "$usr_dir" -type f \( -path "$app_binary" -o -name "*.so" -o -name "*.so.*" \) | sort > "$dependency_roots"
while IFS= read -r binary; do
    while IFS= read -r qt_lib; do
        copy_qt_library_family "$qt_lib" "$qt_bundle_lib_dir"
    done < <(collect_qt_dependencies "$binary")
done < "$dependency_roots"

copy_required_runtime_libraries

runtime_dependencies_copied=1
while [[ "$runtime_dependencies_copied" -eq 1 ]]; do
    runtime_dependencies_copied=0
    find "$usr_dir" -type f \( -path "$app_binary" -o -name "*.so" -o -name "*.so.*" \) | sort > "$dependency_roots"
    while IFS= read -r binary; do
        while IFS= read -r runtime_lib; do
            if should_bundle_runtime_library "$runtime_lib"; then
                if copy_library_family "$runtime_lib" "$qt_bundle_lib_dir"; then
                    runtime_dependencies_copied=1
                fi
            fi
        done < <(collect_linked_library_paths "$binary")
    done < "$dependency_roots"
done

if ! find "$qt_bundle_lib_dir" -maxdepth 1 -name "libQt6Widgets.so.6*" | grep -q .; then
    echo "Qt deployment did not stage libQt6Widgets.so.6" >&2
    find "$usr_dir" -maxdepth 4 -type f | sort >&2
    exit 1
fi

if ! find "$usr_dir" -type f -path "*/platforms/libqoffscreen.so" | grep -q .; then
    echo "Qt deployment did not stage the offscreen platform plugin" >&2
    find "$usr_dir" -maxdepth 5 -type f | sort >&2
    exit 1
fi

cat > "$appdir/AppRun" <<EOF
#!/bin/sh
set -eu

multiarch="$app_multiarch"

appdir="\${APPDIR:-}"
if [ -z "\$appdir" ]; then
  apprun="\$0"
  case "\$apprun" in
    /*) ;;
    *) apprun="\$PWD/\$apprun" ;;
  esac
  while [ -L "\$apprun" ]; do
    apprun_dir="\$(dirname "\$apprun")"
    apprun_target="\$(readlink "\$apprun")"
    case "\$apprun_target" in
      /*) apprun="\$apprun_target" ;;
      *) apprun="\$apprun_dir/\$apprun_target" ;;
    esac
  done
  appdir="\$(cd "\$(dirname "\$apprun")" && pwd)"
fi

if [ -n "\$multiarch" ]; then
  app_multiarch_lib_path="\$appdir/usr/lib/\$multiarch"
  app_multiarch_plugin_path="\$appdir/usr/lib/\$multiarch/qt6/plugins"
else
  app_multiarch_lib_path=""
  app_multiarch_plugin_path=""
fi

if [ -n "\${LD_LIBRARY_PATH:-}" ]; then
  if [ -n "\$app_multiarch_lib_path" ]; then
    export LD_LIBRARY_PATH="\$appdir/usr/lib:\$app_multiarch_lib_path:\$LD_LIBRARY_PATH"
  else
    export LD_LIBRARY_PATH="\$appdir/usr/lib:\$LD_LIBRARY_PATH"
  fi
else
  if [ -n "\$app_multiarch_lib_path" ]; then
    export LD_LIBRARY_PATH="\$appdir/usr/lib:\$app_multiarch_lib_path"
  else
    export LD_LIBRARY_PATH="\$appdir/usr/lib"
  fi
fi

qt_plugin_path="\$appdir/usr/plugins:\$appdir/usr/lib/qt6/plugins"
if [ -n "\$app_multiarch_plugin_path" ]; then
  qt_plugin_path="\$qt_plugin_path:\$app_multiarch_plugin_path"
fi
if [ -n "\${QT_PLUGIN_PATH:-}" ]; then
  export QT_PLUGIN_PATH="\$qt_plugin_path:\$QT_PLUGIN_PATH"
else
  export QT_PLUGIN_PATH="\$qt_plugin_path"
fi

exec "\$appdir/usr/bin/TherionStudio" "\$@"
EOF
chmod +x "$appdir/AppRun"

set +e
APPDIR="$appdir" QT_QPA_PLATFORM=offscreen timeout 10s \
    "$appdir/AppRun" -platform offscreen >/tmp/therion-studio-appdir-launch.log 2>&1
exit_code=$?
set -e
if [[ "$exit_code" -ne 0 && "$exit_code" -ne 124 ]]; then
    echo "AppDir launch failed with exit code $exit_code" >&2
    cat /tmp/therion-studio-appdir-launch.log >&2
    exit "$exit_code"
fi
