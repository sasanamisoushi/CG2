#pragma once

#include "MyMath.h"
#include <vector>
#include <map>
#include <string>

template <typename tValue>
struct Keyframe {
    float time;
    tValue value;
};
using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

template<typename tValue>
struct AnimationCurve {
    std::vector<Keyframe<tValue>> keyframes;
};

struct NodeAnimation {
    AnimationCurve<Vector3> translate;
    AnimationCurve<Quaternion> rotate;
    AnimationCurve<Vector3> scale;
};

struct Animation {
    float duration; // アニメーション全体の尺 (単位は秒)
    // NodeAnimationの集合。Node名でひけるようにしておく
    std::map<std::string, NodeAnimation> nodeAnimations;
};

// アニメーションを読み込む
Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

// 任意の時刻の値を取得する
Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

struct Skeleton;
void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);
