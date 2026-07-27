#pragma once
#include "3D/Object3d.h"
#include "3D/Trail.h"
#include "engine/math/MyMath.h"
#include "engine/Camera/Camera.h"
#include <memory>
#include <vector>
#include <string>

class BoosterEffect {
public:
    void Initialize();
    void Update(const Vector3& position, const Quaternion& rotation, int playerMode, float speedRatio, bool isAccelerating);
    void Draw(Camera* camera);

private:
    struct Burner {
        std::unique_ptr<Trail> trail;
        std::unique_ptr<Object3d> trailObject;
        Vector3 offset;
        Vector3 defaultScale;
        Vector3 currentScale;
        Vector4 color;
    };

    std::vector<Burner> burners_;
    int lastMode_ = -1;
    float time_ = 0.0f;

    void SetupBurnersForMode(int playerMode);
};
