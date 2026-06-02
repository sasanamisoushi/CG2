#include "GamePlayScene.h"
#include "3D/ModelManager.h"
#include <Windows.h>
#include "engine/Graphics/DirectXCommon.h"
#include "2D/SpriteCommon.h"
#include "3D/Object3dCommon.h"
#include "engine/Input/Input.h"
#include "engine/Debug/ImGuiManager.h"
#include <externals/imgui/imgui.h>
#include "engine/Camera/FlyCamera.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Utility/StageLoader.h"
#include "Game/editor/EditorReceiver.h"



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
	// objects.push_back(myShere.get());
	// ボーンとしても使われるため、目立つように赤くしておく
	if (myShere->GetModel()) {
		myShere->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
	}

	// ボックス
	myBox = std::make_unique<Primitive>();
	myBox->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Box);
	myBox->SetTranslate({ -2.0f,0.0f,0.0f });
	// objects.push_back(myBox.get()); // Boxの代わりにモデルを使う

	// 動的モデル
	myModelObject = std::make_unique<Object3d>();
	myModelObject->Initialize(Object3dCommon::GetInstance());
	ModelManager::GetInstance()->LoadModel("AnimatedCube/AnimatedCube.gltf");
	myModelObject->SetModel("AnimatedCube/AnimatedCube.gltf");
	//objects.push_back(myModelObject.get());

	// アニメーションとノード階層の読み込み
	animationData = LoadAnimationFile("resources/AnimatedCube", "AnimatedCube.gltf");
	Node rootNode = Model::LoadNodeHierarchy("resources/AnimatedCube", "AnimatedCube.gltf");
	skeleton = CreateSkeleton(rootNode);
	if (!skeleton.joints.empty()) {
		skeleton.joints[skeleton.root].transform.translate = { 0.0f, 0.0f, 0.0f };
	}

	myModelObject->skinCluster = myModelObject->GetModel()->CreateSkinCluster(skeleton);

	// ボーンライン用オブジェクトの初期化
	ModelManager::GetInstance()->CreateLineModel("SkeletonLines");
	skeletonLinesObject = std::make_unique<Object3d>();
	skeletonLinesObject->Initialize(Object3dCommon::GetInstance());
	skeletonLinesObject->SetModel("SkeletonLines");

	// デバッグ用コライダー表示ラインオブジェクトの初期化
	ModelManager::GetInstance()->CreateLineModel("DebugColliderLines");
	debugColliderLinesObject = std::make_unique<Object3d>();
	debugColliderLinesObject->Initialize(Object3dCommon::GetInstance());
	debugColliderLinesObject->SetModel("DebugColliderLines");

	// デバッグ用フリーカメラの初期化
	debugFlyCamera_ = std::make_unique<FlyCamera>();
	debugFlyCamera_->SetTranslate({ 0.0f, 5.0f, -20.0f }); // 初期位置
	isDebugCameraActive_ = false;


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
	particleManager->CreateParticleGroup("test", "resources/circle.png");
	particleEmitter = std::make_unique<ParticleEmitter>("test", Vector3{ 0.0f,0.0f,0.0f }, particleManager.get());

	//音声再生
	soundData1 = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");
	soundData2 = AudioManager::GetInstance()->LoadAudio("resources/maou_bgm_fantasy15.mp3");

	pVoice1=AudioManager::GetInstance()->PlayWave(soundData1, true);
	pVoice2=AudioManager::GetInstance()->PlayWave(soundData2, true);

	// 1. マネージャー経由でトレイル専用モデルを作る
	ModelManager::GetInstance()->CreateTrailModel("SmokeTrail");

	// 2. トレイル計算機の初期化（今回は60フレーム=約1秒分の長さを残す）
	missileTrail = std::make_unique<Trail>();
	missileTrail->Initialize(60);

	// 3. 描画用オブジェクトの初期化
	trailObject = std::make_unique<Object3d>();
	trailObject->Initialize(Object3dCommon::GetInstance());
	trailObject->SetModel("SmokeTrail");


	ModelManager::GetInstance()->CreateBoxModel("PlayerBox");
	ModelManager::GetInstance()->CreateBoxModel("EnemyBox");
	ModelManager::GetInstance()->CreateBoxModel("ObstacleBox");

	player_ = std::make_unique<Player>();
	player_->Initialize("PlayerBox");

	// 弾
	missileManager_ = std::make_unique<MissileManager>();
	missileManager_->Initialize();

	// 爆発エフェクト
	explosionManager_ = std::make_unique<ExplosionManager>();
	explosionManager_->Initialize(particleManager.get());

	//敵
	enemies_.clear();
	auto firstEnemy = std::make_unique<Enemy>();
	firstEnemy->Initialize({ 0.0f, 0.0f, 50.0f });
	enemies_.push_back(std::move(firstEnemy));

	enemyBulletManager_ = std::make_unique<EnemyBulletManager>();
	enemyBulletManager_->Initialize();

	// ゲームオーバー演出の初期化
	isGameOver_ = false;
	gameOverTimer_ = 0;

	// 敵と障害物のリストを一旦空にする
	enemies_.clear();
	obstacles_.clear();
	enemySpawnPoints_.clear();

	// Blenderから出力したJSONを読み込んで、敵と障害物を一発配置！！！
	StageLoader::LoadSceneJson("resources/scene.json", enemies_, obstacles_, player_.get(), &enemySpawnPoints_);
	SpawnEnemiesFromSpawnPoints();

	// 最初のファイル更新日時を記録しておく
	try {
		lastJsonWriteTime_ = std::filesystem::last_write_time("resources/scene.json");
	} catch (...) {
		// ※万が一ファイルが見つからない時のためのエラー回避
	}

	// エディターレシーバーの初期化
	EditorReceiver::GetInstance()->Initialize();
}

void GamePlayScene::SetDebugCameraActive(bool isActive) {
	if (isDebugCameraActive_ == isActive) {
		return;
	}

	isDebugCameraActive_ = isActive;
	if (isDebugCameraActive_) {
		debugFlyCamera_->SetTranslate(camera->GetTranslate());
		Object3dCommon::GetInstance()->SetDefaultCamera(debugFlyCamera_.get());
		OutputDebugStringA("[DebugCamera] ON: FlyCamera\n");
	} else {
		Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
		OutputDebugStringA("[DebugCamera] OFF: Player Camera\n");
	}
}

void GamePlayScene::SpawnEnemiesFromSpawnPoints() {
	for (const Vector3 &spawnPoint : enemySpawnPoints_) {
		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize(spawnPoint);
		enemies_.push_back(std::move(enemy));
	}
}

void GamePlayScene::Finalize() {
	if (pVoice1) {
		pVoice1->Stop();
		pVoice2->DestroyVoice();
	}

	AudioManager::GetInstance()->UnloadWave(soundData1);
	AudioManager::GetInstance()->UnloadWave(soundData2);

	// シーン切り替え時にポストエフェクトを通常に戻す
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	EditorReceiver::GetInstance()->Finalize();
}

void GamePlayScene::Update() {

	// Blenderからデータが来ていたら敵をリアルタイム更新！
	EditorReceiver::GetInstance()->Update(player_.get(), enemies_, obstacles_, enemySpawnPoints_);


	// =========================================================
	// ホットリロードの監視処理！
	// =========================================================
	try {
		// 今の "scene.json" の更新日時をチェックする
		auto currentTime = std::filesystem::last_write_time("resources/scene.json");

		// もし記憶している日時よりも新しければ（＝Blenderで上書き保存されたら！）
		if (currentTime > lastJsonWriteTime_) {
			lastJsonWriteTime_ = currentTime; // 新しい日時で上書き

			// 画面上の敵と障害物を一旦すべて消す
			enemies_.clear();
			obstacles_.clear();
			enemySpawnPoints_.clear();

			// 最新のJSONをもう一度読み込み直す！
			StageLoader::LoadSceneJson("resources/scene.json", enemies_, obstacles_, player_.get(), &enemySpawnPoints_);
			SpawnEnemiesFromSpawnPoints();

			// デバッグウィンドウにお知らせを出す
			OutputDebugStringA("Hot Reloaded: scene.json を再読み込みしました！\n");
		}
	} catch (...) {
		// 💡超重要：Blenderがファイルに書き込んでいる最中（数ミリ秒）は
		// C++からアクセスできずエラーになることがあるため、try-catchで握りつぶす
	}

	if (Input::GetInstance()->TriggerKey(DIK_0)) {
		OutputDebugStringA("HIt 0\n");
	}

	// Rキーでシーンを最初からやり直す
	if (Input::GetInstance()->TriggerKey(DIK_R)) {
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
		return;
	}

	// ==========================================
	// ゲームオーバー判定と演出進行
	// ==========================================
	if (!isGameOver_ && player_ && player_->IsDead()) {
		isGameOver_ = true;
		gameOverTimer_ = 0;

		// 💥 自機がやられた時の大爆発パーティクルを生成！
		std::vector<Vector3> playerHitPos = { player_->GetPosition() };
		if (explosionManager_) {
			explosionManager_->CreateExplosions(playerHitPos);
		}

		// 🎵 BGMを停止して絶望感を演出
		if (pVoice2) {
			pVoice2->Stop();
		}
	}

	bool shouldUpdateGame = true;

	if (isGameOver_) {
		gameOverTimer_++;

		// 絶望の白黒化（グレースケール）エフェクトを適用！
		if (PostEffect::GetInstance()) {
			float effectProgress = static_cast<float>(gameOverTimer_) / 120.0f;
			if (effectProgress > 1.0f) {
				effectProgress = 1.0f;
			}
			float vignetteRadius = 0.62f - 0.22f * effectProgress;
			float blurIntensity = 1.5f + 3.0f * effectProgress;
			PostEffect::GetInstance()->SetVignetteSmoothing(vignetteRadius, 0.38f, blurIntensity);
		}

		// 5フレームに1回だけ更新することで、スローモーション（世界停止）を実現！
		shouldUpdateGame = (gameOverTimer_ % 5 == 0);

		// 約2秒（120フレーム）経過したら、正式にゲームオーバーシーンへ遷移する！
		if (gameOverTimer_ >= 120) {
			SceneManager::GetInstance()->ChangeScene("GAMEOVER");
		}
	} else {
		// 通常時はノーマルエフェクト
		if (PostEffect::GetInstance()) {
			PostEffect::GetInstance()->SetEffectType(0); // 0: Normal
		}
	}

	if (myBox && animationData.duration > 0.0f) {
		if (playAnimation) {
			animationTime += 1.0f / 60.0f;
			animationTime = std::fmod(animationTime, animationData.duration);
		}
		
		// アニメーションの更新と骨への適用
		ApplyAnimation(skeleton, animationData, animationTime);
		::Update(skeleton);
		if (enableSkinning && myModelObject->GetModel()) {
			myModelObject->GetModel()->UpdateSkinCluster(myModelObject->skinCluster, skeleton);
		}

		// 今のやつに合わせた状態で使うため、Skeletonから計算結果を取り出してBox/Modelに適用する
		if (!skeleton.joints.empty()) {
			myBox->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
			myBox->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
			myBox->SetScale(skeleton.joints[skeleton.root].transform.scale);

			// スキニングが実装されたため、スキンなしモデルの場合のみTransformを適用する
			// スキニングが実装されたため、スキンなしモデルの場合のみTransformを適用する
			if (myModelObject->GetModel()) {
				if (!myModelObject->skinCluster.isValid) {
					myModelObject->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
					myModelObject->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
					myModelObject->SetScale(skeleton.joints[skeleton.root].transform.scale);
				} else {
					// スキニングモデルはアニメーションが行列に含まれるため、ベースのトランスフォームはリセットする
					// (これを行わないと二重に移動して画面外に消える)
					myModelObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
					myModelObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
					myModelObject->SetScale({ modelScale, modelScale, modelScale });
				}
			}
		}

		// 骨描画の更新
		if (showBones) {
			std::vector<VertexData> lineVertices;

			for (size_t i = 0; i < skeleton.joints.size(); ++i) {
				Vector3 pos = {
					skeleton.joints[i].skeletonSpaceMatrix.m[3][0],
					skeleton.joints[i].skeletonSpaceMatrix.m[3][1],
					skeleton.joints[i].skeletonSpaceMatrix.m[3][2]
				};
				// モデル全体のスケールに合わせてボーンの座標もスケーリングする
				pos.x *= modelScale;
				pos.y *= modelScale;
				pos.z *= modelScale;

				// ライン用の頂点を作成（親がいる場合）
				if (skeleton.joints[i].parent) {
					int32_t parentIndex = *skeleton.joints[i].parent;
					Vector3 parentPos = {
						skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][0],
						skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][1],
						skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][2]
					};
					parentPos.x *= modelScale;
					parentPos.y *= modelScale;
					parentPos.z *= modelScale;

					VertexData v1, v2;
					v1.position = { parentPos.x, parentPos.y, parentPos.z, 1.0f };
					v1.normal = { 0.0f, 1.0f, 0.0f };
					v1.texcoord = { 0.0f, 0.0f };
					v1.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色

					v2.position = { pos.x, pos.y, pos.z, 1.0f };
					v2.normal = { 0.0f, 1.0f, 0.0f };
					v2.texcoord = { 1.0f, 1.0f };
					v2.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色

					lineVertices.push_back(v1);
					lineVertices.push_back(v2);
				}
			}

			// ラインモデルの頂点を更新
			if (!lineVertices.empty() && skeletonLinesObject->GetModel()) {
				skeletonLinesObject->GetModel()->UpdateLineVertices(lineVertices);
			}
			skeletonLinesObject->Update();
		}
	}

	// モデルの更新
	if (showModel && myModelObject) {
		myModelObject->Update();
	}

	// =====================================================
	// F1 キー: デバッグフリーカメラ ON/OFF
	// =====================================================
	// Debug camera switching is handled by the ImGui button in UpdateUI().
	if (false && Input::GetInstance()->TriggerKey(DIK_F1)) {
		isDebugCameraActive_ = !isDebugCameraActive_;
		if (isDebugCameraActive_) {
			// 自機追従カメラの現在位置をフリーカメラの初期座標として引き継ぐ
			debugFlyCamera_->SetTranslate(camera->GetTranslate());
			Object3dCommon::GetInstance()->SetDefaultCamera(debugFlyCamera_.get());
			OutputDebugStringA("[DebugCamera] ON: FlyCamera\n");
		} else {
			Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
			OutputDebugStringA("[DebugCamera] OFF: Player Camera\n");
		}
	}

	// プレイヤーの更新と、カメラの追従
	if (player_) {
		if (shouldUpdateGame) {
			player_->Update(obstacles_);
		}

		// デバッグカメラが OFFのときのみ、自機追従カメラを適用
		if (!isDebugCameraActive_) {
			player_->UpdateCamera(camera.get());
		}
	}

	// ==========================================
	// 敵
	// ==========================================
	// プレイヤーの最新座標を取得する
	Vector3 playerPos = player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };

	if (shouldUpdateGame) {
		// 敵の弾の更新（被弾時の爆発座標を受け取る）
		std::vector<Vector3> enemyBulletHits;
		if (enemyBulletManager_ && player_) {
			enemyBulletManager_->Update(player_.get(), enemyBulletHits, obstacles_);
		}

		// 敵の弾がプレイヤーに当たった場合も爆発を発生させる
		if (explosionManager_ && !enemyBulletHits.empty()) {
			explosionManager_->CreateExplosions(enemyBulletHits);
		}

		for (auto it = enemies_.begin(); it != enemies_.end(); ) {
			(*it)->Update(playerPos, enemyBulletManager_.get(), obstacles_);
			if ((*it)->IsDead()) {
				it = enemies_.erase(it); // 当たった敵はリストから消滅
			} else {
				++it;
			}
		}

		// 障害物自身のUpdateを回す（現状中身は空に近いですが一応回します）
		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	}

	// カメラの更新
	if (isDebugCameraActive_) {
		debugFlyCamera_->Update(); // FlyCameraが自分で入力を消化して自分を更新する
		skybox->Update(debugFlyCamera_.get());
	} else {
		camera->Update();
		skybox->Update(camera.get());
	}

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
			Vector3 uvScale = { 1.0f, 10.0f, 1.0f };
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

	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();

	if (showParticles) {
		particleEmitter->Update();
	}


	// ==========================================
	// ミサイルの発射処理
	// ==========================================
	if (player_ && !isGameOver_) {
		Vector3 playerPos = player_->GetPosition();
		Vector3 forward = player_->GetForwardVector();

		// 左クリック：速くて煙が出ない通常弾
		if (Input::GetInstance()->TriggerMouseButton(0)) {
			Vector3 vel = { forward.x * 2.0f, forward.y * 2.0f, forward.z * 2.0f };
			missileManager_->Shoot(playerPos, vel, MissileType::Normal);
		}

		// 右クリック：少し遅くて煙を引くミサイル
		if (Input::GetInstance()->TriggerMouseButton(1)) {
			Vector3 vel = { forward.x * 1.0f, forward.y * 1.0f, forward.z * 1.0f };
			missileManager_->Shoot(playerPos, vel, MissileType::MissileWithTrail);
		}
	}

	// ==========================================
	// 弾の更新処理
	// ==========================================
	std::vector<Vector3> hitPositions;
	if (shouldUpdateGame) {
		if (missileManager_) {
			missileManager_->Update(activeCamera, enemies_, obstacles_, hitPositions);
		}

		// 爆発マネージャーに座標リストを渡して、発生を依頼するだけ！
		if (explosionManager_ && !hitPositions.empty()) {
			explosionManager_->CreateExplosions(hitPositions);
		}

		// 爆発マネージャーの更新
		if (explosionManager_) {
			explosionManager_->Update();
		}
	}

	// 大元のパーティクル全体の更新
	particleManager->Update(activeCamera);

	// ==========================================
	// デバッグ用コライダー頂点構築
	// ==========================================
	if (showDebugColliders && debugColliderLinesObject && debugColliderLinesObject->GetModel()) {
		std::vector<VertexData> colliderVertices;

		// AABB構築ヘルパー
		auto addAABB = [&](const Vector3& center, const Vector3& extents, const Vector4& color) {
			Vector3 p[8] = {
				{ center.x - extents.x, center.y - extents.y, center.z - extents.z },
				{ center.x + extents.x, center.y - extents.y, center.z - extents.z },
				{ center.x + extents.x, center.y + extents.y, center.z - extents.z },
				{ center.x - extents.x, center.y + extents.y, center.z - extents.z },
				{ center.x - extents.x, center.y - extents.y, center.z + extents.z },
				{ center.x + extents.x, center.y - extents.y, center.z + extents.z },
				{ center.x + extents.x, center.y + extents.y, center.z + extents.z },
				{ center.x - extents.x, center.y + extents.y, center.z + extents.z }
			};

			auto addLine = [&](int i1, int i2) {
				VertexData v1, v2;
				v1.position = { p[i1].x, p[i1].y, p[i1].z, 1.0f };
				v1.color = color;
				v2.position = { p[i2].x, p[i2].y, p[i2].z, 1.0f };
				v2.color = color;
				colliderVertices.push_back(v1);
				colliderVertices.push_back(v2);
			};

			// 底面
			addLine(0, 1); addLine(1, 2); addLine(2, 3); addLine(3, 0);
			// 天面
			addLine(4, 5); addLine(5, 6); addLine(6, 7); addLine(7, 4);
			// 側面
			addLine(0, 4); addLine(1, 5); addLine(2, 6); addLine(3, 7);
		};

		auto addOBB = [&](const Vector3& center, const Vector3& extents, const Vector3& rotation, const Vector4& color) {
			Matrix4x4 rotMat = MyMath::Multiply(MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation.x), MyMath::MakeRotateYMatrix(rotation.y)), MyMath::MakeRotateZMatrix(rotation.z));

			Vector3 local[8] = {
				{ -extents.x, -extents.y, -extents.z },
				{  extents.x, -extents.y, -extents.z },
				{  extents.x,  extents.y, -extents.z },
				{ -extents.x,  extents.y, -extents.z },
				{ -extents.x, -extents.y,  extents.z },
				{  extents.x, -extents.y,  extents.z },
				{  extents.x,  extents.y,  extents.z },
				{ -extents.x,  extents.y,  extents.z }
			};

			Vector3 p[8];
			for (int i = 0; i < 8; ++i) {
				Vector3 rotated = MyMath::Transform(local[i], rotMat);
				p[i] = { center.x + rotated.x, center.y + rotated.y, center.z + rotated.z };
			}

			auto addLine = [&](int i1, int i2) {
				VertexData v1, v2;
				v1.position = { p[i1].x, p[i1].y, p[i1].z, 1.0f };
				v1.color = color;
				v2.position = { p[i2].x, p[i2].y, p[i2].z, 1.0f };
				v2.color = color;
				colliderVertices.push_back(v1);
				colliderVertices.push_back(v2);
			};

			addLine(0, 1); addLine(1, 2); addLine(2, 3); addLine(3, 0);
			addLine(4, 5); addLine(5, 6); addLine(6, 7); addLine(7, 4);
			addLine(0, 4); addLine(1, 5); addLine(2, 6); addLine(3, 7);
		};

		// 球体構築ヘルパー
		auto addSphere = [&](const Vector3& center, float radius, const Vector4& color) {
			const int subdiv = 24;
			for (int i = 0; i < subdiv; ++i) {
				float theta1 = 2.0f * 3.14159265f * (float)i / (float)subdiv;
				float theta2 = 2.0f * 3.14159265f * (float)(i + 1) / (float)subdiv;

				float cos1 = std::cos(theta1);
				float sin1 = std::sin(theta1);
				float cos2 = std::cos(theta2);
				float sin2 = std::sin(theta2);

				// X-Y 平面
				VertexData v1, v2;
				v1.color = color;
				v2.color = color;

				v1.position = { center.x + radius * cos1, center.y + radius * sin1, center.z, 1.0f };
				v2.position = { center.x + radius * cos2, center.y + radius * sin2, center.z, 1.0f };
				colliderVertices.push_back(v1);
				colliderVertices.push_back(v2);

				// Y-Z 平面
				v1.position = { center.x, center.y + radius * cos1, center.z + radius * sin1, 1.0f };
				v2.position = { center.x, center.y + radius * cos2, center.z + radius * sin2, 1.0f };
				colliderVertices.push_back(v1);
				colliderVertices.push_back(v2);

				// Z-X 平面
				v1.position = { center.x + radius * sin1, center.y, center.z + radius * cos1, 1.0f };
				v2.position = { center.x + radius * sin2, center.y, center.z + radius * cos2, 1.0f };
				colliderVertices.push_back(v1);
				colliderVertices.push_back(v2);
			}
		};

		// 1. プレイヤーのAABBと球
		if (player_ && !player_->IsDead()) {
			// AABB: Green, Size: 0.4x0.4x0.4 (extents: 0.2)
			addAABB(player_->GetPosition(), { 0.2f, 0.2f, 0.2f }, { 0.0f, 1.0f, 0.0f, 1.0f });
			// Sphere (対敵の弾用当たり判定): Cyan/Green (radius: 1.0f)
			addSphere(player_->GetPosition(), 1.0f, { 0.0f, 1.0f, 0.5f, 1.0f });
		}

		// 2. 障害物のAABB
		for (const auto& obstacle : obstacles_) {
			// モデルの実際のバウンディングボックス × Blenderスケール = 正確なワールドAABB
			Vector3 extents = obstacle->GetWorldHalfExtents();
			addOBB(obstacle->GetPosition(), extents, obstacle->GetRotation(), { 0.0f, 1.0f, 1.0f, 1.0f });
		}

		// 3. 敵のAABBと球
		for (const auto& enemy : enemies_) {
			if (!enemy->IsDead()) {
				// AABB: Red, scaled from Blender object.
				addAABB(enemy->GetPosition(), enemy->GetWorldHalfExtents(), { 1.0f, 0.0f, 0.0f, 1.0f });
				// Sphere (対プレイヤーミサイル用当たり判定): Red/Orange (radius: 1.0f)
				addSphere(enemy->GetPosition(), 1.0f, { 1.0f, 0.3f, 0.0f, 1.0f });
			}
		}

		// 4. 自機ミサイル（Player Bullets）
		if (missileManager_) {
			for (const auto& missile : missileManager_->GetMissiles()) {
				if (!missile->IsDead()) {
					// Sphere: Magenta (radius: 0.5f)
					addSphere(missile->GetPosition(), 0.5f, { 1.0f, 0.0f, 1.0f, 1.0f });
				}
			}
		}

		// 5. 敵の弾（Enemy Bullets）
		if (enemyBulletManager_) {
			for (const auto& bullet : enemyBulletManager_->GetBullets()) {
				if (!bullet.isDead) {
					// Sphere: Orange (radius: 0.5f)
					addSphere(bullet.position, 0.5f, { 1.0f, 0.5f, 0.0f, 1.0f });
				}
			}
		}

		// 空の場合はダミーの透明な線を追加（リソース stuck 防止）
		if (colliderVertices.empty()) {
			VertexData v1, v2;
			v1.position = { 0.0f, 0.0f, 0.0f, 1.0f };
			v1.color = { 0.0f, 0.0f, 0.0f, 0.0f };
			v2.position = { 0.0f, 0.0f, 0.0f, 1.0f };
			v2.color = { 0.0f, 0.0f, 0.0f, 0.0f };
			colliderVertices.push_back(v1);
			colliderVertices.push_back(v2);
		}

		debugColliderLinesObject->GetModel()->UpdateLineVertices(colliderVertices);
		debugColliderLinesObject->Update();
	}

#ifdef ENABLE_IMGUI
	UpdateUI();
#endif
	sprite->Update();
}

void GamePlayScene::Draw() {
	//3Dオブジェトの描画準備
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	// プレイヤーの描画
	if (player_) {
		player_->Draw();
	}

	// すべてのミサイルを描画
	if (missileManager_) {
		missileManager_->Draw();
	}

	// 敵の弾を描画
	if (enemyBulletManager_) {
		enemyBulletManager_->Draw();
	}

	// 敵の描画
	for (const auto &enemy : enemies_) {
		enemy->Draw();
	}

	// 障害物の描画
	for (const auto &obstacle : obstacles_) {
		obstacle->Draw();
	}

	//3Dオブジェクトの描画
	if (showPlane) {
		for (Object3d* object3d : objects) {
			object3d->Draw();
		}
	}

	// アニメーションモデルの個別描画制御
	if (showModel && myModelObject) {
		myModelObject->Draw();
	}
	
	if (showBones) {
		// ボーン描画の前に設定を確実にする
		Object3dCommon::GetInstance()->SetCommonDrawSettings();

		// ボーンラインの描画
		if (skeletonLinesObject && skeletonLinesObject->GetModel()) {
			skeletonLinesObject->Draw();
		}
	}

	// デバッグコライダーの描画
	if (showDebugColliders && debugColliderLinesObject && debugColliderLinesObject->GetModel()) {
		// 描画の前に設定を確実にする
		Object3dCommon::GetInstance()->SetCommonDrawSettings();
		debugColliderLinesObject->Draw();
	}
	
	// スカイボックスの描画
	if (showSkybox) {
		skybox->Draw();
	}
	
	// エフェクト系の描画 (深度書き込み無効)
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (myRing && showNormalRing) myRing->Draw();
	if (myPartialRing && showPartialRing) myPartialRing->Draw();
	if (myCylinder && showCylinder) myCylinder->Draw();
	if (explosionManager_) explosionManager_->Draw();


	//Spriteの描画基準
	SpriteCommon::GetInstance()->SetCommonPipelineState();
	//スプライト描画
	if (showSprite) {
		sprite->Draw();
	}

	particleManager->Draw();
}

void GamePlayScene::UpdateUI() {
#ifdef ENABLE_IMGUI
	if (ImGuiManager::IsVisible()) {
		//開発用UIの処理
		ImGui::ShowDemoWindow();

		//ウィンドウのサイズを設定
		ImGui::SetNextWindowSize(ImVec2(500.0f, 400.0f), ImGuiCond_Once);

		//ウィンドウの作成
		ImGui::Begin("演習");

		ImGui::Text("表示設定");
		ImGui::Checkbox("スカイボックスを表示", &showSkybox);
		ImGui::Checkbox("平面を表示", &showPlane);
		ImGui::Checkbox("球体（ミサイル頭）を表示", &showSphere);
		ImGui::Checkbox("通常リングを表示", &showNormalRing);
		ImGui::Checkbox("部分リングを表示", &showPartialRing);
		ImGui::Checkbox("シリンダーを表示", &showCylinder);
		ImGui::Checkbox("トレイルを表示", &showTrail);
		ImGui::Checkbox("モデルを表示", &showModel);
		ImGui::Checkbox("パーティクルを表示", &showParticles);
		ImGui::Checkbox("ボーンを表示", &showBones);
		ImGui::Checkbox("スプライトを表示", &showSprite);
		ImGui::Checkbox("Show Debug Colliders", &showDebugColliders);

		ImGui::Separator();
		ImGui::Text("デバック　カメラ");
		if (ImGui::Button(isDebugCameraActive_ ? "Switch to Player Camera" : "Switch to Debug Camera")) {
			SetDebugCameraActive(!isDebugCameraActive_);
		}
		ImGui::SameLine();
		ImGui::Text("%s", isDebugCameraActive_ ? "Active: Debug Camera" : "Active: Player Camera");

		ImGui::Separator();
		ImGui::Text("GPUパーティクルの操作");
		bool gpuChanged = false;
		if (auto *emitter = particleManager->GetEmitterSphere()) {
			if (ImGui::DragFloat3("位置", &emitter->translate.x, 0.01f)) gpuChanged = true;
			if (ImGui::DragFloat("射出半径", &emitter->radius, 0.01f)) gpuChanged = true;
			if (ImGui::DragInt("射出数", (int *) &emitter->count, 1, 0, 1000)) gpuChanged = true;
			if (ImGui::DragFloat("射出間隔", &emitter->frequency, 0.01f, 0.01f, 10.0f)) gpuChanged = true;
		}

		if (ImGui::Button("GPUパーティクルを再初期化") || gpuChanged) {
			particleManager->RequestGpuInitialize();
		}

		ImGui::Separator();
		ImGui::Text("アニメーション制御");
		const char *animationNames[] = { "AnimatedCube", "simpleSkin", "sneakWalk", "walk" };
		if (ImGui::Combo("アニメーション", &currentAnimationIndex, animationNames, IM_ARRAYSIZE(animationNames))) {
			std::string dir, file, loadFile;
			if (currentAnimationIndex == 0) { dir = "resources/AnimatedCube"; file = "AnimatedCube.gltf"; loadFile = "AnimatedCube/AnimatedCube.gltf"; } else if (currentAnimationIndex == 1) { dir = "resources/simpleSkin"; file = "simpleSkin.gltf"; loadFile = "simpleSkin/simpleSkin.gltf"; } else if (currentAnimationIndex == 2) { dir = "resources/human"; file = "sneakWalk.gltf"; loadFile = "human/sneakWalk.gltf"; } else if (currentAnimationIndex == 3) { dir = "resources/human"; file = "walk.gltf"; loadFile = "human/walk.gltf"; }

			animationData = LoadAnimationFile(dir, file);
			Node rootNode = Model::LoadNodeHierarchy(dir, file);
			skeleton = CreateSkeleton(rootNode);
			if (!skeleton.joints.empty()) {
				skeleton.joints[skeleton.root].transform.translate = { 0.0f, 0.0f, 0.0f };
			}
			animationTime = 0.0f;

			ModelManager::GetInstance()->LoadModel(loadFile);
			myModelObject->SetModel(loadFile);
			if (myModelObject->GetModel()) {
				myModelObject->skinCluster = myModelObject->GetModel()->CreateSkinCluster(skeleton);
			}

			// モデルに応じた適切なスケールを自動設定する
			if (currentAnimationIndex == 0) {
				modelScale = 0.01f; // AnimatedCubeは巨大なので縮小
			} else {
				modelScale = 1.0f;  // simpleSkinなどは等倍で人間サイズ
			}
		}
		ImGui::Checkbox("スキニング (ガワを動かす)", &enableSkinning);
		ImGui::SliderFloat("モデルスケール", &modelScale, 0.001f, 1.0f);
		if (ImGui::Checkbox("アニメーション再生", &playAnimation)) {}
		ImGui::SliderFloat("再生時間", &animationTime, 0.0f, animationData.duration);

		ImGui::Separator();
		ImGui::Text("シリンダー設定");
		ImGui::DragFloat3("シリンダー座標", cylinderPos, 0.01f);
		ImGui::DragFloat3("シリンダースケール", cylinderScale, 0.01f);
		ImGui::DragFloat2("UVスクロール速度", cylinderUVScrollSpeed, 0.001f);
		ImGui::SliderFloat("アルファリファレンス", &cylinderAlphaReference, 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::Text("ミサイル軌跡設定"); // ミサイル軌道設定

		// スライダーで各パラメータを調整できるようにする
		ImGui::SliderFloat("スピード", &missileSpeed, 0.01f, 0.2f);
		ImGui::SliderFloat("X軸の旋回半径", &missileAmpX, 0.0f, 50.0f);
		ImGui::SliderFloat("Z軸の旋回半径", &missileAmpZ, 0.0f, 50.0f);
		ImGui::SliderFloat("上下の波打ち幅", &missileAmpY, 0.0f, 20.0f);
		ImGui::SliderFloat("波打ちの細かさ", &missileFreqY, 0.1f, 20.0f);
		ImGui::SliderFloat("基準高度", &missileBaseY, 0.0f, 20.0f);

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
			Model *model = myCylinder->GetModel();
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
			Model *model = myPartialRing->GetModel();
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

		ImGui::Begin("Camera Settings");

		// =====================================================
		// デバッグカメラ 切り替え
		// =====================================================
		if (isDebugCameraActive_) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "[FREE CAM ACTIVE]");
			ImGui::Text("WASD: 移動  /  矢印キー: 回転  /  Q,E: ロール");
			ImGui::Text("F1 を押すと自機追従カメラに戻ります");

			if (ImGui::Button("自機追従カメラに戻る (F1)")) {
				isDebugCameraActive_ = false;
				Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
			}

			ImGui::Separator();
			ImGui::Text("操作方法:");
			ImGui::BulletText("右クリック + ドラッグ : 視点回転");
			ImGui::BulletText("スクロールホイール   : 前後移動");
			ImGui::BulletText("中クリック + ドラッグ : 上下左右パン");
			ImGui::BulletText("WASD                  : 前後左右移動");
			ImGui::BulletText("矢印キー              : 視点回転(キーボード)");
			ImGui::BulletText("Q / E                 : ロール");

			ImGui::Separator();
			// フリーカメラの速度調整（カメラから現在値を読む）
			float moveSpd = debugFlyCamera_->GetMoveSpeed();
			float rotSpd  = debugFlyCamera_->GetRotateSpeed();
			float sens    = debugFlyCamera_->GetMouseSensitivity();
			float scroll  = debugFlyCamera_->GetScrollSpeed();
			float pan     = debugFlyCamera_->GetPanSpeed();
			if (ImGui::DragFloat("移動速度 (WASD)##fly",      &moveSpd, 0.01f, 0.01f, 20.0f)) {
				debugFlyCamera_->SetMoveSpeed(moveSpd);
			}
			if (ImGui::DragFloat("回転感度 (マウス右)##fly",  &sens,    0.0001f, 0.0001f, 0.05f, "%.4f")) {
				debugFlyCamera_->SetMouseSensitivity(sens);
			}
			if (ImGui::DragFloat("スクロール速度##fly",       &scroll,  0.1f, 0.1f, 20.0f)) {
				debugFlyCamera_->SetScrollSpeed(scroll);
			}
			if (ImGui::DragFloat("パン速度 (中ボタン)##fly",  &pan,     0.001f, 0.001f, 1.0f)) {
				debugFlyCamera_->SetPanSpeed(pan);
			}
			if (ImGui::DragFloat("回転速度 (キーボード)##fly",&rotSpd,  0.001f, 0.001f, 0.5f)) {
				debugFlyCamera_->SetRotateSpeed(rotSpd);
			}

			ImGui::Separator();
			// フリーカメラの現在位置表示
			Vector3 flyPos = debugFlyCamera_->GetTranslate();
			float flyPosArr[3] = { flyPos.x, flyPos.y, flyPos.z };
			if (ImGui::DragFloat3("カメラ位置##fly", flyPosArr, 0.1f)) {
				debugFlyCamera_->SetTranslate({ flyPosArr[0], flyPosArr[1], flyPosArr[2] });
			}

		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[PLAYER FOLLOW CAM]");
			ImGui::Text("F1 を押すとフリーカメラに切り替わります");

			if (ImGui::Button("フリーカメラに切り替え (F1)")) {
				isDebugCameraActive_ = true;
				debugFlyCamera_->SetTranslate(camera->GetTranslate());
				Object3dCommon::GetInstance()->SetDefaultCamera(debugFlyCamera_.get());
			}

			ImGui::Separator();
			// 自機追従カメラの現在位置表示（参考用）
			Vector3 camPos = camera->GetTranslate();
			float camPosArr[3] = { camPos.x, camPos.y, camPos.z };
			if (ImGui::DragFloat3("カメラ位置 (参考)##follow", camPosArr, 0.1f)) {
				camera->SetTranslate({ camPosArr[0], camPosArr[1], camPosArr[2] });
			}
		}

		ImGui::End();

		ImGui::Separator();

		ImGui::Begin("敵 & 障害物");
		ImGui::Text("=== ターゲット配置 ===");
		ImGui::DragFloat3("出現座標 (X,Y,Z)", newEnemyPos, 1.0f);

		// ボタンを押した瞬間に、新しい敵をリストに追加！
		if (ImGui::Button("敵を生成する！")) {
			auto newEnemy = std::make_unique<Enemy>();
			newEnemy->Initialize({ newEnemyPos[0], newEnemyPos[1], newEnemyPos[2] });
			enemies_.push_back(std::move(newEnemy));
		}

		ImGui::Separator();
		ImGui::Text("=== 敵のリスト (総数: %d) ===", (int)enemies_.size());
		int index = 0;
		for (const auto& enemy : enemies_) {
			Vector3 pos = enemy->GetPosition();
			ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", index, pos.x, pos.y, pos.z);
			index++;
		}
		if (enemies_.empty()) {
			ImGui::Text("現在、敵は存在しません。");
		}

		ImGui::Separator();
		ImGui::Text("=== 障害物のリスト (総数: %d) ===", (int)obstacles_.size());
		int obsIndex = 0;
		for (const auto& obstacle : obstacles_) {
			Vector3 pos = obstacle->GetPosition();
			ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", obsIndex, pos.x, pos.y, pos.z);
			obsIndex++;
		}
		if (obstacles_.empty()) {
			ImGui::Text("現在、障害物は存在しません。");
		}
		ImGui::End();

		ImGui::Begin("敵撃破パーティクル設定");
		if (explosionManager_) {
			auto& config = explosionManager_->GetConfig();
			ImGui::DragInt("発生数", &config.count, 1, 0, 1000);
			ImGui::ColorEdit4("カラー", config.color);
			ImGui::DragFloat("速度", &config.speed, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("速度ばらつき", &config.speedVariance, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("スケール", &config.scale, 0.001f, 0.0f, 5.0f);
			ImGui::DragFloat("スケールばらつき", &config.scaleVariance, 0.001f, 0.0f, 2.0f);
			ImGui::DragFloat("最小寿命", &config.lifeTimeMin, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("最大寿命", &config.lifeTimeMax, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("位置ばらつき", &config.posVariance, 0.01f, 0.0f, 5.0f);

			ImGui::Separator();
			if (ImGui::Button("設定をJSONに保存")) {
				explosionManager_->SaveToJson("resources/explosionConfig.json");
			}
			ImGui::SameLine();
			if (ImGui::Button("設定をJSONから読込")) {
				explosionManager_->LoadFromJson("resources/explosionConfig.json");
			}
		}
		ImGui::End();

		ImGui::Begin("Level Editor Tools"); // 新しいウィンドウを作る場合

		// もしボタンが押されたら { } の中が実行される
		if (ImGui::Button("Open Blender")) {
			// ここでBlenderを起動！
			ShellExecuteA(nullptr, "open", "resources\\stage.blend", nullptr, nullptr, SW_SHOW);
		}

		ImGui::End();
	}
#endif
}
