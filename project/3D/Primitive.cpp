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
	}
}
