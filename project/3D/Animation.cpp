#include "Animation.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <cmath>
#include "Model.h"

Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename) {
    Animation animation; // 今回作るアニメーション
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;
    const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
    if (!scene || scene->mNumAnimations == 0) {
        animation.duration = 0.0f;
        return animation;
    }
    aiAnimation* animationAssimp = scene->mAnimations[0]; // 最初のアニメーションだけ採用
    float ticksPerSecond = animationAssimp->mTicksPerSecond != 0.0 ? float(animationAssimp->mTicksPerSecond) : 1.0f;
    animation.duration = float(animationAssimp->mDuration / ticksPerSecond); // 時間の単位を秒に変換

    // assimpでは個々のNodeのAnimationをchannelと呼んでいるのでchannelを回してNodeAnimationの情報をとってくる
    for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
        aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
        NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
        
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / ticksPerSecond); // ここも秒に変換
            keyframe.value = {-keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z}; // 右手->左手
            nodeAnimation.translate.keyframes.push_back(keyframe);
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
            aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
            KeyframeQuaternion keyframe;
            keyframe.time = float(keyAssimp.mTime / ticksPerSecond);
            // RotateはQuaternionで、右手->左手に変換するために、yとzを反転させる必要がある。
            keyframe.value = {keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w};
            nodeAnimation.rotate.keyframes.push_back(keyframe);
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / ticksPerSecond);
            // Scaleはそのまま
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
            nodeAnimation.scale.keyframes.push_back(keyframe);
        }
    }
    return animation;
}

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {
    assert(!keyframes.empty()); // キーがないものは返す値がわからないのでダメ
    if (keyframes.size() == 1 || time <= keyframes[0].time) { // キーが1つか、時刻がキーフレーム前なら最初の値とする
        return keyframes[0].value;
    }
    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        // timeがこのキーフレームの区間内か判定
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            // 線形補間(lerp)する
            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
            Vector3 start = keyframes[index].value;
            Vector3 end = keyframes[nextIndex].value;
            return {
                start.x + (end.x - start.x) * t,
                start.y + (end.y - start.y) * t,
                start.z + (end.z - start.z) * t
            };
        }
    }
    // ここまできたら最後のキーフレームより後の時刻
    return keyframes.back().value;
}

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) {
    assert(!keyframes.empty());
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }
    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            // 球面線形補間 (Slerp)
            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
            Quaternion start = keyframes[index].value;
            Quaternion end = keyframes[nextIndex].value;
            
            float dot = start.x * end.x + start.y * end.y + start.z * end.z + start.w * end.w;
            Quaternion q1 = start;
            // ドット積が負の場合は反転して最短経路を通るようにする
            if (dot < 0.0f) {
                q1 = {-start.x, -start.y, -start.z, -start.w};
                dot = -dot;
            }
            // ドット積が1に近い場合は線形補間(Lerp)で済ませる
            if (dot > 0.9995f) {
                Quaternion result = {
                    q1.x + (end.x - q1.x) * t,
                    q1.y + (end.y - q1.y) * t,
                    q1.z + (end.z - q1.z) * t,
                    q1.w + (end.w - q1.w) * t
                };
                // 正規化
                float len = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w);
                return {result.x / len, result.y / len, result.z / len, result.w / len};
            }
            
            // Slerpの計算
            float theta_0 = std::acos(dot);
            float theta = theta_0 * t;
            float sin_theta = std::sin(theta);
            float sin_theta_0 = std::sin(theta_0);
            
            float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
            float s1 = sin_theta / sin_theta_0;
            
            return {
                q1.x * s0 + end.x * s1,
                q1.y * s0 + end.y * s1,
                q1.z * s0 + end.z * s1,
                q1.w * s0 + end.w * s1
            };
        }
    }
    return keyframes.back().value;
}

void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime) {
    for (Joint& joint : skeleton.joints) {
        if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end()) {
            const NodeAnimation& rootNodeAnimation = it->second;
            if (!rootNodeAnimation.translate.keyframes.empty()) {
                joint.transform.translate = CalculateValue(rootNodeAnimation.translate.keyframes, animationTime);
            }
            if (!rootNodeAnimation.rotate.keyframes.empty()) {
                joint.transform.rotate = CalculateValue(rootNodeAnimation.rotate.keyframes, animationTime);
            }
            if (!rootNodeAnimation.scale.keyframes.empty()) {
                joint.transform.scale = CalculateValue(rootNodeAnimation.scale.keyframes, animationTime);
            }
        }
    }
}

#include <fstream>
#include <iostream>

template<typename T>
void WriteBinary(std::ofstream& stream, const T& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template<typename T>
void ReadBinary(std::ifstream& stream, T& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
}

void WriteString(std::ofstream& stream, const std::string& str) {
    uint32_t length = static_cast<uint32_t>(str.size());
    WriteBinary(stream, length);
    if (length > 0) {
        stream.write(str.c_str(), length);
    }
}

void ReadString(std::ifstream& stream, std::string& str) {
    uint32_t length = 0;
    ReadBinary(stream, length);
    if (length > 0) {
        str.resize(length);
        stream.read(&str[0], length);
    } else {
        str.clear();
    }
}

const uint32_t kAnimMagic = 0x414E4142; // "BANA"
const uint32_t kAnimVersion = 1;

bool SaveAnimationToBinary(const Animation& animation, const std::string& filePath) {
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    WriteBinary(file, kAnimMagic);
    WriteBinary(file, kAnimVersion);
    WriteBinary(file, animation.duration);

    uint32_t numNodes = static_cast<uint32_t>(animation.nodeAnimations.size());
    WriteBinary(file, numNodes);

    for (const auto& pair : animation.nodeAnimations) {
        WriteString(file, pair.first);
        const NodeAnimation& nodeAnim = pair.second;

        // Translate
        uint32_t numTranslate = static_cast<uint32_t>(nodeAnim.translate.keyframes.size());
        WriteBinary(file, numTranslate);
        for (const auto& kf : nodeAnim.translate.keyframes) {
            WriteBinary(file, kf.time);
            WriteBinary(file, kf.value);
        }

        // Rotate
        uint32_t numRotate = static_cast<uint32_t>(nodeAnim.rotate.keyframes.size());
        WriteBinary(file, numRotate);
        for (const auto& kf : nodeAnim.rotate.keyframes) {
            WriteBinary(file, kf.time);
            WriteBinary(file, kf.value);
        }

        // Scale
        uint32_t numScale = static_cast<uint32_t>(nodeAnim.scale.keyframes.size());
        WriteBinary(file, numScale);
        for (const auto& kf : nodeAnim.scale.keyframes) {
            WriteBinary(file, kf.time);
            WriteBinary(file, kf.value);
        }
    }

    return true;
}

bool LoadAnimationFromBinary(Animation& outAnimation, const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    uint32_t magic = 0, version = 0;
    ReadBinary(file, magic);
    if (magic != kAnimMagic) return false;

    ReadBinary(file, version);
    if (version != kAnimVersion) return false;

    outAnimation.nodeAnimations.clear();
    ReadBinary(file, outAnimation.duration);

    uint32_t numNodes = 0;
    ReadBinary(file, numNodes);

    for (uint32_t i = 0; i < numNodes; ++i) {
        std::string nodeName;
        ReadString(file, nodeName);
        NodeAnimation& nodeAnim = outAnimation.nodeAnimations[nodeName];

        // Translate
        uint32_t numTranslate = 0;
        ReadBinary(file, numTranslate);
        nodeAnim.translate.keyframes.resize(numTranslate);
        for (uint32_t k = 0; k < numTranslate; ++k) {
            ReadBinary(file, nodeAnim.translate.keyframes[k].time);
            ReadBinary(file, nodeAnim.translate.keyframes[k].value);
        }

        // Rotate
        uint32_t numRotate = 0;
        ReadBinary(file, numRotate);
        nodeAnim.rotate.keyframes.resize(numRotate);
        for (uint32_t k = 0; k < numRotate; ++k) {
            ReadBinary(file, nodeAnim.rotate.keyframes[k].time);
            ReadBinary(file, nodeAnim.rotate.keyframes[k].value);
        }

        // Scale
        uint32_t numScale = 0;
        ReadBinary(file, numScale);
        nodeAnim.scale.keyframes.resize(numScale);
        for (uint32_t k = 0; k < numScale; ++k) {
            ReadBinary(file, nodeAnim.scale.keyframes[k].time);
            ReadBinary(file, nodeAnim.scale.keyframes[k].value);
        }
    }

    return true;
}

