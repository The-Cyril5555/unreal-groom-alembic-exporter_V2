"""
Checks the three guide modes and the physics settings file (plugin 1.1).

For each groom in the Grooms folder it:
  1. exports the groom three times: guides kept with the strands (Mixed, the Unreal
     round trip), guides in separate objects (Separate), guides left out (Exclude);
  2. checks the counts agree:
       Separate writes the same curves as Mixed, and the same number of guides;
       Exclude writes Mixed's curves minus its guides, and reports them as left out;
  3. re-imports the Separate file into Unreal and exports it again in Mixed mode. The
     same curve and guide counts prove Unreal reads the guides_* objects back as guides.
     Done twice: with the groom_* attributes (the default) and without them, where only
     the explicit groom_guide = 1 on the guide objects marks them as guides;
  4. exports with the Blender preset and checks the .physics.json next to the .abc
     parses, then prints the stiffness / damping / gravity / collision values it holds.

It also prints how many extra curves the guides add on top of the real strands: that is
what Blender renders as extra hair when the guides are kept in the file.

Run with:
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript
      -script="<path>/Plugins/GroomExport/Tests/export_guide_modes.py"
      -unattended -nosplash -nopause -stdout
"""

import json
import os
import sys

import unreal

GROOM_PACKAGE_DIR = "/Game/MetaHumans/NewMetaHumanCharacter/Grooms"
IMPORT_DEST = "/Game/GroomExportGuideModes"
OUT_DIR = os.path.abspath(os.path.join(unreal.Paths.project_saved_dir(), "GroomGuideModes"))

LIB = unreal.GroomExportBlueprintLibrary
MODES = (
    ("mixed", unreal.GroomAlembicGuideMode.MIXED),
    ("separate", unreal.GroomAlembicGuideMode.SEPARATE),
    ("exclude", unreal.GroomAlembicGuideMode.EXCLUDE),
)

# Physics values worth printing, matched case-insensitively against the JSON keys.
PHYSICS_KEYWORDS = ("stiffness", "damping", "gravity", "drag", "friction", "collision",
                    "viscosity", "radius", "enablesimulation", "substeps", "iterationcount")


def find_grooms():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    grooms = []
    for data in registry.get_assets_by_path(GROOM_PACKAGE_DIR, recursive=True):
        asset = data.get_asset()
        if isinstance(asset, unreal.GroomAsset):
            grooms.append(asset)
    return sorted(grooms, key=lambda a: a.get_name())


def export(groom, path, mode, physics=False, blender=False, attributes=True):
    options = LIB.get_blender_export_options() if blender else LIB.get_unreal_round_trip_export_options()
    options.guide_mode = mode
    options.write_physics_settings = physics
    options.export_groom_attributes = attributes
    return LIB.export_groom_to_alembic_with_options(groom, path, options)


def reimport(abc_path, asset_name):
    task = unreal.AssetImportTask()
    task.filename = abc_path
    task.destination_path = IMPORT_DEST
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = unreal.GroomImportOptions()

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    for path in task.get_editor_property("imported_object_paths") or []:
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.GroomAsset):
            return asset
    return None


def check_reimport(name, label, abc_path, mixed, guides):
    """Re-imports abc_path, exports it again in Mixed mode, compares with the original export."""
    imported = reimport(abc_path, "%s_%s" % (name, label.upper()))
    if imported is None:
        unreal.log_error("FAIL %-24s re-import of the %s file produced no GroomAsset" % (name, label))
        return False
    back = export(imported, os.path.join(OUT_DIR, "%s_%s_rt.abc" % (name, label)), unreal.GroomAlembicGuideMode.MIXED)
    if not back.success:
        unreal.log_error("FAIL %-24s export after re-import of the %s file: %s" % (name, label, back.error))
        return False
    got = (back.num_curves, back.num_points, back.num_guide_curves)
    expected = (mixed.num_curves, mixed.num_points, guides)
    if got != expected:
        unreal.log_error("DIFF %-24s %s file after re-import: curves=%d points=%d guides=%d, expected %d / %d / %d"
                         % ((name, label) + got + expected))
        return False
    unreal.log("OK   %-24s %s file re-imports with its %d guides" % (name, label, guides))
    return True


def walk_physics(node, path, out):
    """Collects (path, value) for the keys matching PHYSICS_KEYWORDS."""
    if isinstance(node, dict):
        for key, value in node.items():
            child = "%s.%s" % (path, key) if path else key
            # A scalar, or a flat struct such as a vector {"x":..,"y":..,"z":..}.
            is_leaf = not isinstance(value, (dict, list)) or (
                isinstance(value, dict) and all(not isinstance(v, (dict, list)) for v in value.values()))
            if is_leaf and any(word in key.lower() for word in PHYSICS_KEYWORDS):
                out.append((child, value))
            elif isinstance(value, (dict, list)):
                walk_physics(value, child, out)
    elif isinstance(node, list):
        for index, value in enumerate(node):
            walk_physics(value, "%s[%d]" % (path, index), out)


def check_physics(name, report):
    path = report.physics_settings_file
    if not path or not os.path.isfile(path):
        unreal.log_error("FAIL %-24s no physics settings file (got %r)" % (name, path))
        return False
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)

    groups = data.get("groups", [])
    physics_groups = [g for g in groups if "physics" in g]
    if not physics_groups:
        unreal.log_error("FAIL %-24s %s has no groups[].physics" % (name, path))
        return False

    unreal.log("OK   %-24s physics: %d group(s) in %s" % (name, len(physics_groups), os.path.basename(path)))
    if data.get("not_found_in_this_engine_version"):
        unreal.log("     not found in this engine version: %s" % data["not_found_in_this_engine_version"])

    values = []
    walk_physics(physics_groups[0]["physics"], "groups[0].physics", values)
    for key, value in values[:40]:
        unreal.log("       %s = %s" % (key, json.dumps(value)))
    return True


def main():
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)

    grooms = find_grooms()
    if not grooms:
        unreal.log_error("No groom assets found under %s" % GROOM_PACKAGE_DIR)
        return 1

    failures = 0
    for groom in grooms:
        name = groom.get_name()
        reports = {}
        for tag, mode in MODES:
            path = os.path.join(OUT_DIR, "%s_%s.abc" % (name, tag))
            report = export(groom, path, mode)
            if not report.success:
                failures += 1
                unreal.log_error("FAIL %-24s %s export: %s" % (name, tag, report.error))
                break
            reports[tag] = report
        if len(reports) != len(MODES):
            continue

        mixed, separate, exclude = reports["mixed"], reports["separate"], reports["exclude"]
        guides = mixed.num_guide_curves
        strands = mixed.num_curves - guides

        problems = []
        if separate.num_curves != mixed.num_curves:
            problems.append("separate curves %d != mixed %d" % (separate.num_curves, mixed.num_curves))
        if separate.num_guide_curves != guides:
            problems.append("separate guides %d != mixed %d" % (separate.num_guide_curves, guides))
        if exclude.num_curves != strands:
            problems.append("exclude curves %d != %d strands" % (exclude.num_curves, strands))
        if exclude.num_guide_curves != 0 or exclude.num_guide_curves_skipped != guides:
            problems.append("exclude wrote %d guides, left out %d (expected 0 / %d)"
                            % (exclude.num_guide_curves, exclude.num_guide_curves_skipped, guides))
        expected_objects = mixed.num_curve_objects + (mixed.num_groups if guides else 0)
        if guides and separate.num_curve_objects > expected_objects:
            problems.append("separate wrote %d objects, expected at most %d"
                            % (separate.num_curve_objects, expected_objects))

        extra = (100.0 * guides / strands) if strands else 0.0
        unreal.log("%s %-24s strands=%-7d guides=%-6d (+%.1f%% curves if guides are rendered)  objects: mixed=%d separate=%d"
                   % ("OK  " if not problems else "DIFF", name, strands, guides, extra,
                      mixed.num_curve_objects, separate.num_curve_objects))
        for problem in problems:
            unreal.log_error("     %s" % problem)
        if problems:
            failures += 1
            continue

        # Separate -> Unreal -> Mixed must give back the same strands and guides.
        if not check_reimport(name, "separate", separate.alembic_file, mixed, guides):
            failures += 1

        # Same without the groom_* attributes: the guide objects' own groom_guide = 1 is
        # then the only thing that marks them (Unreal may regroup the curves, counts stay).
        bare = export(groom, os.path.join(OUT_DIR, "%s_separate_noattrs.abc" % name),
                      unreal.GroomAlembicGuideMode.SEPARATE, attributes=False)
        if not bare.success:
            failures += 1
            unreal.log_error("FAIL %-24s separate export without attributes: %s" % (name, bare.error))
        elif not check_reimport(name, "separate_noattrs", bare.alembic_file, mixed, guides):
            failures += 1

        # Blender preset + physics settings file.
        blender = export(groom, os.path.join(OUT_DIR, "%s_blender.abc" % name),
                         unreal.GroomAlembicGuideMode.EXCLUDE, physics=True, blender=True)
        if not blender.success:
            failures += 1
            unreal.log_error("FAIL %-24s blender preset export: %s" % (name, blender.error))
        elif not check_physics(name, blender):
            failures += 1

    unreal.log("---- %d groom(s), %d failure(s) ----" % (len(grooms), failures))
    return 1 if failures else 0


sys.exit(main())
