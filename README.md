# fil-c-llvm

The LLVM and Clang sources (with libc++) of [Fil-C](https://github.com/pizlonator/fil-c),
used by [Fil-C-Light](https://github.com/top-master/fil-c-light) as its `compiler/`
submodule, and the releases of Fil-C-Light's prebuilt toolchain packages.

## Usage

[`fil-c-light`](https://github.com/top-master/fil-c-light)'s `build.sh` fetches
`optfil-<version>-linux-<arch>.xz` from the release of its upstream version (the release
`v<version>` here, where `<version>` comes from fil-c-light's own `upstream-<version>` git
tag), else the latest one, when run without `--nightly`: `--no-build` installs it at
`/opt/fil` (with `--no-install`, unpacks it into the tree, i.e. the fil-c-light checkout),
and a build lays it out as the tree's `build/` and `pizfix/`.

## Release Naming

We reuse upstream's naming where possible:

- **`optfil-*`** is the prefix for `glibc` compiled binaries, which will be installed to
  `/opt/fil` directory (portable: the install into the fixed path is optional).
- **`filc-*`** means `musl` as libc (portable).
- **`cosmo-filc-*`** means `cosmopolitan` as libc (portable).
