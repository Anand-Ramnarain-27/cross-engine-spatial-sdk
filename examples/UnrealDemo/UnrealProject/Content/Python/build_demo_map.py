# Creates the demo's one level asset: /Game/Maps/DemoMap, deliberately
# empty. ASpatialSDKDemoGameMode (set as this project's GlobalDefaultGameMode
# in Config/DefaultEngine.ini) spawns the camera pawn and the demo actor in
# code, so the level itself needs nothing placed in it — see
# SpatialSDKDemoGameMode.h for why. Run via:
#   UnrealEditor.exe <uproject> -run=pythonscript -script="Content/Python/build_demo_map.py"

import unreal

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level_subsystem.new_level("/Game/Maps/DemoMap")
level_subsystem.save_current_level()

unreal.log("build_demo_map: wrote /Game/Maps/DemoMap")
