#pragma once
#include "3D/Object3d.h"

// プリミティブの種類を分かりやすくEnumにしておく
enum class PrimitiveType {
	Plane,
	Sphere,
	Box,
	Ring,
	PartialRing,
	Cylinder,
};

class Primitive : public Object3d {
public:

	// 初期化時に「どの形にするか」を指定できるようにする
	void Initialize(Object3dCommon *object3dCommon, PrimitiveType type);

private:
	PrimitiveType type_;
};

