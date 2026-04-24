#include "Primitive.h"
#include "3D/ModelManager.h"


void Primitive::Initialize(Object3dCommon *object3dCommon, PrimitiveType type) {
	// 1. まずは親クラス(Object3d)の初期化を呼ぶ（これでCBVなどのリソースが作られる）
	Object3d::Initialize(object3dCommon);

	type_ = type;

	// 2. 指定されたタイプに合わせて、モデルを生成＆セットする
	switch (type_) {
	//------------平面------------
	case PrimitiveType::Plane:
		// ModelManagerに「Plane」という名前で平面を作ってもらう
		ModelManager::GetInstance()->CreatePlaneModel("Primitive_Plane");
		// 親クラス(Object3d)の機能を使ってモデルをセット
		SetModel("Primitive_Plane");
		break;
	//------------球体------------
	case PrimitiveType::Sphere:
		ModelManager::GetInstance()->CreateSphereModel("Primitive_Sphere", 16);
		SetModel("Primitive_Sphere");
		break;
	//------------ボックス------------
	case PrimitiveType::Box:
		ModelManager::GetInstance()->CreateBoxModel("Primitive_Box");
		SetModel("Primitive_Box");
		break;
	//------------リング------------
	case PrimitiveType::Ring:
		ModelManager::GetInstance()->CreateRingModel("Primitive_Ring");
		SetModel("Primitive_Ring");
		break;
	//------------部分リング (三日月エフェクト)------------
	case PrimitiveType::PartialRing:
		// UVはV方向(false)、色は内側が透明(a=0)で外側が不透明(a=1)の黄色系、180度、両端30度フェード
		ModelManager::GetInstance()->CreateRingModel("Primitive_PartialRing", 64, 1.2f, 0.4f,
			false, { 1.0f, 1.0f, 0.0f, 0.0f }, { 1.0f, 0.5f, 0.0f, 1.0f }, 0.0f, 180.0f, 30.0f);
		SetModel("Primitive_PartialRing");
		break;
	}
}
