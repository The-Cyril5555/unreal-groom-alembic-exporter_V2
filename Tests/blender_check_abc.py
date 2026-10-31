"""
Blender-side check of exported grooms: what Blender actually imports from each .abc.

For every file it lists the curves objects Blender creates, with their curve / point
counts, the attributes Blender kept, and the world-space Z range (a Y-up export of a
MetaHuman should land at head height, about 150-185 cm). Blender names the strand
objects after the /groom transform (groom, groom.001, ...); the guide curves written by
the "separate objects" guide mode come in as guides_group_N (or guides). A file exported
with guides kept with the strands (the Unreal round trip mode) has no such object: its
guides are inside the strand objects and get rendered as hair, since Blender drops the
groom_guide attribute (the "attributes" line shows what Blender kept).

Run with Blender in the background (no UI), one or more files after "--":

  blender --background --factory-startup --python Tests/blender_check_abc.py -- Eyelashes_mixed.abc Eyelashes_separate.abc Eyelashes_exclude.abc

Comparing the three exports of Tests/export_guide_modes.py shows the guide curves:
mixed and separate hold the same total, exclude holds the render strands only.
"""

import os
import sys

import bpy
from mathutils import Vector


def files_from_argv():
    argv = sys.argv
    return argv[argv.index("--") + 1:] if "--" in argv else []


def curve_counts(obj):
    """(curves, points) of a curves object, or None for anything else."""
    if obj.type == "CURVES":  # Blender 3.5+ hair curves, what the Alembic importer creates for curves.
        return len(obj.data.curves), len(obj.data.points)
    if obj.type == "CURVE":  # Legacy curve object, older Blender versions.
        splines = obj.data.splines
        return len(splines), sum(len(s.points) + len(s.bezier_points) for s in splines)
    return None


def attribute_names(obj):
    if obj.type != "CURVES":
        return []
    return sorted(a.name for a in obj.data.attributes if not a.name.startswith("."))


def z_range(obj):
    zs = [(obj.matrix_world @ Vector(corner)).z for corner in obj.bound_box]
    return min(zs), max(zs)


def check(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    before = set(bpy.data.objects)
    bpy.ops.wm.alembic_import(filepath=os.path.abspath(path))
    imported = [o for o in bpy.data.objects if o not in before]
    bpy.context.view_layer.update()

    print("==== %s" % path)
    totals = {"render": [0, 0], "guides": [0, 0]}
    for obj in sorted(imported, key=lambda o: o.name):
        counts = curve_counts(obj)
        if counts is None:
            print("  %-28s %s" % (obj.name, obj.type))
            continue
        kind = "guides" if obj.name.startswith("guides") or obj.data.name.startswith("guides") else "render"
        totals[kind][0] += counts[0]
        totals[kind][1] += counts[1]
        z0, z1 = z_range(obj)
        print("  %-28s %-6s curves=%-7d points=%-9d z=%.1f..%.1f" % (obj.name, kind, counts[0], counts[1], z0, z1))
        names = attribute_names(obj)
        if names:
            print("  %-28s attributes: %s" % ("", ", ".join(names)))

    render, guides = totals["render"], totals["guides"]
    print("  total: render objects %d curves / %d points, guide objects %d curves / %d points"
          % (render[0], render[1], guides[0], guides[1]))
    return render[0] + guides[0]


def main():
    files = files_from_argv()
    if not files:
        print("usage: blender --background --factory-startup --python blender_check_abc.py -- <file.abc> [...]")
        return 1
    missing = [f for f in files if not os.path.isfile(f)]
    if missing:
        print("missing file(s): %s" % ", ".join(missing))
        return 1
    for path in files:
        check(path)
    return 0


if __name__ == "__main__":
    code = main()
    if code:
        sys.exit(code)
