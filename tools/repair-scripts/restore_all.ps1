Copy-Item "operators_git.py" -Destination "project\resources\level_editor\operators.py" -Force
$scripts = @(
    "unmojibake.py",
    "insert.py",
    "insert_helpers.py",
    "update_sanitize.py",
    "update_build.py",
    "update_spawn.py",
    "fix_parse.py",
    "fix_loop.py",
    "fix_pattern.py",
    "revert_pattern.py",
    "fix_shapes.py",
    "rewrite_parse.py",
    "update_ai_pipeline.py",
    "update_ai_partial_regen.py",
    "update_obstacles.py"
)
foreach ($s in $scripts) {
    Write-Host "Running $s..."
    & C:\PROGRA~1\Blender` Foundation\Blender` 4.4\4.4\python\bin\python.exe $s
}
