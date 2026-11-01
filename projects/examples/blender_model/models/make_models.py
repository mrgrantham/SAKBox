"""Makes models.blend, the example's models, as an artist might in Blender.

  blender --background --factory-startup --python make_models.py -- models.blend

- Ship: a subdivided monkey head (Blender's Suzanne), about 15,700 triangles,
  coloured by height, from blue below to white above.
- Ship_DS: the DS's version of it, made by hand: a linked duplicate (sharing
  Ship's mesh, so editing Ship changes it too) with a Decimate modifier,
  which the export applies: about 1,570 triangles. The custom property
  sak.platform = "ds" says what it's for (its name, Ship_DS, says so too).
- Rock: a bumpy sphere, about 20,000 triangles, in browns, with no DS
  version: the build simplifies it for the DS (its BUILD says SIMPLIFY).

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


bpy.ops.mesh.primitive_monkey_add()
ship = bpy.context.active_object
ship.name = "Ship"
apply(ship, "SUBSURF", levels=2)
colour(ship.data, lambda p: (0.15 + 0.4 * (p.z + 1), 0.3 + 0.35 * (p.z + 1), 0.9, 1.0))

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
