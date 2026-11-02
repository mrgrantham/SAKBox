"""Makes models.blend, the example's models, as an artist might in Blender.

  blender --background --factory-startup --python make_models.py -- models.blend

- Ship: a small spaceship made of Blender's shapes: a smooth hull, light
  above and darker below, a dark canopy, swept wings and a tail fin in
  blue, and two engines with orange nozzles; about 13,000 triangles.
- Ship_DS: the DS's version of it, made by hand: a linked duplicate (sharing
  Ship's mesh, so editing Ship changes it too) with a Decimate modifier,
  which the export applies: about a tenth of the triangles. The custom property
  sak.platform = "ds" says what it's for (its name, Ship_DS, says so too).
- Rock: a bumpy sphere, 5,120 triangles, in browns, with no DS version:
  the build simplifies it for the DS (its BUILD says SIMPLIFY_AGGRESSIVE).

Run it again (with the same Blender) to make the file anew; the BUILD file
exports and cooks it in the build.
"""

import math
import sys

import bpy

bpy.ops.wm.read_factory_settings(use_empty=True)


def colour(mesh, of_corner):
    """Gives `mesh` a colour on each face corner, from of_corner(position).

    The colours are linear light, as Blender's `color` is (it stores them
    as sRGB; the export gives them linear, and the cook back as sRGB)."""
    attribute = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    for loop in mesh.loops:
        attribute.data[loop.index].color = of_corner(mesh.vertices[loop.vertex_index].co)


def apply(obj, kind, **settings):
    modifier = obj.modifiers.new(kind.title(), kind)
    for key, value in settings.items():
        setattr(modifier, key, value)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=modifier.name)


def shape(add, rgb, location=(0, 0, 0), scale=(1, 1, 1), rotation=(0, 0, 0), smooth=0,
          simple=0, shift=None, **settings):
    """A part of the ship: a shape `add` makes, placed, scaled and turned
    (applied to its mesh), its vertices moved by shift(position) if given,
    subdivided (smooth: rounded; simple: more faces, the same shape), and
    coloured `rgb`, or rgb(position) if it's a function."""
    add(**settings)
    part = bpy.context.active_object
    part.location, part.scale, part.rotation_euler = location, scale, rotation
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    if shift:
        for vertex in part.data.vertices:
            vertex.co = shift(vertex.co)
    if smooth:
        apply(part, "SUBSURF", levels=smooth)
    if simple:
        apply(part, "SUBSURF", levels=simple, subdivision_type="SIMPLE")
    colour(part.data, rgb if callable(rgb) else lambda p: (*rgb, 1.0))
    return part


def swept(back):
    """Moves a part's vertices back (-x) the further they are from its middle
    across (y) or up (z): wings and fins swept back"""
    def move(p):
        p = p.copy()
        p.x -= back * (abs(p.y) + max(p.z, 0))
        return p
    return move


BLUE = (0.12, 0.25, 0.7)
GREY = (0.35, 0.37, 0.42)
parts = [
    # The hull: a stretched sphere along x, nose at +x, light above, darker below
    shape(bpy.ops.mesh.primitive_uv_sphere_add, lambda p: (0.75, 0.77, 0.8, 1.0) if p.z > 0
          else (0.4, 0.42, 0.48, 1.0), scale=(1.0, 0.26, 0.2), smooth=2, segments=24, ring_count=12),
    # The canopy, forward and on top
    shape(bpy.ops.mesh.primitive_uv_sphere_add, (0.04, 0.18, 0.25), location=(0.35, 0, 0.12),
          scale=(0.32, 0.13, 0.11), smooth=1, segments=16, ring_count=8),
    # The wings, one flat box across, swept back at the tips
    shape(bpy.ops.mesh.primitive_cube_add, BLUE, location=(-0.15, 0, -0.03),
          scale=(0.28, 0.85, 0.025), simple=3, shift=swept(0.35)),
    # The tail fin, swept back at the top
    shape(bpy.ops.mesh.primitive_cube_add, BLUE, location=(-0.72, 0, 0.2),
          scale=(0.2, 0.02, 0.22), simple=3, shift=swept(0.6)),
]
for side in (-1, 1):
    # An engine either side, along x, and its glowing nozzle behind it
    parts.append(shape(bpy.ops.mesh.primitive_cylinder_add, GREY, location=(-0.62, 0.33 * side, -0.03),
                       rotation=(0, math.pi / 2, 0), scale=(1, 1, 1), simple=2, vertices=20,
                       radius=0.09, depth=0.42))
    parts.append(shape(bpy.ops.mesh.primitive_cylinder_add, (1.0, 0.45, 0.08),
                       location=(-0.85, 0.33 * side, -0.03), rotation=(0, math.pi / 2, 0),
                       vertices=20, radius=0.07, depth=0.04))

# One object, as an artist would join the parts (Ctrl+J), at the origin
for part in parts:
    part.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
ship = bpy.context.active_object
ship.name = "Ship"
ship.scale = (1.3, 1.3, 1.3)  # about the rock's size
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.object.select_all(action="DESELECT")

ship_ds = ship.copy()  # linked: it shares the mesh
ship_ds.name = "Ship_DS"
ship_ds.location.x = 3
bpy.context.collection.objects.link(ship_ds)
decimate = ship_ds.modifiers.new("Decimate", "DECIMATE")
decimate.ratio = 0.1
ship_ds["sak.platform"] = "ds"

bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=5, radius=1)
rock = bpy.context.active_object
rock.name = "Rock"
rock.location.x = -3
for vertex in rock.data.vertices:
    p = vertex.co
    bump = 0.12 * math.sin(7 * p.x) * math.cos(6 * p.y) + 0.08 * math.sin(11 * p.z + 2 * p.x)
    vertex.co = p * (1 + bump)
colour(rock.data, lambda p: (0.45 + 0.2 * p.z, 0.33 + 0.12 * p.z, 0.22, 1.0))

bpy.ops.wm.save_as_mainfile(filepath=sys.argv[sys.argv.index("--") + 1])
