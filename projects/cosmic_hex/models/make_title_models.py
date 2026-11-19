"""Makes title_models.blend: the title screen's model, a
placeholder to be replaced by the user's own.

  blender --background --factory-startup --python make_title_models.py -- title_models.blend

- Rocket: a retro rocket, lying along +x and tilted 15 degrees nose up, on a
  thin rectangular rod rising from the middle of a stepped round pedestal,
  like a game piece. A white body, a red nose cone and three red fins, a
  blue porthole, a dark nozzle; the rod grey, the pedestal dark blue with a
  lighter top. Vertex colours only (no textures: the Tron look). One mesh,
  so it turns as one piece. Its pedestal's foot is at the origin.

To replace it: open title_models.blend in Blender and change or replace the
object named Rocket (keep the name, or change `object` in BUILD); the build
exports and cooks it, and the DS's version is simplified to its budget.
"""

import math
import sys

import bpy

bpy.ops.wm.read_factory_settings(use_empty=True)

SEGMENTS = 16  # round parts' sides


def linear(hex_colour):
    """An sRGB colour ('#rrggbb') as Blender's linear light, with alpha."""
    out = []
    for i in (1, 3, 5):
        c = int(hex_colour[i:i + 2], 16) / 255
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return (*out, 1.0)


def coloured(obj, hex_colour):
    """Gives every corner of `obj`'s mesh one colour."""
    attribute = obj.data.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    for item in attribute.data:
        item.color = linear(hex_colour)
    return obj


def cylinder(radius, depth, location, rotation=(0, 0, 0), vertices=SEGMENTS, cone_to=None):
    if cone_to is None:
        bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth,
                                            location=location, rotation=rotation)
    else:
        bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius, radius2=cone_to,
                                        depth=depth, location=location, rotation=rotation)
    return bpy.context.active_object


def box(size, location, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location, rotation=rotation)
    obj = bpy.context.active_object
    obj.scale = size
    return obj


parts = []
# The pedestal: two hexagonal steps (the user's: hexagons, for
# the game's name), the foot at z = 0
parts.append(coloured(cylinder(0.9, 0.2, (0, 0, 0.1), vertices=6), "#2b3a5c"))
parts.append(coloured(cylinder(0.62, 0.15, (0, 0, 0.275), vertices=6), "#4a6290"))
# The rod: thin and rectangular, up from the pedestal's middle
parts.append(coloured(box((0.07, 0.22, 0.95), (0, 0, 0.35 + 0.475)), "#9aa4b4"))

# The rocket, built along +x round its own middle, then tilted and raised
rocket = []
along = (0, math.pi / 2, 0)  # a cylinder's axis (z) turned to lie along x
rocket.append(coloured(cylinder(0.32, 1.5, (0, 0, 0), along), "#f2f0ea"))       # body
rocket.append(coloured(cylinder(0.32, 0.7, (1.1, 0, 0), along, cone_to=0), "#d84a3a"))  # nose
rocket.append(coloured(cylinder(0.24, 0.3, (-0.9, 0, 0), along, cone_to=0.16), "#3a3d46"))  # nozzle
porthole = cylinder(0.12, 0.08, (0.25, -0.3, 0.05), (math.pi / 2, 0, 0), vertices=12)
rocket.append(coloured(porthole, "#62b7e8"))
for k in range(3):
    # A fin: a thin swept slab at the tail, a third of a turn apart
    angle = math.pi / 2 + k * 2 * math.pi / 3
    fin = box((0.55, 0.05, 0.42), (-0.55, 0.42 * math.cos(angle), 0.42 * math.sin(angle)),
              (angle - math.pi / 2, 0, 0))
    fin.data.vertices[0].co.x += 0.25  # (swept: the leading edge's root forward)
    rocket.append(coloured(fin, "#d84a3a"))
# (each part's own turn and place applied first, so the joined rocket lies
# along x with no turn of its own, ready to tilt)
for obj in rocket:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.object.select_all(action="DESELECT")
for obj in rocket:
    obj.select_set(True)
bpy.context.view_layer.objects.active = rocket[0]
bpy.ops.object.join()
body = bpy.context.active_object
body.rotation_euler = (0, math.radians(-15), 0)  # nose up
body.location = (0.1, 0, 1.42)
parts.append(body)

# One mesh: the pedestal, the rod and the rocket
for obj in parts:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.object.select_all(action="DESELECT")
for obj in parts:
    obj.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
whole = bpy.context.active_object
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
whole.name = "Rocket"
whole.data.name = "Rocket"
print("Rocket:", sum(len(p.vertices) - 2 for p in whole.data.polygons), "triangles")

out = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.save_as_mainfile(filepath=out)
