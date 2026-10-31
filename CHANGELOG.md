# Changelog

## 1.1 — fork by The-Cyril5555 (not yet built: see README, Status)

Based on effectustasi's 1.0 (commit `9eaf714`, 2026-10-03).

### Added

- **Guide curve modes** (`EGroomAlembicGuideMode`): *Keep with the strands* (the 1.0
  behaviour, default), *Separate objects* (`guides_group_N`, always flagged
  `groom_guide = 1`) and *Leave out*. Blender's Alembic importer does not act on
  `groom_guide`, so with 1.0 it rendered the guides as extra hair (up to 22 % extra
  curves on a MetaHuman eyelash groom).
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
- Tests: `Tests/export_guide_modes.py` (Unreal) and `Tests/blender_check_abc.py`
  (Blender).

### Changed

- `ExportGroomToAlembic` keeps its 1.0 signature and output. `bExportGuideCurves = false`
  is now the *Leave out* mode, which is what it already did. Its `OutNumGroups` is
  documented as the number of hair groups, which is the number of curves objects in
  every case this function can produce.
- The internal `FGroomExportSettings::bExportGuideCurves` is replaced by `GuideMode`.
- The `ExportGroomToAlembic` doc comment sat above `IsExportMenuEntryRegistered`; it is
  back on its function.
- The module depends on `Json` and `JsonUtilities` (both ship with the engine).

### Compatibility

- The 1.0 code compiles and exports correctly on Unreal Engine 5.8.2.

## 1.0 — 2026-10-03, effectustasi

Initial release: Groom to Alembic exporter for UE 5.7.
