"""Exports the open .blend as a .glb, for the cook tool (see README.md).

Run by //tools/assets' rules as
  blender --background --factory-startup model.blend --python export_gltf.py -- out.glb

Every exporter option the cook relies on is set here, so a Blender update
can't change them: modifiers applied (a Decimate on Ship_DS takes effect),
custom properties as glTF extras (sak.platform), the active colour attribute
as vertex colours, Y up. Animations, skins, shape keys, cameras and lights
are left out: models are rigid parts, for now.
"""

import sys

import bpy

output = sys.argv[sys.argv.index("--") + 1]
bpy.ops.export_scene.gltf(
    filepath=output,
    export_format="GLB",
    export_apply=True,
    export_extras=True,
    export_vertex_color="ACTIVE",
    export_normals=True,
    export_texcoords=True,
    export_yup=True,
    export_animations=False,
    export_skins=False,
    export_morph=False,
    export_cameras=False,
    export_lights=False,
)
