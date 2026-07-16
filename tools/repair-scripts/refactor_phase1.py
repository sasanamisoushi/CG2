import sys
import re

def process_header(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Remove the primitive parameter definitions
    content = re.sub(r'^\s*int\s+prSubdivision\s*=.*?\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+prOuterRadius\s*=.*?\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+prInnerRadius\s*=.*?\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*bool\s+prIsUvHorizontal\s*=.*?\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+prInnerColor.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+prOuterColor.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+prStartAngle.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+prEndAngle.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+prFadeAngle.*?;\n', '', content, flags=re.MULTILINE)

    content = re.sub(r'^\s*float\s+cylinderPos.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderScale.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderUVOffset.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderUVScrollSpeed.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderAlphaReference.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*int\s+cylinderSubdivision.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*int\s+cylinderVerticalSubdivision.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderTopRadiusX.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderTopRadiusZ.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderBottomRadiusX.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderBottomRadiusZ.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderHeight.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderTopColor.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderBottomColor.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderStartAngle.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*float\s+cylinderEndAngle.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*bool\s+cylinderIsUvFlipped.*?;\n', '', content, flags=re.MULTILINE)

    content = re.sub(r'^\s*bool\s+showNormalRing\s*=.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*bool\s+showPartialRing\s*=.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*bool\s+showCylinder\s*=.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*bool\s+showSkybox\s*=.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*bool\s+showParticles\s*=.*?;\n', '', content, flags=re.MULTILINE)

    # Remove the unique_ptrs
    content = re.sub(r'^\s*std::unique_ptr<Skybox>\s+skybox.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*std::unique_ptr<ParticleManager>\s+particleManager.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*std::unique_ptr<ParticleEmitter>\s+particleEmitter.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*std::unique_ptr<Primitive>\s+boundaryAlertPlane_.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*std::unique_ptr<Primitive>\s+myRing.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*std::unique_ptr<Primitive>\s+myPartialRing.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*std::unique_ptr<Primitive>\s+myCylinder.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*std::unique_ptr<Trail>\s+missileTrail.*?;\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*std::unique_ptr<Object3d>\s+trailObject.*?;\n', '', content, flags=re.MULTILINE)

    # Ensure EnvironmentRenderer is included if missing
    if '#include "EnvironmentRenderer.h"' not in content:
        content = content.replace('#include "3D/Trail.h"', '#include "3D/Trail.h"\n#include "EnvironmentRenderer.h"')

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

def process_cpp(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    new_lines = []
    skip = False
    for i, line in enumerate(lines):
        # Skip initialization blocks
        if "skybox = std::make_unique<Skybox>();" in line:
            skip = True
        if "particleEmitter = std::make_unique<ParticleEmitter>" in line:
            skip = True
        if "myRing = std::make_unique<Primitive>();" in line:
            skip = True
        if "myPartialRing = std::make_unique<Primitive>();" in line:
            skip = True
        if "myCylinder = std::make_unique<Primitive>();" in line:
            skip = True
        if "boundaryAlertPlane_ = std::make_unique<Primitive>();" in line:
            skip = True
            
        if skip:
            if line.strip() == "" and i > 0 and not lines[i-1].strip() == "":
                skip = False
            continue

        # Skip drawing logic
        if "if (myRing && showNormalRing) myRing->Draw();" in line: continue
        if "if (myPartialRing && showPartialRing) myPartialRing->Draw();" in line: continue
        if "if (myCylinder && showCylinder) myCylinder->Draw();" in line: continue
        if "if (particleManager) {" in line and "Draw" in lines[i+1]:
            continue
        if "particleManager->Draw();" in line and "if (particleManager)" in lines[i-1]:
            continue
        if line.strip() == "}" and "particleManager->Draw();" in lines[i-1]:
            continue
        if "if (skybox) {" in line and "Draw" in lines[i+1]:
            continue
        if "skybox->Draw();" in line and "if (skybox)" in lines[i-1]:
            continue
        if line.strip() == "}" and "skybox->Draw();" in lines[i-1]:
            continue

        # Skip update logic (skybox is handled specially because it takes camera.get())
        if "skybox->Update(" in line: continue
        if "particleManager->Update(camera.get());" in line: continue
        if "particleEmitter->Update();" in line: continue
        if "if (myRing && showNormalRing) {" in line:
            skip = True
            continue
        if "if (myPartialRing && showPartialRing) {" in line:
            skip = True
            continue
        if "if (myCylinder && showCylinder) {" in line:
            skip = True
            continue
            
        # In Draw, insert environmentRenderer_->Draw() just before explosionManager_
        if "if (explosionManager_) explosionManager_->Draw();" in line:
            new_lines.append("\tenvironmentRenderer_->Draw();\n")
            
        new_lines.append(line)
        
        # Insert environmentRenderer initialization
        if "environmentRenderer_ = std::make_unique<EnvironmentRenderer>();" in line:
            new_lines.append("\tenvironmentRenderer_->Initialize();\n")
            
    with open(filepath, 'w', encoding='utf-8') as f:
        f.writelines(new_lines)


if __name__ == '__main__':
    process_header("project/Game/Scene/GamePlayScene.h")
    process_cpp("project/Game/Scene/GamePlayScene.cpp")
