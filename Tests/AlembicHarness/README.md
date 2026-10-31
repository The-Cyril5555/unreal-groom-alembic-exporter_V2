# Alembic harness (no Unreal needed)

Compiles the plugin's **real** `Source/GroomExportEditor/Private/GroomAlembicWriter.cpp`
on Linux against the official [Alembic](https://github.com/alembic/alembic) library, with
a small stand-in for the Unreal types it uses (`shim/`), and runs it on synthetic grooms.
It is built twice, from the 1.0 sources (taken from git history, commit `9eaf714`) and
from the current ones, so the two outputs can be compared byte for byte.

It answers questions that otherwise need an Unreal install:

- does a writer change still compile, warning-free (`-Wall -Wextra -Wshadow -Werror`;
  shadowed variables are errors in Unreal too)?
- does the *keep with the strands* mode still write exactly what 1.0 wrote, and the
  *leave out* mode exactly what 1.0 wrote with guides off?
- what layout, counts and `groom_guide` values does the *separate objects* mode write?

It does **not** replace a build in Unreal: the editor module (dialog, Python API,
physics settings file) is not compiled here, `shim/` only imitates the few Unreal types
the writer touches, and the physics writer is replaced by a stub.

## Build Alembic once

```bash
git clone --depth 1 --branch v3.1.12 https://github.com/AcademySoftwareFoundation/Imath.git
git clone --depth 1 --branch 1.8.8 https://github.com/alembic/alembic.git
cmake -S Imath -B Imath/build -DCMAKE_INSTALL_PREFIX=$PWD/install -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
cmake --build Imath/build -j4 --target install
cmake -S alembic -B alembic/build -DCMAKE_INSTALL_PREFIX=$PWD/install -DCMAKE_PREFIX_PATH=$PWD/install -DALEMBIC_SHARED_LIBS=OFF -DUSE_TESTS=OFF -DUSE_HDF5=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
cmake --build alembic/build -j4 --target install
```

## Run

```bash
ALEMBIC_ROOT=$PWD/install Tests/AlembicHarness/run.sh
```

It prints one line per export (counts, objects written), `IDENTICAL` / `DIFFERENT` for
each 1.0 vs current comparison, the layout of the separate-objects file, and ends with
`ALL OK` (exit 0) or `FAILED`. Files are left in `Tests/AlembicHarness/build/out/`.

To see what Blender makes of them, run `Tests/blender_check_abc.py` on those files, in
Blender or with the `bpy` module from PyPI:

```bash
python Tests/blender_check_abc.py -- Tests/AlembicHarness/build/out/v11_separate_yup.abc
```

## Synthetic grooms

`main.cpp` builds 100 strands of 3 to 10 points in two hair groups with non-sequential
ids (3, then 1), 23 of them flagged `groom_guide = 1`, with per-vertex widths and colours,
root UVs, int / FName strand attributes and groom-scope attributes. Variants: per-strand
widths, no `groom_guide` attribute, every strand a guide.
