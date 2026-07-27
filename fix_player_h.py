import re

path = r"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Player\Player.h"
with open(path, "r", encoding="utf-8") as f:
    text = f.read()

broken_pattern = re.compile(r"(\s*// カメラへの追従（Debug用のカメラではなく、本番用カメラをプレイヤーの後ろに置く処理）\s*)(Vector3 GetForwardVector\(\) const;)", re.MULTILINE | re.DOTALL)

fixed_content = r"""\1
    void UpdateCamera(Camera *camera, const Vector3 *targetPos = nullptr);
    void SyncRotationToLastCameraDirection();

    Vector3 GetPosition() const { return position_; }
    Vector3 GetVelocity() const { return velocity_; }
    Quaternion GetQuaternion() const { return quaternion_; }
    Object3d* GetObject3d() const { return object_.get(); }
    Vector3 GetWorldHalfExtents() const;
    float GetCollisionRadius() const;
    OBB GetOBB() const;
    const std::string& GetModelName() const { return modelName_; }

    \2"""

if broken_pattern.search(text):
    text = broken_pattern.sub(fixed_content, text)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    print("Fixed Player.h!")
else:
    print("Pattern not found in Player.h!")
