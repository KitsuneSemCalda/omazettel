# AUR package

`PKGBUILD` and `.SRCINFO` for the [AUR](https://aur.archlinux.org/packages/omazettel)
package, which builds a tagged release from GitHub (unlike `pkgbuild/`, which
builds this checkout).

For a new release: tag `omazettel-vX.Y.Z` and push the tag; set `pkgver`
(and `pkgrel=1`) here; update the checksum with `updpkgsums`; regenerate
`.SRCINFO` with `makepkg --printsrcinfo > .SRCINFO`; test with `makepkg -f`;
then copy both files into the AUR git repository
(`ssh://aur@aur.archlinux.org/omazettel.git`), commit and push.
