/*
2025/9/2 W2L1
Our code's scene & mesh

Mesh
- make_vao_for_program

LitColorTextureProgram: for shaders
Scene

Our code's asset pipeline
export-meshes.py - open with Blender

export piepline:
run blender file, prints some debug info
../../blender-4.5.2-linux-x64.blender -y --background --python export-meshes.py -- hexapod.blend:Main ../dist/hexapod.pnct

/Applications/Blender.app/Contents/MacOS/Blender -y --background --python scenes/export-meshes.py -- scenes/hexapod.blend:Main /Users/tucheyu/Desktop/ScottyDog/15466/15-466-f25-base2/dist/hexapod.pnct

2025/9/4 W2L2
/Applications/Blender.app/Contents/MacOS/Blender -y --background --python scenes/export-meshes.py -- scenes/inner_mesh_test.blend dist/inner_mesh_test.pnct dist/inner_mesh_test.scene
/Applications/Blender.app/Contents/MacOS/Blender -y --background --python scenes/export-scene.py -- scenes/inner_mesh_test.blend dist/inner_mesh_test.scene

our code (old): use light_distance sth
should update to light_distance sth else
the collection should call Main
*/

// makefile

