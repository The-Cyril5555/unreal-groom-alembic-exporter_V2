# Groom Alembic Exporter 1.1 (UE 5.7 / 5.8)

Adds **Export Groom to Alembic (.abc)…** to the Content Browser context menu of Groom
Assets. UE ships a groom Alembic *importer* but no exporter; this plugin fills that gap.

> **This is a fork.** The exporter was written by
> [effectustasi](https://github.com/effectustasi) and published as
> [effectustasi/unreal-groom-alembic-exporter](https://github.com/effectustasi/unreal-groom-alembic-exporter)
> under the MIT license; the original history is kept in this repository. Version 1.1
> adds what came up while exporting MetaHumans to Blender. See [CHANGELOG.md](CHANGELOG.md).

## What's new in 1.1

- **Guide curves handled for Blender.** By default (like 1.0) the simulation guides are
  written in the same curves objects as the real strands, flagged by the `groom_guide`
  attribute. Blender's Alembic importer does not act on that attribute, so it renders the
  guides as extra hair: up to **22 % more curves** than the real lashes on a MetaHuman
  eyelash groom. Guides can now be **kept** (round trip), written to **separate objects**,
  or **left out**. See [Guide curves](#guide-curves).
- **Blender preset** in the options dialog: Y-up, guides left out, physics settings on.
  An **Unreal round trip** preset restores the defaults.
- **Physics settings file.** Optionally writes `<name>.physics.json` next to the `.abc`
  with the groom's per-group simulation settings (stiffness, damping, gravity, air drag,
  collisions, guide interpolation), which Alembic has no place for. See
  [Physics settings file](#physics-settings-file).
- **Scripting:** `ExportGroomToAlembicWithOptions` exposes every option and returns a
  report struct. The 1.0 `ExportGroomToAlembic` is unchanged for existing scripts.
- **Unreal 5.8:** the 1.0 code compiles and exports correctly on UE 5.8.2.

## Install

1. Your project must be a **C++ project** (a Blueprint-only project has no build target for code plugins). If it isn't, add any C++ class once via *Tools → New C++ Class*; UE converts the project for you.
2. Copy this repository into your project as `Plugins/GroomExport/`.
3. Open the `.uproject` and accept the "rebuild modules?" prompt, or build from the command line (see [Building](#building)).

Requires the **HairStrands** and **AlembicImporter** plugins (both ship with UE and are enabled automatically as dependencies).

## Status

| Code | UE 5.7 | UE 5.8.2 |
| --- | --- | --- |
| Original 1.0 | ✅ verified by the original author on 2026-08-28 (details below) | ✅ compiles and exports correctly |
| This fork, 1.1 | ⏳ not built yet | ⏳ not built yet: run `Tests/export_guide_modes.py` |

The tables below are the original author's 1.0 verification on UE 5.7, against
MetaHuman grooms. Their curve counts include whatever guide curves the grooms carry (1.0
kept them by default).

| Check | Result |
| --- | --- |
| Compiles, editor DLL links | ✅ |
| Exports valid Ogawa `.abc` | ✅ all 5 groom assets |
| Blender reads it as curves, correct scale | ✅ 1508 curves / 15631 points, centimetres |
| Round trip back into Unreal | ✅ re-exported file is **byte-identical** on all 5 |
| Content Browser menu entry registered | ✅ via `IsExportMenuEntryRegistered()` in a GUI session |

Export numbers:

| Groom | Curves | Points |
| --- | ---: | ---: |
| Beard_S_Full | 16,452 | 180,842 |
| Eyebrows_M_Full | 1,508 | 15,631 |
| Eyelashes_S_Thin | 1,258 | 13,334 |
| GroomAsset_0 | 102,827 | 808,593 |
| Mustache_S_Full | 2,781 | 53,690 |

Note on the asset list: `Hair_S_BrushCut` is a **GroomBindingAsset**, not a groom. The head
hair's strand data lives in `GroomAsset_0` — the 102,827-curve entry above. Those five are
every `GroomAsset` in the Grooms folder.

## What it writes

A single archive per groom, laid out the way UE's own importer reads it
(`Engine/Plugins/Importers/AlembicHairImporter/…/AlembicHairTranslator.cpp`):

```
/groom                 OXform, identity. Carries groom-scope user properties
                       (groom_version_major/minor, groom_tool, plus whatever
                       groom-scope attributes the source asset had).
  /group_0             OCurves — one per hair group
  /group_1             OCurves
  ...
  /guides_group_0      OCurves — guide curves of group 0, only in the
  /guides_group_1      "separate objects" guide mode
  ...
```

With *One curves object per hair group* turned off, the strands go into a single
`/groom/curves` and, in the separate guide mode, the guides into `/groom/guides`.

Each `OCurves` object carries:

| Channel | Content |
| --- | --- |
| `P` | Point positions |
| `nVertices` | Point count per curve |
| curve type | `kLinear` / `kNonPeriodic` / `kNoBasis` — hair strands are polylines, not splines |
| `width` | Vertex scope when the groom has per-point widths, else uniform scope per strand |
| `uv` | Root UV, uniform scope |
| `arbGeomParams` | Every remaining `groom_*` attribute: `groom_group_id`, `groom_id`, `groom_guide`, `groom_clumpid`, `groom_color`, … |

Strand attributes are written at **uniform** scope and point attributes at **vertex**
scope, which is exactly what the importer's `ConvertAlembicAttribute` expects.

### Where the data comes from

The exporter reads `UGroomAsset::GetHairDescription()` — the editor-only source
description. That is the lossless representation: full curve count, float positions, and
all original attributes.

It deliberately does **not** read `HairGroupsPlatformData[i].Strands.BulkData`. That data
is decimated and quantized for rendering, so exporting it would silently produce a
degraded groom. If a groom has no source description (cooked asset, or editor-only data
stripped), the export fails with an explanatory message instead of writing bad data.

## Guide curves

A groom holds two kinds of curves: the **strands** that are rendered, and the
**guides** that the physics simulates and that drive the strands. Unreal tells them
apart through the per-curve `groom_guide` attribute (1 for a guide). MetaHuman grooms
ship with imported guides, so their source description contains both.

Version 1.0 always wrote the guides in the same `OCurves` as the strands, flagged by
`groom_guide`. That is exactly what Unreal's importer expects, but Blender's Alembic
importer does not use the attribute: the guides become ordinary curves and are rendered
as extra, often thicker hair. On a MetaHuman eyelash groom that was up to 22 % more
curves than the real lashes.

The options dialog now has three choices:

| Mode | What is written | Use it for |
| --- | --- | --- |
| **Keep with the strands** (default) | Guides mixed into `group_N`, flagged `groom_guide = 1` | Round trip into Unreal (byte-identical, as in 1.0) |
| **Separate objects** | Strands in `group_N`, guides in `guides_group_N`, always flagged `groom_guide = 1` | Blender / Houdini / Maya when you want the guides too (simulation, inspection): hide or delete the `guides_*` objects to render |
| **Leave out** | Strands only | Rendering in Blender: nothing to clean up |

In the separate mode the strand objects are exactly what the *leave out* mode writes,
and a guide object stays recognisable even with *Export groom_\* attributes* off: it
always carries `groom_guide = 1`, which is how Unreal's importer recognises guides;
`Tests/export_guide_modes.py` re-imports such a file to check they come back as guides. In the *leave out* mode Unreal regenerates
guides from the strands if the file is imported back, as for any groom without guides.

The completion notification says where the guides went, e.g. *"1,234 guide curve(s)
kept with the strands. Blender renders them as extra hair…"*, so a Blender export with
the wrong mode does not go unnoticed.

## Physics settings file

Alembic carries geometry only, so a groom's simulation settings are lost on export and
have to be looked up by hand in the Groom Asset editor. With **Write physics settings**
on (it is part of the Blender preset), the export also writes `<name>.physics.json`
next to `<name>.abc`:

```jsonc
{
  "format": "groom_physics_settings",
  "format_version": 1,
  "engine_version": "5.8.2-…",
  "groom_asset": "/Game/MetaHumans/…/Grooms/Eyelashes_S_Thin.Eyelashes_S_Thin",
  "units": "Values are copied verbatim from the Groom Asset, in Unreal units: centimeters, Z-up, gravity in cm/s^2. …",
  "export": {                       // how the .abc next to it was written
    "alembic_file": "Eyelashes_S_Thin.abc",
    "coordinate_system": "y_up", "unit_scale": 1, "guide_mode": "exclude",
    "num_curves": …, "num_guide_curves": 0, "num_guide_curves_left_out": …,
    "curves_objects": [ { "name": "group_0", "group_id": 0, "guides_only": false, "num_curves": …, … } ]
  },
  "asset": { "enableGlobalInterpolation": …, "hairInterpolationType": … },
  "groups": [                       // one entry per hair group of the asset, in editor order
    {
      "group_index": 0,
      "physics": {                  // FHairGroupsPhysics
        "solverSettings":      { "bEnableSimulation": …, "subSteps": …, "iterationCount": …, … },
        "externalForces":      { "gravityVector": { "x": 0, "y": 0, "z": -981 }, "airDrag": …, … },
        "materialConstraints": { "bendConstraint": { "bendStiffness": …, "bendDamping": …, … },
                                 "stretchConstraint": { … }, "collisionConstraint": { … } },
        "strandsParameters":   { … }
      },
      "interpolation": { … },       // FHairGroupsInterpolation: guide density, interpolation, rigging
      "info": { … }                 // FHairGroupInfo: curve and guide counts as Unreal built them
    }
  ]
}
```

(Abridged. `…` stands for values; the real file is plain JSON without comments.)

The settings are read through Unreal's reflection rather than a hand-written list, so
every field the engine version has is written, under the engine's own property names
(Unreal's JSON converter lower-cases the first letter). A property that does not exist
in the running engine version is listed under `not_found_in_this_engine_version`
instead of failing the export; only `HairGroupsPhysics` itself is required.

Values are **not** converted: they stay in Unreal units and axes whatever coordinate
system and unit scale the `.abc` was written with. For Blender, which is Z-up like
Unreal: gravity `(0, 0, -981)` cm/s² is Blender's `(0, 0, -9.81)` m/s².

## Coordinate space

The importer applies **no** hardcoded axis conversion. It multiplies incoming positions by
whatever the user set in the import dialog's conversion settings, whose defaults are
Rotation `(0,0,0)` / Scale `(1,1,1)`, and it treats Alembic units as centimeters
(`UNIT_TO_CM = 1`).

So the export offers two modes:

- **Unreal Z-up (default).** Positions written exactly as stored. Re-importing with the
  importer's default conversion settings reproduces the original groom — this is the
  round-trip safe option.
- **Y-up (Maya / Blender).** Swaps Y and Z: `(X, Y, Z)_unreal → (X, Z, Y)_alembic`. The
  swap has determinant −1, which is what a left-handed → right-handed basis change
  requires, so the groom keeps its shape and is **not** mirrored. To bring such a file back
  into Unreal, set the importer's conversion rotation to `(90, 0, 0)`.

`Unit scale` (default `1.0`) scales positions and widths; use `0.01` to write meters.

## Building

You need a C++ toolchain for UE. The Build Tools package is enough; the full Visual Studio
IDE is not required:

```bash
winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended"
```

Run that from an **Administrator** terminal — the install needs elevation and takes a
while. If you would rather have the full IDE, use
`Microsoft.VisualStudio.2022.Community` with the same `--add` arguments.

UE also needs the **.NET Framework SDK**, which is not in the Build Tools product graph
(`Microsoft.Net.Component.4.6.2.SDK` fails with "Cannot find package in product graph", and
the *targeting pack* is not a substitute). Without it UBT refuses to build `SwarmInterface`.
Install it standalone:

```bash
winget install --id Microsoft.DotNet.Framework.DeveloperPack_4
```

### Toolchain gotchas worth knowing

- **UE's banned MSVC list uses the compiler version, not the folder name.** The folder
  `MSVC\14.44.35207` holds compiler *14.44.35228*; UBT reports the latter and accepts it,
  even though 35207 would fall inside `BannedVisualCppVersions` (`14.44.0-14.44.35210`)
  from `Engine/Config/Windows/Windows_SDK.json`. Don't panic at the folder name.
- **`--wait` is not valid for the installed `setup.exe modify`** (only for the
  `vs_BuildTools.exe` bootstrapper). Passing it fails with exit code 87.

Then build:

```bash
"<UE_ROOT>\Engine\Build\BatchFiles\Build.bat" <YourProject>Editor Win64 Development -Project="<path>\<YourProject>.uproject" -WaitMutex
```

Or just open your `.uproject` and accept the "rebuild modules?" prompt.

## Using it

1. Right-click one or more Groom Assets in the Content Browser.
2. Choose **Export Groom to Alembic (.abc)…**.
3. Pick options — or click a preset: **Blender** (Y-up, guides left out, physics
   settings on) or **Unreal round trip** (the defaults) — then a destination. A single
   selection asks for a file name; a multi-selection asks for a folder once and writes
   `<AssetName>.abc` per groom. The options dialog offers "use these settings for all
   selected grooms" so a batch does not re-prompt. The dialog remembers the last
   settings for the rest of the editor session.

### From Python

```python
import unreal

lib = unreal.GroomExportBlueprintLibrary
groom = unreal.load_asset("/Game/MetaHumans/NewMetaHumanCharacter/Grooms/Eyelashes_S_Thin")

options = lib.get_blender_export_options()          # or get_unreal_round_trip_export_options()
options.guide_mode = unreal.GroomAlembicGuideMode.SEPARATE   # MIXED / SEPARATE / EXCLUDE
report = lib.export_groom_to_alembic_with_options(groom, "C:/Exports/Eyelashes.abc", options)

print(report.success, report.error)
print(report.num_curves, report.num_guide_curves, report.num_guide_curves_skipped)
print(report.physics_settings_file)
```

`GroomAlembicExportOptions` fields: `convert_to_y_up`, `unit_scale`, `split_by_group`,
`export_widths`, `export_root_uv`, `export_groom_attributes`, `guide_mode`,
`write_physics_settings`. The 1.0 `export_groom_to_alembic(groom, path, y_up, scale,
export_guide_curves)` still works: `export_guide_curves=False` is the *leave out* mode.

Failures are reported per asset in a notification and logged under `LogGroomExport` /
`LogGroomExportEditor`.

## Verifying it

`Tests/` holds the scripts used to verify this, all runnable headlessly. They point at the
default MetaHuman groom folder (`/Game/MetaHumans/NewMetaHumanCharacter/Grooms`); change the
constants at the top of each script to test your own assets.

| Script | What it does |
| --- | --- |
| `export_grooms.py` | Exports every groom in the Grooms folder, checks each file starts with the Ogawa magic, prints curve/point counts |
| `roundtrip_test.py` | Export → re-import → export again, and compares counts. Also dumps every asset in the folder with its class |
| `export_both_modes.py` | Writes `Eyebrows_zup.abc` and `Eyebrows_yup.abc` for external DCC checking |
| `verify_fidelity.py` | Round-trips and compares the two `.abc` files byte for byte — the strongest fidelity check |
| `check_menu.py` | Confirms the context-menu entry registered. **GUI session only** (see below) |
| `export_guide_modes.py` | 1.1: exports each groom with the three guide modes and checks the counts agree, re-imports the separate-objects file to check Unreal reads its guides back, and checks the `.physics.json` of a Blender-preset export |
| `blender_check_abc.py` | 1.1, **runs in Blender**: lists the curves objects Blender creates from each `.abc`, with counts, attributes and height. `blender --background --factory-startup --python Tests/blender_check_abc.py -- a.abc b.abc` |

`check_menu.py` cannot run as a commandlet: without a Content Browser the menu is never
built and you get a false negative. Run it in a real editor session, which quits itself:

```bash
"<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor.exe" "<path>\<YourProject>.uproject" -ExecCmds="py C:/path/without/spaces/check_menu.py, QUIT_EDITOR" -unattended -nosplash -nopause -stdout
```

Put the script somewhere without spaces — quotes nested inside `-ExecCmds` do not survive
cmd plus UE's parser, and the command silently never runs.

It asks the plugin's own `IsExportMenuEntryRegistered()` rather than inspecting the menu
from Python, because `UToolMenu::Sections` is protected and Python cannot read it — a
Python-side check reports "missing" no matter what.

Run one with:

```bash
"<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<path>\<YourProject>.uproject" -run=pythonscript -script="<path>/<YourProject>/Plugins/GroomExport/Tests/roundtrip_test.py" -unattended -nosplash -nopause -stdout
```

Two things that will bite you when scripting this:

- **Use forward slashes in `-script=`.** UE processes backslash escapes inside the quoted
  value, so `...\Tests\roundtrip_test.py` silently truncates at the `\r` of "roundtrip" and
  you get `Could not load Python file '...\Tests`.
- **The Python return is out-params only.** `export_groom_to_alembic` returns
  `(error, groups, curves, points)` — the C++ `bool` return is not part of the tuple. The
  test scripts pick values out by type rather than assuming an arity.

### Blender check (Eyebrows_M_Full, original 1.0 verification)

Imported into Blender 4.4 as a single `CURVES` object, 1508 curves / 15631 points — the
same counts Unreal reports.

| File | Blender world bbox | Reading |
| --- | --- | --- |
| `Eyebrows_yup.abc` | Z 178.5 – 181.3 | ✅ eyebrows 1.8 m up, i.e. head height. Correct. |
| `Eyebrows_zup.abc` | Z 9.2 – 14.3 | lying on its side — expected, this file is in Unreal space |

Both have the same bbox dimensions, just permuted (13.00 × 2.72 × 5.04 vs
13.00 × 5.04 × 2.72), so there is no scaling error and no mirroring. 13 cm across for a
pair of eyebrows is right.

Blender's Alembic importer assumes the file is Y-up, which is why the Y-up export is the
one that lands correctly there — and why the Unreal-space export, the one that round-trips
perfectly, looks rotated in Blender. Pick the mode for the destination.

## Notes

- Cards meshes are out of scope. The `*_CardsMesh_Group0_LOD*` assets in the Grooms folder
  are ordinary static meshes and export as FBX through the normal asset export path. This
  plugin handles strand data only.
- `Eyelashes_S_Thin` has no cards mesh — strands only.
- The 1.0 code is tested on UE 5.7 and 5.8.2, on Windows. Reports for other engine versions and platforms are very welcome.

## Credits and license

- Original exporter (1.0): **[effectustasi](https://github.com/effectustasi)**,
  [effectustasi/unreal-groom-alembic-exporter](https://github.com/effectustasi/unreal-groom-alembic-exporter).
- 1.1 changes (guide curves, Blender preset, physics settings file, UE 5.8 check):
  [The-Cyril5555](https://github.com/The-Cyril5555).

MIT, see [LICENSE](LICENSE), which keeps the original copyright notice. Not affiliated
with or endorsed by Epic Games. Unreal Engine and MetaHuman are trademarks of Epic Games, Inc.

## More tools by effectustasi

- [agent-receipts](https://github.com/effectustasi/agent-receipts): Skills that make AI coding agents prove "done" with real test output
- [blender-dlss5-neural-rendering](https://github.com/effectustasi/blender-dlss5-neural-rendering): Blender viewport and renders through DLSS 5 neural rendering
- [metahuman-face-capture](https://github.com/effectustasi/metahuman-face-capture): MetaHuman face capture from a webcam in Blender
- [autodesk-inventor-mcp](https://github.com/effectustasi/autodesk-inventor-mcp): Connect AI agents to a live Autodesk Inventor session
