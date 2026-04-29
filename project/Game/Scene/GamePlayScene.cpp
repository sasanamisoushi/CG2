#include "GamePlayScene.h"
#include "3D/ModelManager.h"
#include <Windows.h>
#include "engine/Graphics/DirectXCommon.h"
#include "2D/SpriteCommon.h"
#include "3D/Object3dCommon.h"
#include "engine/Input/Input.h"
#include "engine/Debug/ImGuiManager.h"
#include <externals/imgui/imgui.h>



void GamePlayScene::Initialize() {

	//カメラ・シーンリソース
	camera = std::make_unique<Camera>();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());

	//スプライトの初期化
	sprite = std::make_unique<Sprite>();
	sprite->Initialize(SpriteCommon::GetInstance() , "resources/uvChecker.png");

	// SkyboxCommon に DirectX の情報を渡して初期化する！
	SkyboxCommon::GetInstance()->Initialize(DirectXCommon::GetInstance());

	// スカイボックスの生成と初期化
	skybox = std::make_unique<Skybox>();
	skybox->Initialize("resources/SkyBox.dds");

	//モデル・パーティクル
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("multiMesh.obj");
	ModelManager::GetInstance()->CreateSphereModel("Sphere", 16);

	////1つ目のオブジェクト
	//objA = std::make_unique<Object3d>();
	//objA->Initialize(Object3dCommon::GetInstance());
	//objA->SetModel("plane.obj");
	//objA->transform.translate = { -2.0f,0.0f,0.0f };
	//objects.push_back(objA.get());

	//２つ目のオブジェクト
	/*objB = std::make_unique<Object3d>();
	objB->Initialize(Object3dCommon::GetInstance());
	objB->SetModel("Sphere");
	objB->transform.translate = { 2.0f,0.0f,0.0f };
	objects.push_back(objB.get());*/

	//======================================================
	// プリミティブの生成！
	//======================================================

	// 平面（元のチェッカーボード）
	myPlane = std::make_unique<Primitive>();
	myPlane->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Plane);
	myPlane->SetTranslate({ 0.0f, 2.0f, 0.0f }); // 元の上の方の配置
	objects.push_back(myPlane.get());

	// 球体
	myShere = std::make_unique<Primitive>();
	myShere->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Sphere);
	myShere->SetTranslate({ 2.0f,0.0f,0.0f });
	objects.push_back(myShere.get());

	// ボックス
	myBox = std::make_unique<Primitive>();
	myBox->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Box);
	myBox->SetTranslate({ -2.0f,0.0f,0.0f });
	objects.push_back(myBox.get());

	// リング
	myRing = std::make_unique<Primitive>();
	myRing->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Ring);
	myRing->SetTranslate({ 0.0f, 0.0f, 0.0f }); // パーティクルと同じ中心位置に合わせる

	// 部分リング (三日月)
	myPartialRing = std::make_unique<Primitive>();
	myPartialRing->Initialize(Object3dCommon::GetInstance(), PrimitiveType::PartialRing);
	myPartialRing->SetTranslate({ 0.0f, 0.0f, 0.0f });

	// 円柱エフェクト
	myCylinder = std::make_unique<Primitive>();
	myCylinder->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Cylinder);
	myCylinder->SetTranslate({ 0.0f, 0.0f, 0.0f });
	myCylinder->SetScale({ 2.0f, 2.0f, 2.0f });

	//パーティクル
	particleManager = std::make_unique<ParticleManager>();
	particleManager->Initialize(DirectXCommon::GetInstance());
	particleManager->CreateParticleGroup("test", "resources/circle2.png");
	particleEmitter = std::make_unique<ParticleEmitter>("test", Vector3{ 0.0f,0.0f,0.0f }, particleManager.get());

	//音声再生
	soundData1 = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");
	soundData2 = AudioManager::GetInstance()->LoadAudio("resources/maou_bgm_fantasy15.mp3");

	pVoice1=AudioManager::GetInstance()->PlayWave(soundData1, true);
	pVoice2=AudioManager::GetInstance()->PlayWave(soundData2, true);

}

void GamePlayScene::Finalize() {
	if (pVoice1) {
		pVoice1->Stop();
		pVoice2->DestroyVoice();
	}

	AudioManager::GetInstance()->UnloadWave(soundData1);
	AudioManager::GetInstance()->UnloadWave(soundData2);
}

void GamePlayScene::Update() {
	if (Input::GetInstance()->TriggerKey(DIK_0)) {
		OutputDebugStringA("HIt 0\n");
	}

	//カメラの更新
	camera->Update();

	// カメラの更新後にスカイボックスも更新（カメラの行列を渡す）
	skybox->Update(camera.get());

	if (myRing && showNormalRing) {
		static float ringTime = 0.0f;
		ringTime += 0.05f;

		// オブジェクト自体の回転や拡縮は行わず、UVスクロールのみでエフェクトを表現する
		// パーティクルのエフェクトを囲むようにスケールを調整
		myRing->SetRotate({ 0.0f, 0.0f, 0.0f });
		myRing->SetScale({ 2.0f, 2.0f, 1.0f });

		// UVスクロールとスケーリング
		Model* ringModel = myRing->GetModel();
		if (ringModel) {
			Vector3 uvScale = { 10.0f, 1.0f, 1.0f }; // U方向にScaleして細かい模様にする
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			// 資料の指示通り、U方向（X成分）を時間でスクロールさせて円を回転させる
			Vector3 uvTranslate = { ringTime * 0.1f, 0.0f, 0.0f }; 
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			ringModel->SetUvTransform(uvTransform);
		}
		myRing->Update();
	}

	if (myPartialRing && showPartialRing) {
		static float pRingTime = 0.0f;
		pRingTime += 0.05f;

		// 部分リングはV方向をスクロールさせたり、Z軸回転させたりしてアニメーションできる
		myPartialRing->SetRotate({ 0.0f, 0.0f, pRingTime * -0.5f }); // Z回転で三日月を回す
		myPartialRing->SetScale({ 2.0f, 2.0f, 1.0f });

		Model* pRingModel = myPartialRing->GetModel();
		if (pRingModel) {
			Vector3 uvScale = { 1.0f, 10.0f, 1.0f }; // V方向にScale
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			Vector3 uvTranslate = { 0.0f, pRingTime * 0.1f, 0.0f }; // V方向へスクロール
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			pRingModel->SetUvTransform(uvTransform);
		}
		myPartialRing->Update();
	}

	if (myCylinder && showCylinder) {
		cylinderUVOffset[0] += cylinderUVScrollSpeed[0];
		cylinderUVOffset[1] += cylinderUVScrollSpeed[1];

		Model* cModel = myCylinder->GetModel();
		if (cModel) {
			Vector3 uvScale = { 1.0f, 1.0f, 1.0f };
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			Vector3 uvTranslate = { cylinderUVOffset[0], cylinderUVOffset[1], 0.0f };
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			cModel->SetUvTransform(uvTransform);
			cModel->SetAlphaReference(cylinderAlphaReference);
		}
		
		// ImGuiの変数をCylinderに適用
		myCylinder->SetTranslate({ cylinderPos[0], cylinderPos[1], cylinderPos[2] });
		myCylinder->SetScale({ cylinderScale[0], cylinderScale[1], cylinderScale[2] });

		myCylinder->Update();
	}

	for (Object3d *object3d : objects) {
		object3d->Update();
	}

	Vector2 size = sprite->GetSize();
	size.x = 300.0f;
	size.y = 300.0f;
	sprite->SetSize(size);

	particleEmitter->Update();
	particleManager->Update(camera.get());

#ifdef ENABLE_IMGUI
	//開発用UIの処理
	ImGui::ShowDemoWindow();

	//ウィンドウのサイズを設定
	ImGui::SetNextWindowSize(ImVec2(500.0f, 400.0f), ImGuiCond_Once);

	//ウィンドウの作成
	ImGui::Begin("Exercise");

	ImGui::Text("Ring Settings");
	ImGui::Checkbox("Show Normal Ring", &showNormalRing);
	ImGui::Checkbox("Show Partial Ring", &showPartialRing);
	ImGui::Checkbox("Show Cylinder", &showCylinder);

	ImGui::Separator();
	ImGui::Text("Cylinder Settings");
	ImGui::DragFloat3("Cylinder Position", cylinderPos, 0.01f);
	ImGui::DragFloat3("Cylinder Scale", cylinderScale, 0.01f);
	ImGui::DragFloat2("UV Scroll Speed", cylinderUVScrollSpeed, 0.001f);
	ImGui::SliderFloat("Alpha Reference", &cylinderAlphaReference, 0.0f, 1.0f);

	bool cChanged = false;
	if (ImGui::SliderInt("Subdivision##Cyl", &cylinderSubdivision, 3, 128)) cChanged = true;
	if (ImGui::SliderInt("Vertical Subdivision", &cylinderVerticalSubdivision, 1, 32)) cChanged = true;
	if (ImGui::DragFloat("Top Radius X", &cylinderTopRadiusX, 0.01f)) cChanged = true;
	if (ImGui::DragFloat("Top Radius Z", &cylinderTopRadiusZ, 0.01f)) cChanged = true;
	if (ImGui::DragFloat("Bottom Radius X", &cylinderBottomRadiusX, 0.01f)) cChanged = true;
	if (ImGui::DragFloat("Bottom Radius Z", &cylinderBottomRadiusZ, 0.01f)) cChanged = true;
	if (ImGui::DragFloat("Height", &cylinderHeight, 0.01f)) cChanged = true;
	if (ImGui::ColorEdit4("Top Color", cylinderTopColor)) cChanged = true;
	if (ImGui::ColorEdit4("Bottom Color", cylinderBottomColor)) cChanged = true;
	if (ImGui::SliderFloat("Start Angle##Cyl", &cylinderStartAngle, 0.0f, 360.0f)) cChanged = true;
	if (ImGui::SliderFloat("End Angle##Cyl", &cylinderEndAngle, 0.0f, 360.0f)) cChanged = true;
	if (ImGui::Checkbox("Flip UV", &cylinderIsUvFlipped)) cChanged = true;

	if (myCylinder && cChanged) {
		Model* model = myCylinder->GetModel();
		if (model) {
			model->InitializeCylinder(model->GetModelCommon(), 
				cylinderSubdivision, cylinderVerticalSubdivision,
				cylinderTopRadiusX, cylinderTopRadiusZ,
				cylinderBottomRadiusX, cylinderBottomRadiusZ,
				cylinderHeight,
				{ cylinderTopColor[0], cylinderTopColor[1], cylinderTopColor[2], cylinderTopColor[3] },
				{ cylinderBottomColor[0], cylinderBottomColor[1], cylinderBottomColor[2], cylinderBottomColor[3] },
				cylinderStartAngle, cylinderEndAngle,
				cylinderIsUvFlipped);
		}
	}

	ImGui::Separator();
	ImGui::Text("Partial Ring Settings");

	bool pRingChanged = false;
	if (ImGui::SliderInt("Subdivision", &prSubdivision, 3, 128)) pRingChanged = true;
	if (ImGui::SliderFloat("Outer Radius", &prOuterRadius, 0.1f, 5.0f)) pRingChanged = true;
	if (ImGui::SliderFloat("Inner Radius", &prInnerRadius, 0.1f, 5.0f)) pRingChanged = true;
	if (ImGui::Checkbox("UV Horizontal", &prIsUvHorizontal)) pRingChanged = true;
	if (ImGui::ColorEdit4("Inner Color", prInnerColor)) pRingChanged = true;
	if (ImGui::ColorEdit4("Outer Color", prOuterColor)) pRingChanged = true;
	if (ImGui::SliderFloat("Start Angle", &prStartAngle, 0.0f, 360.0f)) pRingChanged = true;
	if (ImGui::SliderFloat("End Angle", &prEndAngle, 0.0f, 360.0f)) pRingChanged = true;
	if (ImGui::SliderFloat("Fade Angle", &prFadeAngle, 0.0f, 180.0f)) pRingChanged = true;

	if (pRingChanged && myPartialRing) {
		Model* model = myPartialRing->GetModel();
		if (model) {
			model->InitializeRing(
				model->GetModelCommon(), prSubdivision, prOuterRadius, prInnerRadius,
				prIsUvHorizontal, { prInnerColor[0], prInnerColor[1], prInnerColor[2], prInnerColor[3] },
				{ prOuterColor[0], prOuterColor[1], prOuterColor[2], prOuterColor[3] },
				prStartAngle, prEndAngle, prFadeAngle
			);
		}
	}

	//スプライトの座標を配列に格納
	Vector2 currentPos = sprite->GetPosition();
	float pos[2] = { currentPos.x,currentPos.y };

	//スライダーでスプライトの座標を変更できるようにする
	ImGui::SliderFloat2("Sprite Position", pos, 0.0f, 1280.0f, "%.1f");

	//スライダーで変更された座標をスプライトに反映
	sprite->SetPosition({ pos[0],pos[1] });

	ImGui::End();

	//ImGui::Begin("Settings");

	
	//if (objB!= nullptr) {
	//	// ライトのデータ（ポインタ）を取得
	//	DirectionalLight *lightData = objB->GetDirectionalLightData();

	//	if (lightData != nullptr) {
	//		// ① ライトの色（RGBA）
	//		ImGui::ColorEdit4("Light Color", &lightData->color.x);

	//		// ② ライトの向き（X, Y, Z）
	//		ImGui::DragFloat3("Light Direction", &lightData->direction.x, 0.01f, -1.0f, 1.0f);

	//		// ③ ライトの明るさ
	//		ImGui::DragFloat("Light Intensity", &lightData->intensity, 0.01f, 0.0f, 10.0f);
	//	}

	//	// ① 現在のスケールを取得する（関数名はご自身の環境に合わせてください）
	//	Vector3 currentScale = objB->GetScale();

	//	// ImGuiで使いやすいように配列に入れる
	//	float scale[3] = { currentScale.x, currentScale.y, currentScale.z };

	//	// ② スケールを変更できるスライダー（X, Y, Z を個別にイジれる）
	//	ImGui::DragFloat3("Scale", scale, 0.01f, 0.1f, 10.0f);

	//	// ③ 変更されたスケールをオブジェクトに反映する
	//	objB->SetScale({ scale[0], scale[1], scale[2] });
	//}

	//ImGui::End();

	ImGui::Begin("Camera Settings");

	// ① 現在のカメラの回転角と座標を取得
	Vector3 camRot = camera->GetRotate();
	Vector3 camPos = camera->GetTranslate();

	// ImGuiウィンドウ上にマウスがない場合、画面のドラッグでカメラを動かす
	if (!ImGui::GetIO().WantCaptureMouse) {
		// 左ドラッグでカメラ回転
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
			ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
			camRot.y += delta.x * 0.01f; // 左右ドラッグでY軸回転 (Yaw)
			camRot.x += delta.y * 0.01f; // 上下ドラッグでX軸回転 (Pitch)
			camera->SetRotate(camRot);
		}
		// 右ドラッグでカメラの平行移動
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
			ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
			ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
			camPos.x -= delta.x * 0.05f;
			camPos.y += delta.y * 0.05f;
			camera->SetTranslate(camPos);
		}
		// マウスホイールで前進・後退
		float wheel = ImGui::GetIO().MouseWheel;
		if (wheel != 0.0f) {
			camPos.z += wheel * 1.0f;
			camera->SetTranslate(camPos);
		}
	}

	// ② ImGuiで扱いやすいように配列に格納
	float camRotArr[3] = { camRot.x, camRot.y, camRot.z };
	float camPosArr[3] = { camPos.x, camPos.y, camPos.z };

	// ③ スライダー（ドラッグ操作）で値を変更できるようにする
	// ※ 0.01f はマウスでドラッグしたときの変化スピードです
	if (ImGui::DragFloat3("Rotation (Pitch, Yaw, Roll)", camRotArr, 0.01f)) {
		// 値が変更されたらカメラに反映
		camera->SetRotate({ camRotArr[0], camRotArr[1], camRotArr[2] });
	}

	// （おまけ）位置も変えたい場合
	if (ImGui::DragFloat3("Position (X, Y, Z)", camPosArr, 0.1f)) {
		camera->SetTranslate({ camPosArr[0], camPosArr[1], camPosArr[2] });
	}

	ImGui::End();
#endif
	sprite->Update();
}

void GamePlayScene::Draw() {
	//3Dオブジェトの描画準備
	Object3dCommon::GetInstance()->SetCommonDrawSettings();
	//3Dオブジェクトの描画
	for (Object3d *object3d : objects) {
		object3d->Draw();
	}
	
	// スカイボックスの描画
	skybox->Draw();
	
	// エフェクト系の描画 (深度書き込み無効)
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (myRing && showNormalRing) myRing->Draw();
	if (myPartialRing && showPartialRing) myPartialRing->Draw();
	if (myCylinder && showCylinder) myCylinder->Draw();

	//Spriteの描画基準
	SpriteCommon::GetInstance()->SetCommonPipelineState();
	//スプライト描画
	sprite->Draw();

	particleManager->Draw();
}
