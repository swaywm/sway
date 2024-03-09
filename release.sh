#!/bin/sh -eu

prev=$(git describe --tags --abbrev=0)
next=$(meson rewrite kwargs info project / 2>&1 >/dev/null | jq -r '.kwargs["project#/"].version')

case "$next" in
*-dev)
	echo "This is a development version"
	exit 1
	;;
esac

if [ "$prev" = "$next" ]; then
	echo "Version not bumped in meson.build"
	exit 1
fi

if ! git diff-index --quiet HEAD -- meson.build; then
	echo "meson.build not committed"
	exit 1
fi

shortlog="$(git shortlog --no-merges "$prev..")"
(echo "sway $next"; echo ""; echo "$shortlog") | git tag "$next" -ase -F -

prefix=sway-$next
archive=$prefix.tar.gz
git archive --prefix="$prefix/" -o "$archive" "$next"
gpg --output "$archive".sig --detach-sig "$archive"

gh release create "sway $next" -t "$next" -n "" -d "$archive" "$archive.sig"
