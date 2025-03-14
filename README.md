# NIX KERNEL(S) (9LEGACY BRANCH)

**This is _not_ a fork of the work presently done by Ron Minnich,
Paul A. Lalonde and al., and that can be found 
here: [NIX master repository](https://github.com/rminnich/9front) ---
the default branch there should be `nix`.**

The purpose of the present work is to learn both the Plan9 kernel and
the NIX project by making it work _also_ with the 9legacy distribution.
My main purpose (Thierry Laronde aka. TL) is to learn and document,
before, hopefully, being able to contribute more than superficially.
 
## NIX At A Glimpse

![Nix illustration](https://notes.kergis.com/nix-os/img/nix.jpg)

## Initial State

The initial state corresponds to the NIX proper code, isolated, before
modifications to fit 9front, with the sole (voluntary) divergence from
the original code of removing the support for tubes (done upstream by
Ron Minnich).

## Notes For A Quick Start

**We are not here for now. I keep the notes for reference, but since
this is the first import, do not expect the compilation to work.**

The present Nix incarnation is:
	- made for objtype=amd64
 	- has 2M pages, and supports 1G pages.

We will suppose on the following that you have put the sources
in $home/nix.

So for example cloning from the master src:

```
cd
git/clone ssh://git@github.com/tlaronde/nix
```

Once you get the modifications served, the first step is to
manipulate the namespace in order for changed files to
mix with unchanged ones. The 'nix' rc script in
sys/src/nix/ will take care of that:

```
cd nix/sys/src/nix
nix
```

The namespace is now set to allow the compilation and,
furthermore, rc scripts provided for testing will now be
accessible as nix/some_script.

### COMPILATION OF THE KERNEL

```
cd boot
mk
cd ../k10
mk ../root/nvram
mk
```

