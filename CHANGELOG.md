# Changelog

## 1.1 — fork by The-Cyril5555 (verified on UE 5.8.2: see README, Status)

Based on effectustasi's 1.0 (commit `9eaf714`, 2026-10-03).

### Added

- **Guide curve modes** (`EGroomAlembicGuideMode`): *Keep with the strands* (the 1.0
  behaviour, default), *Separate objects* (`/groom/guides_group_N/guides_group_N`, always
  flagged `groom_guide = 1`, imported by Blender as objects named `guides_group_N`) and
  *Leave out*. Blender's Alembic importer drops `groom_guide`, so with 1.0 it rendered the
  guides as extra hair (up to 22 % extra curves on a MetaHuman eyelash groom).
- **Presets** in the options dialog: *Blender* (Y-up, guides left out, physics settings
  on) and *Unreal round trip* (the defaults).
- **Physics settings file**: optional `<name>.physics.json` next to the `.abc` with the
  groom's per-group `HairGroupsPhysics`, `HairGroupsInterpolation` and `HairGroupsInfo`,
  read through reflection, plus a list of the curves objects written.
- **Scripting**: `ExportGroomToAlembicWithOptions(GroomAsset, Filename, Options)`
  returning `FGroomAlembicExportReport`, and `GetBlenderExportOptions()` /
  `GetUnrealRoundTripExportOptions()`.
- The completion notification says how many guide curves were kept, separated or left
  out, and whether a physics settings file was written.
- Tests: `Tests/export_guide_modes.py` (Unreal), `Tests/blender_check_abc.py`
  (Blender) and `Tests/AlembicHarness` (Linux, no Unreal: compiles the real writer of 1.0
  and 1.1 against Alembic and compares their output byte for byte).

### Changed

- `ExportGroomToAlembic` keeps its 1.0 signature and output. `bExportGuideCurves = false`
  is now the *Leave out* mode, which is what it already did. Its `OutNumGroups` is
  documented as the number of hair groups, which is the number of curves objects in
  every case this function can produce.
- The internal `FGroomExportSettings::bExportGuideCurves` is replaced by `GuideMode`.
- A curve counts as a guide when `groom_guide == 1`, as in Unreal's groom builder
  (1.0: any non-zero value). Only grooms with non-standard values are affected.

### Fixed

- *Leave out* (1.0: `bExportGuideCurves = false`) no longer writes the strands'
  `groom_closest_guides` / `groom_guide_weights`. They pointed at the guides it leaves
  out, so a re-import into Unreal bound every strand of a group to one generated guide.
  Only grooms carrying precomputed guide weights are affected.
- README: failures from the context menu raise one notification per batch, not per
  asset; the 1.0 Python function returns `None` on failure.
- The `ExportGroomToAlembic` doc comment sat above `IsExportMenuEntryRegistered`; it is
  back on its function.
- The module depends on `Json` and `JsonUtilities` (both ship with the engine).
- The plugin now enables **AlembicHairImporter** (*Alembic Groom Importer*, ships with the
  engine). Without it Unreal 5.8 cannot import a groom `.abc` at all: `.abc` goes to the
  static mesh importer and the groom factory reports an unknown extension, so a project
  without that plugin could export but not re-import (UE 5.8.2).

### Compatibility

- The 1.0 code compiles and exports correctly on Unreal Engine 5.8.2.
- 1.1 compiles warning-free on Unreal Engine 5.8.2 and passes `Tests/export_guide_modes.py`
  on three MetaHuman grooms (2026-10-31). Not built on 5.7 yet.

## 1.0 — 2026-10-03, effectustasi

Initial release: Groom to Alembic exporter for UE 5.7.
