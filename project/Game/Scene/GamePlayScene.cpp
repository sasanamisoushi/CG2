#include "GamePlayScene.h"
#include "SimulationManager.h"
#include "MissilePresetManager.h"
#include "LockOnManager.h"
#include "GamePlaySceneHelpers.h"
#include "3D/ModelManager.h"
#include <Windows.h>
#include "engine/Graphics/DirectXCommon.h"
#include "2D/SpriteCommon.h"
#include "3D/Object3dCommon.h"
#include "engine/Input/Input.h"
#include "engine/Debug/ImGuiManager.h"
#include "engine/Resource/TextureManager.h"
#include <externals/imgui/imgui.h>
#include "engine/Camera/FlyCamera.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Utility/StageLoader.h"
#include "engine/Utility/StageValidation.h"
#include "externals/json.hpp"
#include "Game/editor/EditorReceiver.h"
#include "Game/enemy/Boss.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <shellapi.h>

using json = nlohmann::json;

namespace {
	constexpr int kNoEnemyRespawnTimer = -1;
	constexpr float kSpecialAttackCost = 50.0f;
	constexpr float kSpGaugeRecoveryPerFrame = 3.0f / 60.0f;
	constexpr int kSpecialAttackDurationFrames = 180;
	constexpr int kSpecialAttackFireIntervalFrames = 6;
	constexpr int kNormalMagazineCapacity = 30;
	constexpr int kNormalReserveCapacity = 120;
	constexpr int kHomingMagazineCapacity = 8;
	constexpr int kHomingReserveCapacity = 24;
	constexpr int kReloadDurationFrames = 120;
	constexpr int kKillsPerAmmoPickup = 5;
	constexpr int kPickupNormalAmmo = 60;
	constexpr int kPickupHomingAmmo = 12;
}




GamePlayScene::GamePlayScene(Mode mode)
	: mode_(mode) {
}

bool GamePlayScene::TryConsumeAmmo(MissileType type) {
	if ((type == MissileType::Normal && isNormalReloading_) ||
		(type == MissileType::MissileWithTrail && isHomingReloading_)) {
		return false;
	}
	int &magazine = (type == MissileType::Normal) ? normalAmmoInMagazine_ : homingAmmoInMagazine_;
	if (magazine <= 0) {
		return false;
	}
	--magazine;
	return true;
}

void GamePlayScene::UpdateReload() {
	if (!player_ || player_->IsDead()) return;
	Input *input = Input::GetInstance();
	if (!isNormalReloading_ && input->TriggerKey(DIK_F) &&
		normalAmmoInMagazine_ < kNormalMagazineCapacity && normalAmmoReserve_ > 0) {
		isNormalReloading_ = true;
		normalReloadFrame_ = 0;
	}
	if (!isHomingReloading_ && input->TriggerKey(DIK_G) &&
		homingAmmoInMagazine_ < kHomingMagazineCapacity && homingAmmoReserve_ > 0) {
		isHomingReloading_ = true;
		homingReloadFrame_ = 0;
	}

	auto finishReload = [](int &magazine, int capacity, int &reserve) {
		const int required = capacity - magazine;
		const int loaded = (std::min)(required, reserve);
		magazine += loaded;
		reserve -= loaded;
	};
	if (isNormalReloading_ && ++normalReloadFrame_ >= kReloadDurationFrames) {
		finishReload(normalAmmoInMagazine_, kNormalMagazineCapacity, normalAmmoReserve_);
		isNormalReloading_ = false;
		normalReloadFrame_ = 0;
	}
	if (isHomingReloading_ && ++homingReloadFrame_ >= kReloadDurationFrames) {
		finishReload(homingAmmoInMagazine_, kHomingMagazineCapacity, homingAmmoReserve_);
		isHomingReloading_ = false;
		homingReloadFrame_ = 0;
	}
}

void GamePlayScene::SpawnAmmoPickup(const Vector3 &position) {
	AmmoPickup pickup;
	pickup.basePosition = position;
	pickup.basePosition.y += 2.0f;
	pickup.phase = static_cast<float>(ammoPickups_.size()) * 0.8f;
	pickup.object = std::make_unique<Object3d>();
	pickup.object->Initialize(Object3dCommon::GetInstance());
	pickup.object->SetModel("AmmoPickupSphere");
	pickup.object->SetScale({ 1.2f, 1.2f, 1.2f });
	pickup.object->SetTranslate(pickup.basePosition);
	pickup.object->Update();
	ammoPickups_.push_back(std::move(pickup));
}

void GamePlayScene::UpdateAmmoPickups() {
	if (!player_) return;
	const Vector3 playerPosition = player_->GetPosition();
	for (auto it = ammoPickups_.begin(); it != ammoPickups_.end();) {
		it->phase += 0.05f;
		Vector3 displayPosition = it->basePosition;
		displayPosition.y += std::sin(it->phase) * 0.5f;
		it->object->SetTranslate(displayPosition);
		it->object->SetRotate({ 0.0f, it->phase, 0.0f });
		it->object->Update();

		const float dx = displayPosition.x - playerPosition.x;
		const float dy = displayPosition.y - playerPosition.y;
		const float dz = displayPosition.z - playerPosition.z;
		if (dx * dx + dy * dy + dz * dz <= 9.0f) {
			normalAmmoReserve_ = (std::min)(kNormalReserveCapacity, normalAmmoReserve_ + kPickupNormalAmmo);
			homingAmmoReserve_ = (std::min)(kHomingReserveCapacity, homingAmmoReserve_ + kPickupHomingAmmo);
			it = ammoPickups_.erase(it);
		} else {
			++it;
		}
	}
}


void GamePlayScene::Initialize() {

	//鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・｡鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｻ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ
	camera = std::make_unique<Camera>();
	uiManager_ = std::make_unique<GamePlayUIManager>(this);
	environmentRenderer_ = std::make_unique<EnvironmentRenderer>();
	environmentRenderer_->Initialize();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());

	//鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢隴惹ｸ橸ｽｹ・ｲ繝ｻ荳ｻ・ｸ・ｷ繝ｻ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴寂握縺狗ｹ晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蠑ｱ繝ｻ繝ｻ繝ｻ蛻ｹ繝ｻ・ｹ驛｢譎｢・ｽ・ｻ
	sprite = std::make_unique<Sprite>();
	sprite->Initialize(SpriteCommon::GetInstance() , "resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture(kLockOnReticleTexturePath);
	TextureManager::GetInstance()->LoadTexture(kMissileLockOnReticleTexturePath);
	TextureManager::GetInstance()->LoadTexture(kAimCursorTexturePath);
	TextureManager::GetInstance()->LoadTexture(kBoundaryAlertTexturePath);

	aimCursorSprite_ = std::make_unique<Sprite>();
	aimCursorSprite_->Initialize(SpriteCommon::GetInstance(), kAimCursorTexturePath);
	aimCursorSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	lockOnReticleSprite_ = std::make_unique<Sprite>();
	lockOnReticleSprite_->Initialize(SpriteCommon::GetInstance(), kLockOnReticleTexturePath);
	lockOnReticleSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	missileLockOnReticleSprite_ = std::make_unique<Sprite>();
	missileLockOnReticleSprite_->Initialize(SpriteCommon::GetInstance(), kMissileLockOnReticleTexturePath);
	missileLockOnReticleSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	TextureManager::GetInstance()->LoadTexture("resources/multi_lock_marker.png");
	multiLockMarkerSprite_ = std::make_unique<Sprite>();
	multiLockMarkerSprite_->Initialize(SpriteCommon::GetInstance(), "resources/multi_lock_marker.png");
	multiLockMarkerSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	TextureManager::GetInstance()->LoadTexture("resources/white1x1.png");
	spGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	spGaugeBackgroundSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	spGaugeFillSprite_ = std::make_unique<Sprite>();
	spGaugeFillSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	spGaugeCostMarkerSprite_ = std::make_unique<Sprite>();
	spGaugeCostMarkerSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");

	const char *hudTexturePaths[] = {
		"resources/hud_panel_frame.png",
		"resources/hud_label_hp.png",
		"resources/hud_label_ammo.png",
		"resources/hud_label_sp.png",
		"resources/hud_digits.png",
		"resources/hud_ammo_icons.png"
	};
	for (const char *path : hudTexturePaths) {
		TextureManager::GetInstance()->LoadTexture(path);
	}
	hudPanelSprite_ = std::make_unique<Sprite>();
	hudPanelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[0]);
	hudAmmoPanelSprite_ = std::make_unique<Sprite>();
	hudAmmoPanelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[0]);
	hpGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	hpGaugeBackgroundSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	hpGaugeFillSprite_ = std::make_unique<Sprite>();
	hpGaugeFillSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	hudHpLabelSprite_ = std::make_unique<Sprite>();
	hudHpLabelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[1]);
	hudAmmoLabelSprite_ = std::make_unique<Sprite>();
	hudAmmoLabelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[2]);
	hudSpLabelSprite_ = std::make_unique<Sprite>();
	hudSpLabelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[3]);
	hudNormalAmmoIconSprite_ = std::make_unique<Sprite>();
	hudNormalAmmoIconSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[5]);
	hudHomingAmmoIconSprite_ = std::make_unique<Sprite>();
	hudHomingAmmoIconSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[5]);
	hudNormalReloadGaugeSprite_ = std::make_unique<Sprite>();
	hudNormalReloadGaugeSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	hudHomingReloadGaugeSprite_ = std::make_unique<Sprite>();
	hudHomingReloadGaugeSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("resources/radar_frame.png");
	radarFrameSprite_ = std::make_unique<Sprite>();
	radarFrameSprite_->Initialize(SpriteCommon::GetInstance(), "resources/radar_frame.png");
	radarFrameSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	radarSweepSprite_ = std::make_unique<Sprite>();
	radarSweepSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	radarSweepSprite_->SetAnchorPoint({ 0.0f, 0.5f });
	for (auto &blipSprite : radarBlipSprites_) {
		blipSprite = std::make_unique<Sprite>();
		blipSprite->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
		blipSprite->SetAnchorPoint({ 0.5f, 0.5f });
	}
	for (auto &digitSprite : hudHpDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[4]);
	}
	for (auto &digitSprite : hudNormalAmmoDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[4]);
	}
	for (auto &digitSprite : hudHomingAmmoDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[4]);
	}
	spGauge_ = 100.0f;
	isSpecialAttackActive_ = false;
	specialAttackFrame_ = 0;
	normalAmmoInMagazine_ = kNormalMagazineCapacity;
	normalAmmoReserve_ = 90;
	homingAmmoInMagazine_ = kHomingMagazineCapacity;
	homingAmmoReserve_ = 16;
	isNormalReloading_ = false;
	isHomingReloading_ = false;
	normalReloadFrame_ = 0;
	homingReloadFrame_ = 0;
	defeatedSmallEnemyCount_ = 0;
	ammoPickups_.clear();

	ModelManager::GetInstance()->CreatePlaneModel("BoundaryAlertPlane");
	Model* alertModel = ModelManager::GetInstance()->FindModel("BoundaryAlertPlane");
	if (alertModel) {
		alertModel->SetTextureFilePath(kBoundaryAlertTexturePath);
		alertModel->SetAlphaReference(0.05f); // Discard almost-black background
	}
	boundaryAlertObject_ = std::make_unique<Object3d>();
	boundaryAlertObject_->Initialize(Object3dCommon::GetInstance());
	boundaryAlertObject_->SetModel("BoundaryAlertPlane");
	ceilingBoundaryAlertObject_ = std::make_unique<Object3d>();
	ceilingBoundaryAlertObject_->Initialize(Object3dCommon::GetInstance());
	ceilingBoundaryAlertObject_->SetModel("BoundaryAlertPlane");

	// SkyboxCommon 鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ DirectX 鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｲ繝ｻ・ｰ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢繝ｻ・ｧ髯ｷ・ｻ髣鯉ｽｨ繝ｻ・ｽ繝ｻ・ｸ郢晢ｽｻ繝ｻ・｡鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ驕ｯ・ｶ繝ｻ・ｻ鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蠑ｱ繝ｻ繝ｻ繝ｻ蛻ｹ繝ｻ・ｹ髫ｰ雋ｻ・ｽ・ｶ髫ｨ蛟･繝ｻ繝ｻ・ｹ繝ｻ・ｧ髯ｷ闌ｨ・ｽ・ｷ郢晢ｽｻ繝ｻ・ｼ驛｢譎｢・ｽ・ｻ
	// SkyboxCommon is now initialized in Framework.cpp

	// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴弱・魃ｵ驛｢譎｢・ｽ・｣鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｨ・ｾ陟・屮・ｽ・ｪ繝ｻ・ｸ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｨ鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蠑ｱ繝ｻ繝ｻ繝ｻ蛻ｹ繝ｻ・ｹ驛｢譎｢・ｽ・ｻ


	//Model驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｻ鬩幢ｽ｢隴弱・・ｱ螢ｹ繝ｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("multiMesh.obj");
	ModelManager::GetInstance()->CreateSphereModel("Sphere", 16);
	ModelManager::GetInstance()->CreateSphereModel("AmmoPickupSphere", 16);
	if (Model *pickupModel = ModelManager::GetInstance()->FindModel("AmmoPickupSphere")) {
		pickupModel->SetColor({ 0.15f, 1.0f, 0.35f, 1.0f });
	}

	//======================================================
	// 鬩幢ｽ｢隴惹ｸ橸ｽｹ・ｲ繝ｻ蜿悶渚繝ｻ・ｹ隴弱・・ｽ・ｺ陋滂ｽ･・主ｬﾎ斐・・ｧ郢晢ｽｻ繝ｻ・｣鬩幢ｽ｢隴寂・繝ｻ鬯ｨ・ｾ陟・屮・ｽ・ｪ繝ｻ・ｸEE
	//======================================================

	// 鬮ｯ諛ｶ・ｽ・ｨ郢晢ｽｻ繝ｻ・ｰ鬯ｯ・ｮ繝ｻ・ｱ郢晢ｽｻ繝ｻ・｢鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・｢鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ繝ｻ蠑ｱ繝ｻ
	// groundModel = std::make_unique<Object3d>();
	// groundModel->Initialize(Object3dCommon::GetInstance());
	// groundModel->SetModel("plane.obj");
	// groundModel->SetScale({ 3000.0f, 1.0f, 3000.0f });
	// groundModel->SetTranslate({ 0.0f, 0.0f, 0.0f });
	// objects.push_back(groundModel.get());

	// 鬯ｨ・ｾ郢晢ｽｻ郢晢ｽｻE
	myShere = std::make_unique<Primitive>();
	myShere->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Sphere);
	myShere->SetTranslate({ 2.0f,0.0f,0.0f });
	// objects.push_back(myShere.get());
	// 鬩幢ｽ｢隴弱・繝ｻ郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｨ鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ驕ｯ・ｶ繝ｻ・ｻ鬩幢ｽ｢繝ｻ・ｧ驛｢・ｧ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｽ郢晢ｽｻ繝ｻ・ｿ鬩幢ｽ｢繝ｻ・ｧ髣包ｽｳ陞ゅ・・ｽ・ｽ隶呵ｶ｣・ｽ・ｹ繝ｻ・ｧ髣包ｽｵ隴擾ｽｶ髯橸ｽｺ鬩幢ｽ｢繝ｻ・ｧ驕ｶ荳橸｣ｰ莉ｰﾂ驕ｶ謫ｾ・ｽ・ｫ髯晢ｽｯ繝ｻ・ｼ鬯ｩ蛹・ｽｽ・ｶ髣包ｽｵ隴擾ｽｶ陷ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ髯具ｽｹ繝ｻ・ｻ驕ｶ蛹・ｽｽ・ｧ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬮ｫ菫ｶ隱薙・・ｽ繝ｻ・､鬩搾ｽｵ繝ｻ・ｺ髣包ｽｳ陞ゅ・・ｽ・ｼ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｦ鬩搾ｽｵ繝ｻ・ｺ鬩怜遜・ｽ・ｫ郢晢ｽｻ繝ｻ・･
	if (myShere->GetModel()) {
		myShere->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
	}

	// 鬩幢ｽ｢隴弱・魃ｵ驛｢譎｢・ｽ・｣鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ
	myBox = std::make_unique<Primitive>();
	myBox->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Box);
	myBox->SetTranslate({ -2.0f,0.0f,0.0f });
	// objects.push_back(myBox.get()); // Box鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮｣豈費ｽｼ螟ｲ・ｽ・ｽ繝ｻ・｣鬩幢ｽ｢繝ｻ・ｧ髣包ｽｳ陞ゅ・・ｽ・ｽ鬯倩ｲｻ・ｽ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫModel驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ髯ｷ莉｣繝ｻ繝ｻ・ｽ繝ｻ・ｽ郢晢ｽｻ繝ｻ・ｿ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ

	// 鬮ｯ・ｷ髢ｧ・ｴ郢晢ｽｻ髯懆ｶ｣・ｽ・ｪModel驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ
	myModelObject = std::make_unique<Object3d>();
	myModelObject->Initialize(Object3dCommon::GetInstance());
	ModelManager::GetInstance()->LoadModel("AnimatedCube/AnimatedCube.gltf");
	myModelObject->SetModel("AnimatedCube/AnimatedCube.gltf");
	//objects.push_back(myModelObject.get());

	// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・｢鬩幢ｽ｢隴乗・・ｽ・ｹ隴∵ｻ・ｱｪ繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｧ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｨ鬩幢ｽ｢隴弱・・ｽ・ｶ繝ｻ・｣郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢隴寂・・ｴ貅ｯ謫ｽ繝ｻ・ｴ鬮ｯ讖ｸ・ｽ・ｻ郢晢ｽｻ繝ｻ・､鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｮ・ｫ繝ｻ・ｱ郢晢ｽｻ繝ｻ・ｭ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｿ鬯ｮ・ｴ髮懶ｽ｣繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｿ
	animationData = LoadAnimationFile("resources/AnimatedCube", "AnimatedCube.gltf");
	Node rootNode = Model::LoadNodeHierarchy("resources/AnimatedCube", "AnimatedCube.gltf");
	skeleton = CreateSkeleton(rootNode);
	if (!skeleton.joints.empty()) {
		skeleton.joints[skeleton.root].transform.translate = { 0.0f, 0.0f, 0.0f };
	}

	myModelObject->skinCluster = myModelObject->GetModel()->CreateSkinCluster(skeleton);

	// 鬩幢ｽ｢隴弱・繝ｻ郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｨ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢隴弱・ﾂｧ驍ｵ・ｺ陞溘ｑ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｧ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴寂握縺狗ｹ晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蠑ｱ繝ｻ繝ｻ繝ｻ蛻ｹ繝ｻ・ｹ驛｢譎｢・ｽ・ｻ
	ModelManager::GetInstance()->CreateLineModel("SkeletonLines");
	skeletonLinesObject = std::make_unique<Object3d>();
	skeletonLinesObject->Initialize(Object3dCommon::GetInstance());
	skeletonLinesObject->SetModel("SkeletonLines");

	// 鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｨ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｳ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隰ｨ魑ｴﾂ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬯ｮ・ｯ繝ｻ・ｦ郢晢ｽｻ繝ｻ・ｨ鬯ｩ遨ゑｽｼ螟ｲ・ｽ・ｽ繝ｻ・ｺ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢隴弱・ﾂｧ驍ｵ・ｺ陞溘ｑ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｧ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴寂握縺狗ｹ晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蠑ｱ繝ｻ繝ｻ繝ｻ蛻ｹ繝ｻ・ｹ驛｢譎｢・ｽ・ｻ
	ModelManager::GetInstance()->CreateLineModel("DebugColliderLines");
	debugColliderLinesObject = std::make_unique<Object3d>();
	debugColliderLinesObject->Initialize(Object3dCommon::GetInstance());
	debugColliderLinesObject->SetModel("DebugColliderLines");

	// 鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｨ鬩幢ｽ｢隴弱・・ｽ・ｼ鬩･繝ｻ繽阪・・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・｡鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蠑ｱ繝ｻ繝ｻ繝ｻ蛻ｹ繝ｻ・ｹ驛｢譎｢・ｽ・ｻ
	debugFlyCamera_ = std::make_unique<FlyCamera>();
	debugFlyCamera_->SetTranslate({ 0.0f, 5.0f, -20.0f }); // 鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蠑ｱ繝ｻ繝ｻ繝ｻ陂ｭ繝ｻ・ｴ髯ｷ・･繝ｻ・ｲ郢晢ｽｻ繝ｻ・ｽ郢晢ｽｻ繝ｻ・ｮ
	isDebugCameraActive_ = false;


	// 鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｰ

	// 鬯ｯ・ｩ陝ｷ・｢繝ｻ・ｽ繝ｻ・ｨ鬮ｯ蜈ｷ・ｽ・ｻ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｰ (鬮｣蛹・ｽｽ・ｳ鬨ｾ蛹・ｽｽ・ｻ髯滓・莠九・・ｭ陝ｶ蜷ｶ繝ｻ

	// 鬮ｯ・ｷ・つ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｨ鬩幢ｽ｢隴弱・・ｽ・ｼ隴∫ｵｶ蜃ｾ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ

	//鬩幢ｽ｢隴寂・・ｲ・ｬ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ
	environmentRenderer_->GetParticleManager()->CreateParticleGroup("test", "resources/circle.png");
	environmentRenderer_->GetParticleManager()->CreateParticleGroup("smoke", "resources/circle.png");

	//鬯ｯ・ｮ繝ｻ・ｻ郢晢ｽｻ繝ｻ・ｳ鬮ｯ讖ｸ・ｽ・｢郢晢ｽｻ繝ｻ・ｰ鬮ｯ・ｷ・つ髯ｷ・･繝ｻ・ｲ髯ｷ繝ｻ・ｽ・ｽ
	soundData1 = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");
	soundData2 = AudioManager::GetInstance()->LoadAudio("resources/maou_bgm_fantasy15.mp3");
	songSoundData = AudioManager::GetInstance()->LoadAudio("resources/song_bgm.mp3");

	pVoice1=AudioManager::GetInstance()->PlayWave(soundData1, true);
	pVoice2=AudioManager::GetInstance()->PlayWave(soundData2, true);
	pSongVoice=AudioManager::GetInstance()->PlayWave(songSoundData, true);
	if (pSongVoice) pSongVoice->SetVolume(0.0f);

	// 1. 鬩幢ｽ｢隴弱・・ｽ・ｧ繝ｻ・ｭ驛｢譎｢・ｽ・ｭ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｸ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・｣鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬯ｩ謳ｾ・ｽ・ｨ髫ｶ蜷晢ｽｮ闌ｨ・ｽ・ｽ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬩幢ｽ｢隴主・讓溘・蜿厄ｽｨ謚ｵ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ鬮ｯ譏ｴ繝ｻ繝ｻ陋ｾﾂ・｡鬮｢繝薪el驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ髯ｷ莉｣繝ｻ繝ｻ・ｽ繝ｻ・ｽ髫ｲ蟶幢ｽ･繝ｻ・ｽ・ｽ郢晢ｽｻ
	ModelManager::GetInstance()->CreateTrailModel("SmokeTrail");

	// 2. 鬩幢ｽ｢隴主・讓溘・蜿厄ｽｨ謚ｵ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ鬯ｮ・ｫ繝ｻ・ｪ鬮｢・ｧ繝ｻ・ｲ郢晢ｽｻ繝ｻ・ｮ鬩阪・・ｽ・ｲ郢晢ｽｻ繝ｻ・ｩ髮狗ｿｫ・代・・ｽ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蠑ｱ繝ｻ繝ｻ繝ｻ蛻ｹ繝ｻ・ｹ髯ｷ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｼ髣費｣ｰ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｻ鬯ｮ・ｮ遶乗劼・ｽ・ｱ鬪ｰ蜈ｷ・ｽ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｯ60鬩幢ｽ｢隴弱・・ｽ・ｼ鬩･繝ｻ・ｨ謚ｵ・ｽ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・｣繝ｻ・ｰ=鬯ｩ蝣ｺ・ｸ鄙ｫ繝ｻ鬯ｩ遨ゑｽｿ・ｶ隲ｷ・｣郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｯ・ｮ雋翫ｑ・ｽ・ｽ繝ｻ・ｷ鬩搾ｽｵ繝ｻ・ｺ鬮ｴ驛・ｽｲ・ｻ繝ｻ・ｽ陞ｳ螟ｲ・ｽ・ｰ繝ｻ・ｿ髣包ｽｵ隴擾ｽｶ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE
	missileTrail = std::make_unique<Trail>();
	missileTrail->Initialize(60);

	// 3. 鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｨ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢隴弱・ﾂｧ驍ｵ・ｺ陞溘ｑ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｧ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴寂握縺狗ｹ晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蠑ｱ繝ｻ繝ｻ繝ｻ蛻ｹ繝ｻ・ｹ驛｢譎｢・ｽ・ｻ
	trailObject = std::make_unique<Object3d>();
	trailObject->Initialize(Object3dCommon::GetInstance());
	trailObject->SetModel("SmokeTrail");


	ModelManager::GetInstance()->CreateBoxModel("PlayerBox");
	ModelManager::GetInstance()->CreateBoxModel("EnemyBox");
	ModelManager::GetInstance()->CreateBoxModel("BossHull");
	ModelManager::GetInstance()->CreateBoxModel("BossCannon");
	ModelManager::GetInstance()->CreateBoxModel("BossBeam");
	if (Model *model = ModelManager::GetInstance()->FindModel("BossHull")) model->SetColor({ 0.18f, 0.22f, 0.35f, 1.0f });
	if (Model *model = ModelManager::GetInstance()->FindModel("BossCannon")) model->SetColor({ 1.0f, 0.35f, 0.05f, 1.0f });
	if (Model *model = ModelManager::GetInstance()->FindModel("BossBeam")) model->SetColor({ 0.15f, 0.8f, 1.0f, 1.0f });
	ModelManager::GetInstance()->CreateBoxModel("ObstacleBox");

	player_ = std::make_unique<Player>();
	player_->Initialize(kPlayerModelName);

	// 鬮ｯ貊捺ｱ壹・・ｽ繝ｻ・ｾ
	missileManager_ = std::make_unique<MissileManager>();
	missileManager_->Initialize(environmentRenderer_->GetParticleManager());

	// 鬮ｴ雜｣・ｽ・ｷ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｨ鬩幢ｽ｢隴弱・・ｽ・ｼ隴∫ｵｶ蜃ｾ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ
	explosionManager_ = std::make_unique<ExplosionManager>();
	explosionManager_->Initialize(environmentRenderer_->GetParticleManager());

	enemies_.clear();

	enemyBulletManager_ = std::make_unique<EnemyBulletManager>();
	enemyBulletManager_->Initialize();

	simulationManager_ = std::make_unique<SimulationManager>(this);
	missilePresetManager_ = std::make_unique<MissilePresetManager>(this);
	lockOnManager_ = std::make_unique<LockOnManager>(this);

	// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｲ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・｣繝ｻ・ｰ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴弱・繝ｻ郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬮ｮ蜿･・ｴ雜｣・ｽ・ｲ繝ｻ・ｻ郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蠑ｱ繝ｻ繝ｻ繝ｻ蛻ｹ繝ｻ・ｹ驛｢譎｢・ｽ・ｻ
	isGameOver_ = false;
	gameOverTimer_ = 0;

	ReloadSceneJson();
// 	simulationManager_->RefreshSimulationActionNames();
// 	missilePresetManager_->RefreshMissilePresetNames();

	if (IsSimulationMode()) {
		isEditorPreviewPlaying_ = false;
		uiManager_->currentSimulationTarget_ = 2;
		uiManager_->showSimulationWindow_ = true;
		SetDebugCameraActive(true);
	}

	// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｨ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｿ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｬ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴弱・繝ｻ郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蠑ｱ繝ｻ繝ｻ繝ｻ蛻ｹ繝ｻ・ｹ驛｢譎｢・ｽ・ｻ
	EditorReceiver::GetInstance()->Initialize();
}

void GamePlayScene::SetDebugCameraActive(bool isActive) {
	if (isDebugCameraActive_ == isActive) {
		return;
	}

	isDebugCameraActive_ = isActive;
	isCinematicLockOnCameraInitialized_ = false;
	if (isDebugCameraActive_) {
		debugFlyCamera_->SetTranslate(camera->GetTranslate());
		Object3dCommon::GetInstance()->SetDefaultCamera(debugFlyCamera_.get());
		OutputDebugStringA("[DebugCamera] ON: FlyCamera\n");
	} else {
		Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
		OutputDebugStringA("[DebugCamera] OFF: Player Camera\n");
	}
}

void GamePlayScene::ReloadSceneJson() {
	bossSpawned_ = false;
	lockedEnemy_ = nullptr;
	aimAssistEnemy_ = nullptr;
	if (lockOnManager_) {
		lockOnManager_->CancelMultiLock();
	}
	isCinematicLockOnCameraInitialized_ = false;
	enemies_.clear();
	obstacles_.clear();
	enemySpawns_.clear();

	StageLoader::LoadSceneJson("resources/scene.json", enemies_, obstacles_, player_.get(), &enemySpawns_);
	enemyRespawnTimers_.assign(enemySpawns_.size(), kNoEnemyRespawnTimer);

	// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴主・蜃ｽ繝ｻ雜｣・ｽ・ｦ鬩幢ｽ｢隴主・讓滄Δ譎｢・ｽ・ｧ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｿ鬩幢ｽ｢繝ｻ・ｧ髫ｰ螟ｲ・ｽ・ｵ郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬯ｮ・ｫ繝ｻ・ｱ郢晢ｽｻ繝ｻ・ｭ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｿ鬯ｮ・ｴ髮懶ｽ｣繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｿ
	enemyEventManager_.LoadEvents("resources/enemy_events.json");
	// Blender/StageLoaderで設定された初期スポーン設定(isInitialSpawn)をそのまま尊重する

	// 障害物のメッシュコライダー等を事前に構築・更新（敵の着地スナップ前に必須）
	for (auto& obstacle : obstacles_) {
		if (obstacle) {
			obstacle->Update();
		}
	}

	for (size_t i = 0; i < enemySpawns_.size(); ++i) {
		if (enemySpawns_[i].isInitialSpawn) {
			SpawnEnemyFromSpawnPoint(i);
		}
	}

	// 地上雑魚敵 5 体の直出し配置 (Blender未配置のハードコード敵は出さない)
	// SpawnDefaultGroundEnemies();

	try {
		lastJsonWriteTime_ = std::filesystem::last_write_time("resources/scene.json");
	} catch (...) {
		// JSON鬩搾ｽｵ繝ｻ・ｺ髯溷供・ｨ・ｯ驕ｨ螳｣縺励・・ｺ郢晢ｽｻ繝ｻ・ｰ鬮ｯ譏ｴ繝ｻ繝ｻ・ｼ隲帛､ｷ蟶昴＠繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ驕ｶ莨√・繝ｻ・ｸ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬮ｯ・ｷ繝ｻ・ｷ髯具ｽｹ繝ｻ・ｻ驍ｵ・ｲ陜｣・､繝ｻ・ｹ繝ｻ・ｧ驛｢・ｧ郢晢ｽｻ・つ驕ｶ荳橸ｽ｣・ｹ隨卍鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｿ鬮ｫ・ｰ繝ｻ・ｫ髯懶ｽ｣繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｽ髫ｲ蟶幢ｽ･繝ｻ・ｽ・ｽ陝ｶ譏ｴ・髯橸ｽ｢繝ｻ・ｹ郢晢ｽｻ繝ｻ・ｰ鬩幢ｽ｢繝ｻ・ｧ髯晢ｽｲ繝ｻ・ｨ郢晢ｽｻ隶呵ｶ｣・ｽ・ｹ繝ｻ・ｧ髣包ｽｵ隴趣ｽ｢繝ｻ・ｽ髢ｧ・ｲ繝ｻ・ｸ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ郢晢ｽｻ
	}
}

void GamePlayScene::ResetEditorPreview() {
	isEditorPreviewPlaying_ = false;
	isGameOver_ = false;
	gameOverTimer_ = 0;
	lockedEnemy_ = nullptr;
	aimAssistEnemy_ = nullptr;
	if (lockOnManager_) {
		lockOnManager_->CancelMultiLock();
	}
	isCinematicLockOnCameraInitialized_ = false;

	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	if (player_) {
		player_->Initialize(kPlayerModelName);
	}
	if (missileManager_) {
		missileManager_->Initialize(environmentRenderer_->GetParticleManager());
	}
	if (enemyBulletManager_) {
		enemyBulletManager_->Initialize();
	}
	if (explosionManager_) {
		explosionManager_->Initialize(environmentRenderer_->GetParticleManager());
	}

	ReloadSceneJson();

	if (!isDebugCameraActive_ && player_) {
		Vector3* targetPos = nullptr;
		Vector3 enemyPos;
		if (lockedEnemy_) {
			enemyPos = lockedEnemy_->GetPosition();
			targetPos = &enemyPos;
		}
		player_->UpdateCamera(camera.get(), targetPos);
	}

	OutputDebugStringA("[EditorPreview] Reset scene and paused.\n");
}

void GamePlayScene::SpawnEnemyFromSpawnPoint(size_t spawnPointIndex) {
	if (spawnPointIndex >= enemySpawns_.size()) {
		return;
	}

	const EnemySpawnData &spawnData = enemySpawns_[spawnPointIndex];
	std::unique_ptr<Enemy> enemy;
	if (spawnData.isBoss) {
		enemy = std::make_unique<Boss>();
	} else if (spawnData.isJammer) {
		enemy = std::make_unique<JammerEnemy>();
	} else if (spawnData.isGround) {
		enemy = std::make_unique<GroundEnemy>();
	} else {
		enemy = std::make_unique<Enemy>();
	}

	enemy->Initialize(spawnData.position);
	enemy->SetRotation(spawnData.rotation);
	if (spawnData.flightPath.IsValid()) {
		enemy->SetFlightPath(spawnData.flightPath.points, spawnData.flightPath.loop, spawnData.flightPath.speed);
	}
	enemy->SetSpawnPointIndex(spawnPointIndex);

	if (GroundEnemy *ge = dynamic_cast<GroundEnemy *>(enemy.get())) {
		ge->SnapToGround(obstacles_);
	}

	enemies_.push_back(std::move(enemy));

	if (spawnPointIndex < enemyRespawnTimers_.size()) {
		enemyRespawnTimers_[spawnPointIndex] = kNoEnemyRespawnTimer;
	}
}

void GamePlayScene::SpawnDefaultGroundEnemies() {
	// Blender未配置のハードコード敵は生成しない
}

bool GamePlayScene::IsEnemySpawnPointActive(size_t spawnPointIndex) const {
	for (const auto &enemy : enemies_) {
		if (enemy && enemy->GetSpawnPointIndex() == spawnPointIndex && !enemy->IsDead()) {
			return true;
		}
	}
	return false;
}

void GamePlayScene::ScheduleEnemySpawn(size_t spawnPointIndex, int delayFrames) {
	if (spawnPointIndex >= enemySpawns_.size()) {
		return;
	}
	if (enemyRespawnTimers_.size() < enemySpawns_.size()) {
		enemyRespawnTimers_.resize(enemySpawns_.size(), kNoEnemyRespawnTimer);
	}
	if (enemyRespawnTimers_[spawnPointIndex] != kNoEnemyRespawnTimer || IsEnemySpawnPointActive(spawnPointIndex)) {
		return;
	}

	enemyRespawnTimers_[spawnPointIndex] = delayFrames > 0 ? delayFrames : 1;
}

void GamePlayScene::TriggerEnemyReinforcements(const std::string &deadEnemyName) {
	if (deadEnemyName.empty()) {
		return;
	}

	const std::vector<EnemyEvent> events = enemyEventManager_.GetEventsForTrigger(deadEnemyName);
	for (const EnemyEvent &event : events) {
		for (size_t spawnPointIndex = 0; spawnPointIndex < enemySpawns_.size(); ++spawnPointIndex) {
			if (enemySpawns_[spawnPointIndex].name == event.targetEnemyName) {
				ScheduleEnemySpawn(spawnPointIndex, event.delayFrames);
				break;
			}
		}
	}

	for (size_t spawnPointIndex = 0; spawnPointIndex < enemySpawns_.size(); ++spawnPointIndex) {
		EnemySpawnData &spawnData = enemySpawns_[spawnPointIndex];
		if (!spawnData.remainingReinforcementTriggers.empty()) {
			auto it = std::find(spawnData.remainingReinforcementTriggers.begin(), spawnData.remainingReinforcementTriggers.end(), deadEnemyName);
			if (it != spawnData.remainingReinforcementTriggers.end()) {
				spawnData.remainingReinforcementTriggers.erase(it);
				if (spawnData.remainingReinforcementTriggers.empty()) {
					ScheduleEnemySpawn(spawnPointIndex, spawnData.reinforcementDelayFrames);
				}
			}
		}
	}
}

void GamePlayScene::UpdateEnemyRespawns() {
	if (enemyRespawnTimers_.size() < enemySpawns_.size()) {
		enemyRespawnTimers_.resize(enemySpawns_.size(), kNoEnemyRespawnTimer);
	}

	for (size_t spawnPointIndex = 0; spawnPointIndex < enemyRespawnTimers_.size(); ++spawnPointIndex) {
		int &timer = enemyRespawnTimers_[spawnPointIndex];
		if (timer == kNoEnemyRespawnTimer) {
			continue;
		}
		if (timer > 0) {
			--timer;
		}
		if (timer == 0) {
			SpawnEnemyFromSpawnPoint(spawnPointIndex);
		}
	}
}

bool GamePlayScene::HasPendingEnemySpawns() const {
	for (int timer : enemyRespawnTimers_) {
		if (timer != kNoEnemyRespawnTimer) {
			return true;
		}
	}
	return false;
}





























MissileTuning GamePlayScene::MakeMissileTuning(MissileType type) const {
	MissileTuning tuning;
	if (type == MissileType::Normal) {
		tuning.speed = missileNormalSpeed;
		tuning.homingStrength = 0.0f;
		tuning.scale = missileNormalScale;
		tuning.collisionRadius = missileNormalCollisionRadius;
		tuning.trailWidth = 0.0f;
		tuning.lifeTime = missileNormalLifeTime;
		return tuning;
	}

	tuning.speed = missileSpeed;
	tuning.homingStrength = missileHomingStrength;
	tuning.scale = missileHomingScale;
	tuning.collisionRadius = missileHomingCollisionRadius;
	tuning.trailWidth = missileTrailWidth;
	tuning.lifeTime = missileLifeTime;
	return tuning;
}































void GamePlayScene::Finalize() {
	if (pVoice1) {
		pVoice1->Stop();
		pVoice1->DestroyVoice();
		pVoice1 = nullptr;
	}
	if (pVoice2) {
		pVoice2->Stop();
		pVoice2->DestroyVoice();
		pVoice2 = nullptr;
	}
	if (pSongVoice) {
		pSongVoice->Stop();
		pSongVoice->DestroyVoice();
		pSongVoice = nullptr;
	}

	AudioManager::GetInstance()->UnloadWave(soundData1);
	AudioManager::GetInstance()->UnloadWave(soundData2);
	AudioManager::GetInstance()->UnloadWave(songSoundData);

	// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬮ｯ蜈ｷ・ｽ・ｻ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｿ鬩搾ｽｵ繝ｻ・ｺ髯懈瑳・ｺ・ｷ郢晢ｽｻ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢隴弱・・ｺ・｢驍ｵ・ｺ陝ｶ・ｷ繝ｻ・ｹ隴主・讓滄し・ｺ鬯倩ｲｻ・ｽ・ｹ隴弱・・ｽ・ｼ隴∫ｵｶ蜃ｾ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴主・讓溽ｹ晢ｽｻ陝ｶ譎｢・ｽ・ｨ繝ｻ・ｾ髯橸ｽ｢繝ｻ・ｼ郢晢ｽｻ繝ｻ・ｸ郢晢ｽｻ繝ｻ・ｸ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬮ｫ・ｰ鬲・ｼ夲ｽｽ・ｽ繝ｻ・ｻ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	EditorReceiver::GetInstance()->Finalize();
}

void GamePlayScene::Update() {

	// Blender鬩搾ｽｵ繝ｻ・ｺ髣包ｽｵ隴趣ｽ｢繝ｻ・ｽ髢ｾ・･繝ｻ・ｹ隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｿ鬩搾ｽｵ繝ｻ・ｺ髫ｴ・ｴ繝ｻ・ｧ髫ｰ・ｫ郢ｧ莨夲ｽｽ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｦ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ鬨ｾ蛹・ｽｽ・ｻ鬯ｮ・ｮ繝ｻ・ｰ鬩幢ｽ｢繝ｻ・ｧ髯句ｹ｢・ｽ・ｵ繝ｻ蜿悶渚繝ｻ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・｢鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｿ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・｣繝ｻ・ｰ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE
	if (EditorReceiver::GetInstance()->Update(player_.get(), enemies_, obstacles_, enemySpawns_)) {
		// Blenderで設定された初期スポーン設定(isInitialSpawn)をそのまま尊重する
	}


	// =========================================================
	// 鬩幢ｽ｢隴取得・ｽ・ｸ陷ｷ・ｶ・取坩ﾎ碑ｭ主・讓溘・蜿悶渚繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｭ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴乗・・ｽ・ｼ陞滂ｽｲ繝ｻ・ｽ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬯ｨ・ｾ繝ｻ・ｶ郢晢ｽｻ繝ｻ・｣鬯ｮ・ｫ陷肴ｺｽ・ｧ竏壹・繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬯ｨ・ｾ郢晢ｽｻ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE
	// =========================================================
	try {
		// 鬮｣逧ｮ逕･繝ｻ・･郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE "scene.json" 鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ鬮ｫ・ｴ鬲・ｼ夲ｽｽ・ｽ繝ｻ・･鬮ｫ・ｴ陟托ｽｱ繝ｻ繝ｻ繝ｻ陜｣・､繝ｻ・ｹ隴擾ｽｶ郢晢ｽｻ驍ｵ・ｺ髢ｾ・･繝ｻ・ｹ隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ郢晢ｽｻ
		auto currentTime = std::filesystem::last_write_time("resources/scene.json");

		// 鬩幢ｽ｢繝ｻ・ｧ驛｢・ｧ郢晢ｽｻ繝ｻ・ｼ繝ｻ・ｰ鬯ｮ・ｫ繝ｻ・ｪ髯句ｸ吶・繝ｻ・ｽ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ驕ｯ・ｶ繝ｻ・ｻ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬮ｫ・ｴ鬲・ｼ夲ｽｽ・ｽ繝ｻ・･鬮ｫ・ｴ陟托ｽｱ繝ｻ繝ｻ繝ｻ髢ｧ・ｲ繝ｻ・ｹ繝ｻ・ｧ鬩怜遜・ｽ・ｫ郢晢ｽｻ郢ｧ螂・ｽｽ・ｭ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ郢晢ｽｻ繝ｻ・ｰ鬩幢ｽ｢繝ｻ・ｧ鬯ｲ繝ｻ・ｼ螟ｲ・ｽ・ｽ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ髫ｰ魃会ｽｽ・ｪlender鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬮｣蛹・ｽｽ・ｳ鬯ｯ繝ｻ・､・ｧ繝ｻ・ｶ隶呵ｶ｣・ｽ・ｸ繝ｻ・ｺ髯懶ｽ｣繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｿ髫ｴ蜿門ｾ励・・ｽ繝ｻ・ｭ髯区ｻゑｽｽ・･郢晢ｽｻ郢晢ｽｻ繝ｻ・ｹ繝ｻ・ｧ髯溷供・ｨ・ｯ髯橸ｽｺ鬩幢ｽ｢繝ｻ・ｧ髣費ｽｨ陞滂ｽｲ繝ｻ・ｽ繝ｻ・ｼ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE
		if (currentTime > lastJsonWriteTime_) {
			ReloadSceneJson();

			// 鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｦ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・｣鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢隴取得・ｽ・ｳ繝ｻ・ｨ驍ｵ・ｺ髢ｧ・ｲ繝ｻ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬩搾ｽｵ繝ｻ・ｺ鬯ｯ蛟｡・｢謇假ｽｽ・｡陷･・ｲ繝ｻ・ｹ繝ｻ・ｧ髯晢ｽｲ繝ｻ・ｨ髫ｨ・ｳ霑｢證ｦ・ｽ・ｹ繝ｻ・ｧ髫ｰ螟ｲ・ｽ・ｵ郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ
			OutputDebugStringA("Hot Reloaded: scene.json 鬩幢ｽ｢繝ｻ・ｧ髫ｰ螟ｲ・ｽ・ｵ郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬯ｮ・ｫ繝ｻ・ｱ郢晢ｽｻ繝ｻ・ｭ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｿ鬯ｮ・ｴ髮懶ｽ｣繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｿ鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ驕ｶ謫ｾ・ｽ・ｪ鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ髫ｨ・ｳ郢晢ｽｻ郢晢ｽｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ\n");
		}
	} catch (...) {
		// 繝ｻ貊難ｽｧ・ｫ繝ｻ・ｺ郢晢ｽｻ陞ｻ繝ｻ・ｹ譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬯ｮ・ｫ髴域鱒繝ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽBlender鬩搾ｽｵ繝ｻ・ｺ髯溷供・ｾ貉厄ｽｨ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・｡鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｸ鬩搾ｽｵ繝ｻ・ｺ髯晢｣ｰ髮懶ｽ｣繝ｻ・ｽ繝ｻ・ｾ郢晢ｽｻ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ鬮ｦ・ｮ陷ｷ・ｶ・つ陜｣・､繝ｻ・ｸ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬮ｫ・ｴ陝・｢・つ鬮｣蛹・ｽｽ・ｳ郢晢ｽｻ繝ｻ・ｭ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬮ｫ・ｰ繝ｻ・ｨ郢晢ｽｻ繝ｻ・ｰ鬩幢ｽ｢隴弱・・ｽ・ｺ闖ｴ・ｩ隲橸ｽｺ・ゑｽｧ髫ｰ螟ｲ・ｽ・ｵ郢晢ｽｻ繝ｻ・ｼ髣費ｽｨ陞滂ｽｲ繝ｻ・ｽ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE
		// C++鬩搾ｽｵ繝ｻ・ｺ髣包ｽｵ隴趣ｽ｢繝ｻ・ｽ髢ｾ・･繝ｻ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・｢鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｻ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬩搾ｽｵ繝ｻ・ｺ鬯ｮ・ｦ繝ｻ・ｪ髫ｨ蛟･繝ｻ繝ｻ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｨ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢繝ｻ・ｧ髣包ｽｵ隴趣ｽ｢繝ｻ・ｼ郢晢ｽｻ繝ｻ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｨ鬩搾ｽｵ繝ｻ・ｺ髯溷供・ｨ・ｯ隴鯉ｽｺ鬩幢ｽ｢繝ｻ・ｧ髣包ｽｵ隴擾ｽｶ髯橸ｽｺ鬩幢ｽ｢繝ｻ・ｧ驕ｶ荳橸｣ｰ莉ｰﾂ驍ｵ・ｲ髢ｼ蝎・catch鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬮ｫ・ｰ繝ｻ・ｰ郢晢ｽｻ繝ｻ・｡鬩幢ｽ｢繝ｻ・ｧ鬩怜遜・ｽ・ｫ髫ｨ繝ｻ・ｽ・ｽ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｶ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ
	}

	const bool canUseKeyboardInput = !IsImGuiKeyboardCaptureActive();
	const bool canUseMouseInput = !IsImGuiMouseCaptureActive();

	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_0)) {
		OutputDebugStringA("HIt 0\n");
	}

	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_F2)) {
		if (IsSimulationMode()) {
			PostQuitMessage(0);
		} else {
			LaunchSimulationExecutable();
		}
		return;
	}

	if (canUseKeyboardInput && IsSimulationMode() && Input::GetInstance()->TriggerKey(DIK_F3)) {
		SetDebugCameraActive(!isDebugCameraActive_);
	}

	// R鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｭ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ髯ｷ・ｻ陜捺ｷ楪陷ｻ・ｵ陝謌奇ｽｭ謫ｾ・ｽ・ｴ繝ｻ繧托ｽｽ・ｰ鬩幢ｽ｢繝ｻ・ｧ髯晢ｽｲ繝ｻ・ｨ郢晢ｽｻ郢晢ｽｻ繝ｻ・ｹ繝ｻ・ｧ鬯ｯ菫ｶ・ｳ魃会ｽｽ・ｳ繝ｻ・ｩ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ
	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_R)) {
		SceneManager::GetInstance()->ChangeScene(IsSimulationMode() ? "SIMULATION" : "GAMEPLAY");
		return;
	}

	// ==========================================
	// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｲ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・｣繝ｻ・ｰ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴寂・繝ｻ鬮ｯ蜈ｷ・ｽ・ｻ郢晢ｽｻ繝ｻ・､鬮ｯ讖ｸ・ｽ・ｳ髯橸ｽ｢繝ｻ・ｹ驕ｶ髮・ｽｮ螟ｲ・ｽ・ｲ隶主･・ｽｽ・ｿ陜難ｽｼ繝ｻ・ｨ繝ｻ・ｾ郢晢ｽｻ繝ｻ・ｲ鬯ｮ・ｯ繝ｻ・ｦ驛｢譎｢・ｽ・ｻ
	// ==========================================
	if (!IsSimulationMode() && !isGameOver_ && player_ && player_->IsDead()) {
		isGameOver_ = true;
		gameOverTimer_ = 0;

		std::vector<Vector3> playerHitPos = { player_->GetPosition() };
		if (explosionManager_) {
			explosionManager_->CreateDestructionEffects(playerHitPos);
		}

		if (pVoice2) {
			pVoice2->Stop();
		}
	}

	bool shouldUpdateGame = true;

	if (isGameOver_) {
		gameOverTimer_++;

		// 鬯ｩ謳ｾ・ｽ・ｨ郢晢ｽｻ繝ｻ・ｶ鬮ｫ・ｴ陝ｶ蟷｢・ｽ・ｦ繝ｻ・｣鬯ｨ・ｾ陷茨ｽｷ繝ｻ・ｽ繝ｻ・ｽ鬯ｲ繝ｻ・ｺ・ｯ繝ｻ・ｲ隶抵ｽｫ陝・・鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｰ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｬ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｱ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫE鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｨ鬩幢ｽ｢隴弱・・ｽ・ｼ隴∫ｵｶ蜃ｾ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴主・讓溽ｹ晢ｽｻ陝ｶ譎｢・ｽ・ｩ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｩ鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｨEE
		if (PostEffect::GetInstance()) {
			float effectProgress = static_cast<float>(gameOverTimer_) / 120.0f;
			if (effectProgress > 1.0f) {
				effectProgress = 1.0f;
			}
			float vignetteRadius = 0.62f - 0.22f * effectProgress;
			float blurIntensity = 1.5f + 3.0f * effectProgress;
			PostEffect::GetInstance()->SetVignetteSmoothing(vignetteRadius, 0.38f, blurIntensity);
		}

		// 5鬩幢ｽ｢隴弱・・ｽ・ｼ鬩･繝ｻ・ｨ謚ｵ・ｽ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・｣繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ1鬮ｯ諛・ｻｸ繝ｻ・ｧ繝ｻ・ｭ髫ｨ繝ｻ・ｽ・｡鬩搾ｽｵ繝ｻ・ｺ鬮ｫ・ｨ繝ｻ・ｬ髯晢ｽｲ繝ｻ・ｩ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ霑｢證ｦ・ｽ・ｸ繝ｻ・ｺ鬮ｦ・ｮ陷ｷ・ｮ郢晢ｽｻ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬩搾ｽｵ繝ｻ・ｲ驕ｶ荳橸ｽ｣・ｹ隨ｳ遏ｩﾎ碑ｭ趣ｽ｢繝ｻ・ｽ繝ｻ・ｭ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・｢鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｧ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳE鬮｣蛹・ｽｽ・ｳ髣包ｽｵ雋翫・繝ｻ鬮ｯ蜿･・ｸ・ｶ郢ｩ・ｧ郢晢ｽｻ繝ｻ・ｭ郢晢ｽｻ繝ｻ・｢E鬩幢ｽ｢繝ｻ・ｧ鬮ｮ蛹ｺ・ｩ・ｸ繝ｻ・ｽ繝ｻ・ｮ髮九・・ｽ・ｽ髫ｶ謐ｺ・ｪ・ｸE
		shouldUpdateGame = (gameOverTimer_ % 5 == 0);

		// 鬯ｩ蝣ｺ・ｸ鄙ｫ繝ｻ鬯ｩ遨ゑｽｿ・ｶ隲ｷ・｣郢晢ｽｻ繝ｻ・ｼ驛｢譎｢・ｽ・ｻ20鬩幢ｽ｢隴弱・・ｽ・ｼ鬩･繝ｻ・ｨ謚ｵ・ｽ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・｣繝ｻ・ｰE鬯ｩ謳ｾ・ｽ・ｨ鬩募●豐厄ｾ代・縺励・・ｺ髯ｷ莨夲ｽｽ・ｱ髫ｨ・ｳ郢晢ｽｻ繝ｻ・ｹ繝ｻ・ｧ髯晢ｽｲ繝ｻ・ｨ繝ｻ縺､ﾂ驕ｶ謫ｾ・ｽ・ｵ郢晢ｽｻ繝ｻ・ｭ郢晢ｽｻ繝ｻ・｣鬮ｯ貅ｷ蠎翫・・ｸ陝ｯ・ｩ郢晢ｽｻ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｲ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・｣繝ｻ・ｰ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴寂・繝ｻ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｸ鬯ｯ・ｩ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｷ鬯ｩ蜍溪・繝ｻ・ｽ繝ｻ・ｻ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ騾｡・ｿE
		if (gameOverTimer_ >= 120) {
			SceneManager::GetInstance()->ChangeScene("GAMEOVER");
		}
	} else {
		// 鬯ｯ・ｨ繝ｻ・ｾ髯橸ｽ｢繝ｻ・ｼ郢晢ｽｻ繝ｻ・ｸ郢晢ｽｻ繝ｻ・ｸ鬮ｫ・ｴ陟托ｽｱ繝ｻ莉｣繝ｻ繝ｻ・ｼ髯橸ｽ｢繝ｻ・ｹ驛｢譎｢・ｽ・ｮ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴弱・・ｽ・ｧ繝ｻ・ｭ繝ｻ蜿門旭繝ｻ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｨ鬩幢ｽ｢隴弱・・ｽ・ｼ隴∫ｵｶ蜃ｾ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴主・讓溘・縺､ﾂ驕ｶ荳橸ｽ｣・ｺ驕ｨ螳｣縺励・・ｺ髮九・ﾂ・･郢晢ｽｻ鬩幢ｽ｢隴弱・ﾂｧ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢隴惹ｹ暦ｽｲ・ｺ髯ｷ繝ｻ・ｽ・ｾ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢隴弱・・ｱ蝣､・ｹ譎｢・ｽ・ｻ鬩幢ｽ｢隴守甥諢帷ｹ晢ｽｻ繝ｻ・ｼ髮主供・ｾ蠕後・
		if (PostEffect::GetInstance()) {
			bool isBoosting = false;
			if (player_ && player_->GetCurrentMode() == PlayerMode::Fighter) {
				float maxSpeed = player_->GetModeParams(PlayerMode::Fighter).maxMoveSpeed;
				float speed = MyMath::Length(player_->GetVelocity());
				if (speed > maxSpeed * 1.5f) {
					isBoosting = true;
					float effectProgress = std::clamp((speed - maxSpeed * 1.5f) / (maxSpeed * 3.0f - maxSpeed * 1.5f), 0.0f, 1.0f);
					float vignetteRadius = 0.5f - 0.1f * effectProgress; // 鬯ｮ・ｫ驕ｨ繧托ｽｽ・ｹ雋翫・繝ｻ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｴ謇假ｽｽ・｢郢晢ｽｻ繝ｻ・ｭ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｾ鬩幢ｽ｢繝ｻ・ｧ鬩怜遜・ｽ・ｫ郢晢ｽｻ陞ｳ螟ｲ・ｽ・ｬ隴会ｽｦ繝ｻ・ｽ繝ｻ・ｧ鬩搾ｽｵ繝ｻ・ｺ髯具ｽｹ繝ｻ・ｻ郢晢ｽｻ遶擾ｽｫ繝ｻ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ
					float blurIntensity = effectProgress * 0.5f; // 鬩幢ｽ｢隴弱・ﾂｧ繝ｻ荳ｻ・ｸ・ｷ繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ髯句ｹ｢・ｽ・ｵ繝ｻ繧托ｽｽ・ｰ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢繝ｻ・ｧ鬯ｮ・ｮ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｼ郢晢ｽｻ繝ｻ・ｱ鬩搾ｽｵ繝ｻ・ｺ髣包ｽｳ陞ゅ・・ｽ・ｼ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｦ鬮ｯ・ｷ魄・ｽｹ闔繧会ｽｪ・ｶ繝ｻ・ｲ鬯ｮ・ｫ驕ｨ繧托ｽｽ・ｹ隴擾ｽｶ隴・ｽ｡鬩幢ｽ｢繝ｻ・ｧ髣包ｽｵ隴趣ｽ｢繝ｻ・ｽ髢ｧ・ｲ繝ｻ・ｸ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ驕ｶ莨∬ｱｪ繝ｻ・ｸ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ郢晢ｽｻ
					PostEffect::GetInstance()->SetVignetteSmoothing(vignetteRadius, 0.4f, blurIntensity);
				}
			}
			
			if (player_) {
				if (previousPlayerHP_ == -1) {
					previousPlayerHP_ = player_->GetHP();
				} else if (player_->GetHP() < previousPlayerHP_) {
					damageEffectTimer_ = 30;
					previousPlayerHP_ = player_->GetHP();
				} else if (player_->GetHP() > previousPlayerHP_) {
					previousPlayerHP_ = player_->GetHP();
				}
			}

			if (damageEffectTimer_ > 0) {
				damageEffectTimer_--;
				PostEffect::GetInstance()->SetEffectType(13); // Fold Wave
			} else if (!isBoosting) {
				PostEffect::GetInstance()->SetEffectType(0); // 0: Normal
			}
		}
	}
	shouldUpdateGame = shouldUpdateGame && isEditorPreviewPlaying_;
	const bool isSimulation = IsSimulationMode();
	const bool isFullFlowPreview = !isSimulation || uiManager_->simulationPlaybackMode_ == 1;
	const bool isSelectedOnlyPreview = isSimulation && uiManager_->simulationPlaybackMode_ == 0;
	const bool updateSelectedPlayer = shouldUpdateGame && canUseKeyboardInput && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 0);
	const bool updateSelectedMissiles = shouldUpdateGame && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 1);
	const bool updateSelectedEnemies = shouldUpdateGame && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 2);
	const bool updateSelectedParticles = shouldUpdateGame && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 3);
	const bool allowMouseMissileFire = shouldUpdateGame && canUseMouseInput && (!isSimulation || isFullFlowPreview);
	const bool allowLockOnBehavior = !isGameOver_ && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 1 || uiManager_->currentSimulationTarget_ == 2);
	const bool updateDebugWireframes = !isSimulation || isFullFlowPreview || updateSelectedPlayer || updateSelectedMissiles || updateSelectedEnemies || updateSelectedParticles;
	const bool updateAnimationPreview = !isSimulation || isFullFlowPreview;

	const bool isAnimationEditor = isSimulation && uiManager_->currentSimulationTarget_ == 5;
	static bool wasAnimationEditor = false;
	if (isAnimationEditor && !wasAnimationEditor) {
		SetDebugCameraActive(true);
		if (player_ && debugFlyCamera_) {
			Vector3 pPos = player_->GetPosition();
			// 鬩幢ｽ｢隴惹ｸ橸ｽｹ・ｲ繝ｻ蜿厄ｽｨ謚ｵ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ譏ｴ繝ｻ繝ｻ・ｻ繝ｻ・｣郢晢ｽｻ繝ｻ・ｰ鬮ｯ貅ｷ萓帙・・ｾ鬲・ｼ夲ｽｽ・ｽ陷･・ｲ繝ｻ・ｸ繝ｻ・ｲ驕ｶ荳橸ｽ､・ｲ繝ｻ・ｽ郢晢ｽｻ繝ｻ・ｹ繝ｻ・ｧ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｸ鬩怜遜・ｽ・ｫ繝ｻ繧托ｽｽ・ｰ鬩幢ｽ｢繝ｻ・ｧ鬮｣繝ｻ・ｽ・ｽ郢晢ｽｻ繝ｻ・ｦ髯区ｻゑｽｽ・ｶ郢晢ｽｻ繝ｻ・ｸ髣包ｽｵ隴趣ｽ｢繝ｻ・ｽ陷･・ｲ繝ｻ・ｸ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ髢ｧ・ｲ繝ｻ・ｸ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ驕ｶ莨・ｽｦ・ｴ繝ｻ・ｩ雋・ｽｷ髫ｱ・ｿ郢晢ｽｻ繝ｻ・ｽ郢晢ｽｻ繝ｻ・ｮ (鬮｣蛹・ｽｽ・ｳ郢晢ｽｻ繝ｻ・ｭ鬮ｯ貊ゑｽｽ・｢驛｢譎｢・ｽ・ｻ驕ｶ鬆托ｽ･・｢繝ｻ・ｬ闔牙遜・ｽ・ｳ繝ｻ・ｨ驕ｶ謫ｾ・ｽ・ｴ鬩幢ｽ｢繝ｻ・ｧ驛｢譎｢・ｽ・ｻ
			debugFlyCamera_->SetTranslate({ pPos.x, pPos.y + 2.0f, pPos.z - 12.0f });
			debugFlyCamera_->SetQuaternion({ 0.0f, 0.0f, 0.0f, 1.0f });
		}
	} else if (!isAnimationEditor && wasAnimationEditor) {
		SetDebugCameraActive(false);
	}
	wasAnimationEditor = isAnimationEditor;

	if (shouldUpdateGame && canUseKeyboardInput && !isSpecialAttackActive_) {
		UpdateReload();
	}

	// C鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｭ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧSP鬩幢ｽ｢繝ｻ・ｧ驛｢譎｢・ｽ・ｻ0%鬮ｮ雜｣・ｽ・ｸ鬯ｩ蟶吶・繝ｻ・ｽ繝ｻ・ｲ郢晢ｽｻ繝ｻ・ｻ鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ繝ｻ縺､ﾂ驛｢譎｢・ｽ・ｻ鬯ｩ遨ゑｽｼ諛ｶ・ｽ・ｸ隴乗・・ｽ・ｿ繝ｻ・｣鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｯ・ｨ繝ｻ・ｾ郢晢ｽｻ繝ｻ・｣鬮ｯ譏ｴ繝ｻ郢晢ｽｻ郢晢ｽｻ繝ｻ・ｿ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｮ郢晢ｽｻ繝ｻ・ｺ鬮ｫ・ｰ陜｣莉ｰﾂ鬩幢ｽ｢繝ｻ・ｧ髯懶ｽ｣繝ｻ・､髯具ｽｹ繝ｻ・ｱ鬮ｯ・ｷ陝雜｣・ｽ・ｼ隰夲ｽｫ郢晢ｽｻ鬩幢ｽ｢繝ｻ・ｧ髣包ｽｵ隰ｨ魑ｴﾂ驛｢譎｢・ｽ・ｻ
	if (!isGameOver_ && shouldUpdateGame && canUseKeyboardInput &&
		!isSpecialAttackActive_ && spGauge_ >= kSpecialAttackCost &&
		Input::GetInstance()->TriggerKey(DIK_C)) {
		spGauge_ -= kSpecialAttackCost;
		isSpecialAttackActive_ = true;
		specialAttackFrame_ = 0;
		if (player_) {
			player_->SetSpecialAttackActive(true);
		}
	}

	// V鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｭ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬮ｮ蟇ゅ・繝ｻ・ｾ陟募ｨｯ繝ｻ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ繝ｻ荳ｻ繝ｻ郢晢ｽｻ髯具ｽｹ繝ｻ・ｻ繝ｻ蜿門旭繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢隴弱・・ｱ蝣､・ｸ・ｺ陷･・ｲ繝ｻ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｲ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｸ100%鬮ｮ雜｣・ｽ・ｸ鬯ｩ蟶吶・繝ｻ・ｽ繝ｻ・ｲ郢晢ｽｻ繝ｻ・ｻ驛｢譎｢・ｽ・ｻ髯晢ｽｲ繝ｻ・ｨ郢晢ｽｻ陝ｶ謨鳴陷茨ｽｷ繝ｻ・ｽ繝ｻ・ｺ鬮ｯ・ｷ陝雜｣・ｽ・ｼ隰夲ｽｫ郢晢ｽｻ鬩幢ｽ｢繝ｻ・ｧ髣包ｽｵ隰ｨ魑ｴﾂ驛｢譎｢・ｽ・ｻ
	if (!isGameOver_ && shouldUpdateGame && canUseKeyboardInput &&
		!isSongActive_ && songGauge_ >= 100.0f &&
		Input::GetInstance()->TriggerKey(DIK_V)) {
		songGauge_ = 0.0f;
		isSongActive_ = true;
		songFrame_ = 0;
		if (player_) {
			player_->SetSongActive(true);
		}
		if (pVoice2) pVoice2->SetVolume(0.0f);
		if (pSongVoice) pSongVoice->SetVolume(1.0f);
	}

	if (isSongActive_) {
		++songFrame_;
		if (songFrame_ >= 900) { // 15 seconds
			isSongActive_ = false;
			songFrame_ = 0;
			if (player_) {
				player_->SetSongActive(false);
			}
			if (pVoice2) pVoice2->SetVolume(1.0f);
			if (pSongVoice) pSongVoice->SetVolume(0.0f);
		}
	}

	if (isSpecialAttackActive_) {
		if (specialAttackFrame_ % kSpecialAttackFireIntervalFrames == 0 && missilePresetManager_) {
			// 鬯ｯ・ｨ繝ｻ・ｾ髯橸ｽ｢繝ｻ・ｼ郢晢ｽｻ繝ｻ・ｸ郢晢ｽｻ繝ｻ・ｸ鬮ｯ貊捺ｱ壹・・ｽ繝ｻ・ｾ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｨ鬯ｮ・ｫ繝ｻ・ｱ髣費ｽｨ隲幢ｽｶ繝ｻ・ｽ繝ｻ・ｰ髣包ｽｳ繝ｻ・ｻ郢晢ｽｻ繝ｻ・ｼ郢晢ｽｻ繝ｻ・ｾ鬩幢ｽ｢繝ｻ・ｧ髯ｷ・ｻ陜捺ｻゑｽｽ・｡郢晢ｽｻ隰壹・・ｫ蟶托ｽｼ螟ｲ・ｽ・ｽ繝ｻ・ｸ郢晢ｽｻ繝ｻ・ｭ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｴ雜｣・ｽ・｣郢晢ｽｻ繝ｻ・ｧ鬮ｮ荵昴・隰梧ｺｯ鮗ｾ繝ｻ・ｿ鬮ｯ・ｷ繝ｻ・ｷ髣比ｼ夲ｽｽ・｣驕ｶ蝓弱Γ隲・ｺ髫ｴ・ｴ繝ｻ・ｧ髯ｷ繝ｻ・ｽ・ｾ鬯ｨ・ｾ陷茨ｽｷ繝ｻ・ｽ繝ｻ・ｺ鬮ｯ譏ｴ繝ｻ郢晢ｽｻ髫ｨ蛟･繝ｻ繝ｻ・ｹ繝ｻ・ｧ髣包ｽｵ隰ｨ魑ｴﾂ驛｢譎｢・ｽ・ｻ
			missilePresetManager_->FirePlayerMissile(MissileType::Normal, nullptr, -0.3f);
			// 鬮ｯ蜈ｷ・ｽ・ｻ髫ｴ蜿厄ｽ･・ｪ・つ髮九・ﾂ・･郢晢ｽｻ鬮ｴ雜｣・ｽ・｣郢晢ｽｻ繝ｻ・ｧ鬮ｮ荵昴・隰梧ｺｯ鮗ｾ繝ｻ・ｿ鬮ｯ・ｷ繝ｻ・ｷ髣比ｼ夲ｽｽ・｣驕ｶ莨∬ｱｪ繝ｻ・ｸ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ驕ｯ・ｶ繝ｻ・ｻ鬮ｯ・ｷ鬯伜ｾ鯉ｼ企劑ﾂ繝ｻ・ｿ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｸ鬮ｯ譏ｴ繝ｻ郢晢ｽｻ驛｢譎｢・ｽ・ｻ鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ繝ｻ縺､ﾂ驕ｶ謫ｾ・ｽ・ｫ髯晢ｽｲ繝ｻ・ｩ鬯ｯ・ｨ繝ｻ・ｾ郢晢ｽｻ繝ｻ・ｲ鬮ｯ蜈ｷ・ｽ・ｹ郢晢ｽｻ繝ｻ・ｺ鬯ｯ・ｮ繝ｻ・｢鬮ｦ・ｮ陷ｷ・ｶ郢晢ｽｻ鬮ｯ貅ｷ萓帙・・ｾ陞ｽ・ｯ郢晢ｽｻ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ鬮ｯ蜿･・ｹ・｢繝ｻ・ｽ繝ｻ・ｴ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬮ｫ・ｰ繝ｻ・ｨ郢晢ｽｻ繝ｻ・ｵ鬩幢ｽ｢繝ｻ・ｧ髯ｷ・ｻ騾趣ｽｯ繝ｻ・ｻ陞ｳ螟ｲ・ｽ・ｬ闔牙遜・ｽ・ｳ繝ｻ・ｨ髫ｨ蛟･繝ｻ繝ｻ・ｹ繝ｻ・ｧ髣包ｽｵ隰ｨ魑ｴﾂ驛｢譎｢・ｽ・ｻ
			missilePresetManager_->FirePlayerMissile(MissileType::MissileWithTrail, nullptr, 0.3f);
		}
		++specialAttackFrame_;
		if (specialAttackFrame_ >= kSpecialAttackDurationFrames) {
			isSpecialAttackActive_ = false;
			specialAttackFrame_ = 0;
			if (player_) {
				player_->SetSpecialAttackActive(false);
			}
		}
	} else {
		spGauge_ = std::clamp(spGauge_ + kSpGaugeRecoveryPerFrame, 0.0f, 100.0f);
	}

	if (myBox && animationData.duration > 0.0f) {
		if (playAnimation && updateAnimationPreview) {
			animationTime += 1.0f / 60.0f;
			animationTime = std::fmod(animationTime, animationData.duration);
		}
		
		// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・｢鬩幢ｽ｢隴乗・・ｽ・ｹ隴∵ｻ・ｱｪ繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｧ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｨ鬯ｯ・ｯ繝ｻ・ｪ郢晢ｽｻ繝ｻ・ｨ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｸ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｯ・ｩ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｩ鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｨ
		ApplyAnimation(skeleton, animationData, animationTime);
		::Update(skeleton);
		if (enableSkinning && myModelObject->GetModel()) {
			myModelObject->GetModel()->UpdateSkinCluster(myModelObject->skinCluster, skeleton);
		}

		// 鬮｣遒代・鬮ｦ諞ｺﾎ斐・・ｧ驛｢譎｢・ｽ・ｻ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬮ｯ・ｷ繝ｻ・ｷ髯具ｽｹ繝ｻ・ｻ郢晢ｽｻ陷證ｦ・ｽ・ｸ繝ｻ・ｺ髯晢ｽｶ陷ｷ・ｮ髯橸ｽｺ鬮ｴ謇假ｽｽ・･郢晢ｽｻ繝ｻ・ｶ鬮ｫ・ｲ繝ｻ・ｷ髣包ｽｵ隴擾ｽｴ・つ陞ｳ螢ｽ蜑ｲ郢晢ｽｻ繝ｻ・ｿ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢繝ｻ・ｧ驕ｶ荳橸｣ｰ莉ｰﾂ驛｢譎｢・ｽ・ｾkeleton鬩搾ｽｵ繝ｻ・ｺ髣包ｽｵ隴趣ｽ｢繝ｻ・ｽ髯具ｽｾ陜ｮ譛ｱ蟇・・・ｲ郢晢ｽｻ繝ｻ・ｮ鬩穂ｼ夲ｽｽ・ｼ郢晢ｽｻ繝ｻ・ｵ髯ｷ莠･豐ｺ繝ｻ・｣繝ｻ・｡鬩幢ｽ｢繝ｻ・ｧ鬮ｮ蛹ｺ・ｧ・ｫ陟募ｮ｣ﾎ斐・・ｧ鬨ｾ・｡隶呵ｶ｣・ｽ・ｸ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ驕ｯ・ｶ繝ｻ・ｻBox/Model鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬯ｯ・ｩ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｩ鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｨ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ郢晢ｽｻ
		if (!skeleton.joints.empty()) {
			myBox->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
			myBox->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
			myBox->SetScale(skeleton.joints[skeleton.root].transform.scale);

			// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｭ鬩幢ｽ｢隴乗・・ｽ・ｹ隴∵ｻゑｽｽ・ｦ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ髫ｰ逍ｲ・ｻ繧托ｽｽ・ｽ繝ｻ・ｮ髮九・・ｽ・ｯ郢晢ｽｻ繝ｻ・｣驛｢譎｢・ｽ・ｻ鬩幢ｽ｢繝ｻ・ｧ髯溷供・ｨ・ｯ髯橸ｽｺ鬩搾ｽｵ繝ｻ・ｺ髮九・竏槭・・ｽ遶擾ｽｫ繝ｻ・ｸ繝ｻ・ｲ驕ｶ荳橸ｽ｣・ｹ隨ｳ遏ｩﾎ斐・・ｧ郢晢ｽｻ繝ｻ・ｭ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｪ鬩搾ｽｵ繝ｻ・ｺ髫ｴ貅ｷ髯ｸdel鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ諛ｶ・ｽ・｣郢晢ｽｻ繝ｻ・ｴ鬮ｯ・ｷ繝ｻ・ｷ驛｢譎｢・ｽ・ｻ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｿTransform鬩幢ｽ｢繝ｻ・ｧ髯晢ｽｶ隴擾ｽｶ郢晢ｽｻ鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｨ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ郢晢ｽｻ
			// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｭ鬩幢ｽ｢隴乗・・ｽ・ｹ隴∵ｻゑｽｽ・ｦ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ髫ｰ逍ｲ・ｻ繧托ｽｽ・ｽ繝ｻ・ｮ髮九・・ｽ・ｯ郢晢ｽｻ繝ｻ・｣驛｢譎｢・ｽ・ｻ鬩幢ｽ｢繝ｻ・ｧ髯溷供・ｨ・ｯ髯橸ｽｺ鬩搾ｽｵ繝ｻ・ｺ髮九・竏槭・・ｽ遶擾ｽｫ繝ｻ・ｸ繝ｻ・ｲ驕ｶ荳橸ｽ｣・ｹ隨ｳ遏ｩﾎ斐・・ｧ郢晢ｽｻ繝ｻ・ｭ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｪ鬩搾ｽｵ繝ｻ・ｺ髫ｴ貅ｷ髯ｸdel鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ諛ｶ・ｽ・｣郢晢ｽｻ繝ｻ・ｴ鬮ｯ・ｷ繝ｻ・ｷ驛｢譎｢・ｽ・ｻ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｿTransform鬩幢ｽ｢繝ｻ・ｧ髯晢ｽｶ隴擾ｽｶ郢晢ｽｻ鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｨ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ郢晢ｽｻ
			if (myModelObject->GetModel()) {
				if (!myModelObject->skinCluster.isValid) {
					myModelObject->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
					myModelObject->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
					myModelObject->SetScale(skeleton.joints[skeleton.root].transform.scale);
				} else {
					// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｭ鬩幢ｽ｢隴乗・・ｽ・ｹ隴∵ｻゑｽｽ・ｦ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｰModel鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・｢鬩幢ｽ｢隴乗・・ｽ・ｹ隴∵ｻ・ｱｪ繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｧ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ髴托ｽｹ陞滂ｽｲ繝ｻ・ｽ繝ｻ・｡鬩包ｽｯ繝ｻ・ｪ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬮ｯ・ｷ繝ｻ・ｷ郢晢ｽｻ繝ｻ・ｫ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｾ鬩幢ｽ｢繝ｻ・ｧ髯滓坩・ｯ莨夲ｽｽ・ｽ霑｢證ｦ・ｽ・ｸ繝ｻ・ｺ髮九・竏槭・・ｽ遶擾ｽｫ繝ｻ・ｸ繝ｻ・ｲ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬩幢ｽ｢隴主・讓溘・荳ｻ・ｸ・ｷ繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢隴弱・・ｽ・ｼ隴・搨・ｰ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・｣繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｻ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ郢晢ｽｻ
					// (鬩搾ｽｵ繝ｻ・ｺ鬮ｦ・ｮ陷ｻ・ｻ繝ｻ・ｽ隶呵ｶ｣・ｽ・ｹ繝ｻ・ｧ髯橸ｽｳ陞滂ｽｲ繝ｻ・ｽ繝ｻ・｡髯滓坩・ｯ莨夲ｽｽ・ｽ陷證ｦ・ｽ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｪ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ鬮｣雋ｻ・｣・ｰ鬩募●繝ｻ髯ｬ貊・＠繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬯ｩ蜍溪・繝ｻ・ｽ繝ｻ・ｻ鬮ｯ・ｷ陝雜｣・ｽ・ｼ髮具ｽｻ繝ｻ・ｼ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｦ鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｻ鬯ｯ・ｮ繝ｻ・ｱ郢晢ｽｻ繝ｻ・｢鬮ｯ讓奇ｽｻ阮卍ｧ驕ｶ鬆托ｽ･・｢繝ｻ・ｱ繝ｻ・ｸ髯具ｽｹ繝ｻ・ｻ驕ｶ謫ｾ・ｽ・ｴ鬩幢ｽ｢繝ｻ・ｧ驛｢譎｢・ｽ・ｻ
					myModelObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
					myModelObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
					myModelObject->SetScale({ modelScale, modelScale, modelScale });
				}
			}
		}

		// 鬯ｯ・ｯ繝ｻ・ｪ郢晢ｽｻ繝ｻ・ｨ鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ
		bool isAnimationEditor = IsSimulationMode() && uiManager_ && uiManager_->currentSimulationTarget_ == 5;
		if ((showBones || isAnimationEditor) && player_) {
			std::vector<VertexData> lineVertices;

			const Skeleton& playerSkeleton = player_->GetSkeleton();
			for (size_t i = 0; i < playerSkeleton.joints.size(); ++i) {
				Vector3 pos = {
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][0],
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][1],
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][2]
				};

				// 鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｨ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｯ・ｯ郢晢ｽｻ繝ｻ閾･・ｸ・ｺ陝ｶ・ｷ繝ｻ・ｹ繝ｻ・ｧ髯ｷ莉｣繝ｻ繝ｻ・ｽ繝ｻ・ｽ髯晄慣・ｽ・｢E鬯ｮ・ｫ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｪ鬩搾ｽｵ繝ｻ・ｺ髯滓坩・ｯ莨夲ｽｽ・ｼ隶捺慣・ｽ・ｹ繝ｻ・ｧ髯ｷ・ｿ繝ｻ・･郢晢ｽｻ繝ｻ・ｰ郢晢ｽｻ繝ｻ・ｴ鬮ｯ・ｷ繝ｻ・ｷ鬮｣魃会ｽｽ・ｨ郢晢ｽｻ繝ｻ・ｼ驛｢譎｢・ｽ・ｻ
				if (playerSkeleton.joints[i].parent) {
					int32_t parentIndex = *playerSkeleton.joints[i].parent;
					Vector3 parentPos = {
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][0],
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][1],
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][2]
					};

					VertexData v1, v2;
					v1.position = { parentPos.x, parentPos.y, parentPos.z, 1.0f };
					v1.normal = { 0.0f, 1.0f, 0.0f };
					v1.texcoord = { 0.0f, 0.0f };

					v2.position = { pos.x, pos.y, pos.z, 1.0f };
					v2.normal = { 0.0f, 1.0f, 0.0f };
					v2.texcoord = { 1.0f, 1.0f };

					Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 鬯ｨ・ｾ陷茨ｽｷ繝ｻ・ｽ繝ｻ・ｽ髮趣ｽｼ繝ｻ・ｶ郢晢ｽｻ繝ｻ・ｲ
					if (simulationManager_ && simulationManager_->IsBoneSelected(playerSkeleton.joints[i].name)) {
						v1.color = { 1.0f, 1.0f, 0.0f, 1.0f };
						v2.color = { 1.0f, 1.0f, 0.0f, 1.0f };
					} else {
						v1.color = color;
						v2.color = color;
					}

					lineVertices.push_back(v1);
					lineVertices.push_back(v2);
				}
			}

			// 鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳModel驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｯ・ｯ郢晢ｽｻ繝ｻ閾･・ｸ・ｺ陝ｶ・ｷ繝ｻ・ｹ繝ｻ・ｧ髯ｷ・ｻ闔・･繝ｻ・ｳ繝ｻ・ｩ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ
			// 鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳModel鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｯ・ｯ郢晢ｽｻ繝ｻ閾･・ｸ・ｺ陝ｶ・ｷ繝ｻ・ｹ繝ｻ・ｧ髯ｷ・ｻ闔・･繝ｻ・ｳ繝ｻ・ｩ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ
			if (player_ && player_->GetObject3d()) {
				skeletonLinesObject->SetTranslate(player_->GetPosition());
				skeletonLinesObject->SetQuaternionRotate(player_->GetQuaternion());
				skeletonLinesObject->SetScale(player_->GetObject3d()->GetScale());
			}
			if (!lineVertices.empty() && skeletonLinesObject->GetModel()) {
				skeletonLinesObject->GetModel()->UpdateLineVertices(lineVertices);
			}
			skeletonLinesObject->Update();
		}
	}

	// Model驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ
	if (showModel && myModelObject) {
		myModelObject->Update();
	}

	// Debug camera switching is handled by ImGui buttons in UpdateUI().
	if (false && Input::GetInstance()->TriggerKey(DIK_F1)) {
		SetDebugCameraActive(!isDebugCameraActive_);
	}

	// 鬩幢ｽ｢隴惹ｸ橸ｽｹ・ｲ繝ｻ蜿厄ｽｨ謚ｵ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｩ蜍溪・繝ｻ・ｽ繝ｻ・ｻ鬮ｯ・ｷ陝雜｣・ｽ・ｼ隰夲ｽｫ郢晢ｽｻ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・｡鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ鬩幢ｽ｢繝ｻ・ｧ髯具ｽｹ繝ｻ・ｻ郢晢ｽｻ鬯倅ｿｶﾂ・ｦ髯具ｽｹ繝ｻ・ｻ驕ｶ莨∬ｱｪ繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｭ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驍ｵ・ｺ鬩｢謳ｾ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬮ｴ謇假ｽｽ・･郢晢ｽｻ繝ｻ・ｶ鬮ｫ・ｲ繝ｻ・ｷ髣包ｽｵ隴趣ｽ｢繝ｻ・ｽ陝ｶ譎｢・ｿ・｡郢晢ｽｻ繝ｻ・ｺ鬮ｯ讖ｸ・ｽ・ｳ髯橸ｽ｢繝ｻ・ｹ髫ｨ蛟･繝ｻ繝ｻ・ｹ繝ｻ・ｧ髣包ｽｵ隰ｨ魑ｴﾂ驛｢譎｢・ｽ・ｻ
	// 鬩搾ｽｵ繝ｻ・ｺ鬮ｦ・ｮ陷ｻ・ｻ繝ｻ・ｽ隶呵ｶ｣・ｽ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢繝ｻ・ｧ髯具ｽｹ繝ｻ・ｻ郢晢ｽｻ鬯倩ｲｻ・ｽ・ｸ繝ｻ・ｲ驕ｶ荳橸ｽ｢繝ｻ・ｺ・ｽ繝ｻ・ｹ隴擾ｽｴ郢晢ｽｻ驍ｵ・ｺ鬩｢謳ｾ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ髫ｨ・ｳ郢晢ｽｻ繝ｻ・ｹ隴弱・・ｽ・ｼ鬩･繝ｻ・ｨ謚ｵ・ｽ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・｣繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ髣包ｽｵ隴趣ｽ｢繝ｻ・ｽ髣・ｽｽ繝ｻ・ｮ陋ｹ繝ｻ・ｽ・ｻ闔ｨ螟ｲ・ｽ・ｽ繝ｻ・ｽ鬮ｦ・ｮ陷ｷ・ｶ郢晢ｽｻ鬯ｮ・ｴ隰・∞・ｽ・ｽ繝ｻ・ｽ鬮ｯ貅ｷ・｢骰玖｢夜劑ﾂ繝ｻ・ｿ鬮ｯ・ｷ繝ｻ・ｷ髣比ｼ夲ｽｽ・｣驕ｶ髮・｣ｰ・､繝ｻ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・｡鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｹ鬮ｯ・ｷ繝ｻ・ｷ髣比ｼ夲ｽｽ・｣驕ｯ・ｶ繝ｻ・ｲ鬮｣蛹・ｽｽ・ｳ繝ｻ縺､ﾂ鬯ｮ・｢繝ｻ・ｾ郢晢ｽｻ繝ｻ・ｴ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ霑｢證ｦ・ｽ・ｸ繝ｻ・ｲ驛｢譎｢・ｽ・ｻ
	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	lockOnManager_->UpdateLockOn(activeCamera, allowLockOnBehavior);

	// 鬩幢ｽ｢隴惹ｸ橸ｽｹ・ｲ繝ｻ蜿厄ｽｨ謚ｵ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｨ鬩搾ｽｵ繝ｻ・ｲ驕ｶ荳橸ｽ｣・ｹ遯ｶ・ｳ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・｡鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｮ・ｴ隰・∞・ｽ・ｽ繝ｻ・ｽ鬮ｯ貅ｯ・ｼ譁舌・
	if (player_) {
		if (updateSelectedPlayer) {
			Vector3 lockOnTargetPosition;
			const Vector3 *lockOnTarget = nullptr;
			if (lockedEnemy_) {
				lockOnTargetPosition = lockedEnemy_->GetPosition();
				lockOnTarget = &lockOnTargetPosition;
			}
			player_->Update(obstacles_, lockOnTarget);
		} else {
			player_->UpdateModel();
		}

	}

	// ==========================================
	// 鬮ｫ・ｰ繝ｻ・ｨ郢晢ｽｻ繝ｻ・ｵ
	// ==========================================
	// 鬩幢ｽ｢隴惹ｸ橸ｽｹ・ｲ繝ｻ蜿厄ｽｨ謚ｵ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｴ陝・｢・つ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ鬮ｯ貅ｯ・ｶ・｣繝ｻ・ｽ繝ｻ・ｧ鬮ｫ・ｶ霓｣蛟｡蜃ｽ郢晢ｽｻ陞ｳ螢ｽ笊るｬｮ・｢・つ郢晢ｽｻ繝ｻ・ｾ髯ｷ莨夲ｽｽ・ｱ髫ｨ蛟･繝ｻ繝ｻ・ｹ繝ｻ・ｧ驛｢譎｢・ｽ・ｻ
	Vector3 playerPos = player_ ? player_->GetOBB().center : Vector3{ 0.0f, 0.0f, 0.0f };

	if (updateSelectedEnemies) {
		// 鬮ｫ・ｰ繝ｻ・ｨ郢晢ｽｻ繝ｻ・ｵ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ貊捺ｱ壹・・ｽ繝ｻ・ｾ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬯ｮ・ｯ繝ｻ・ｲ郢晢ｽｻ繝ｻ・ｫ鬮ｯ貊捺ｱ壹・・ｽ繝ｻ・ｾ鬮ｫ・ｴ陟托ｽｱ繝ｻ莉｣繝ｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬮ｴ雜｣・ｽ・ｷ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬮ｯ貅ｯ・ｶ・｣繝ｻ・ｽ繝ｻ・ｧ鬮ｫ・ｶ霓｣蛟｡蜃ｽ郢晢ｽｻ陞ｳ螢ｽ笊る匚莨夲ｽｽ・ｱ郢晢ｽｻ繝ｻ・ｰ鬮ｯ・ｷ繝ｻ・ｿ髫ｰ雋ｻ・ｽ・ｶ郢晢ｽｻ闕ｵ譏ｴ繝ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE
		std::vector<Vector3> enemyBulletHits;
		if (enemyBulletManager_ && player_) {
			enemyBulletManager_->Update(player_.get(), enemyBulletHits, obstacles_);
		}

		// 鬮ｫ・ｰ繝ｻ・ｨ郢晢ｽｻ繝ｻ・ｵ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ貊捺ｱ壹・・ｽ繝ｻ・ｾ鬩搾ｽｵ繝ｻ・ｺ鬯ｲ繝ｻ・ｼ螟ｲ・ｽ・ｽ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｬ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬮ｯ貅ｷ繝ｻ關難ｽｭ髫ｨ・ｳ郢晢ｽｻ繝ｻ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・｣鬩搾ｽｵ繝ｻ・ｺ髮九・・ｽ・ｷ郢晢ｽｻ繝ｻ・ｰ郢晢ｽｻ繝ｻ・ｴ鬮ｯ・ｷ繝ｻ・ｷ髯具ｽｹ繝ｻ・ｻ郢晢ｽｻ郢ｧ螂・ｽｽ・ｾ繝ｻ・ｷ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ髯懶ｽ｣繝ｻ・､髯具ｽｹ繝ｻ・ｱ鬯ｨ・ｾ陟・屮・ｽ・ｺ陋帙・・ｽ・ｼ郢晢ｽｻ繝ｻ・ｸ繝ｻ・ｺ髯晢ｽｶ陷ｻ・ｻ繝ｻ・ｽ郢晢ｽｻ
		if (explosionManager_ && !enemyBulletHits.empty()) {
			explosionManager_->CreateHitEffects(enemyBulletHits);
		}

		for (auto it = enemies_.begin(); it != enemies_.end(); ) {
			// 近接攻撃の当たり判定
			if (player_ && player_->IsMeleeAttacking()) {
				OBB meleeHitbox = player_->GetMeleeHitbox();
				Sphere enemySphere;
				enemySphere.center = (*it)->GetPosition();
				enemySphere.radius = (*it)->GetCollisionRadius();
				if (MyMath::IsCollision(enemySphere, meleeHitbox)) {
					(*it)->TakeDamage(player_->GetMeleeDamage());
				}
			}

			// 地上敵の近接攻撃当たり判定
			GroundEnemy* groundEnemy = dynamic_cast<GroundEnemy*>(it->get());
			if (groundEnemy && groundEnemy->IsMeleeActive() && player_ && !player_->IsDead()) {
				OBB meleeOBB = groundEnemy->GetMeleeBoxOBB();
				OBB playerOBB = player_->GetOBB();
				if (MyMath::IsCollision(meleeOBB, playerOBB)) {
					player_->TakeDamage(1);
				}
			}

			(*it)->Update(playerPos, enemyBulletManager_.get(), obstacles_);
			if (Boss *boss = dynamic_cast<Boss *>(it->get())) {
				const int summonCount = boss->ConsumeSummonRequests();
				for (int i = 0; i < summonCount; ++i) {
					auto escort = std::make_unique<Enemy>();
					const float side = static_cast<float>(i - summonCount / 2) * 8.0f;
					escort->Initialize({ boss->GetPosition().x + side, boss->GetPosition().y - 3.0f, boss->GetPosition().z - 12.0f });
					escort->StartChasingPlayer();
					enemies_.push_back(std::move(escort));
				}
			}
			if ((*it)->IsDead()) {
				const bool defeatedBoss = (*it)->IsBoss();
				const Vector3 defeatedPosition = (*it)->GetPosition();
				if (!defeatedBoss) {
					++defeatedSmallEnemyCount_;
					if (defeatedSmallEnemyCount_ % kKillsPerAmmoPickup == 0) {
						SpawnAmmoPickup(defeatedPosition);
					}
				}
				if (lockedEnemy_ == it->get()) {
					lockedEnemy_ = nullptr;
					isCinematicLockOnCameraInitialized_ = false;
				}
				if (aimAssistEnemy_ == it->get()) {
					aimAssistEnemy_ = nullptr;
				}
				if (missileManager_) {
// 					missileManager_->ClearTarget(it->get());
				}
				size_t spawnPointIndex = (*it)->GetSpawnPointIndex();
				
				if (spawnPointIndex < enemySpawns_.size()) {
					const std::string& deadName = enemySpawns_[spawnPointIndex].name;
					TriggerEnemyReinforcements(deadName);
				}

				if (spawnPointIndex < enemySpawns_.size() && enemySpawns_[spawnPointIndex].isInitialSpawn) {
// 					ScheduleEnemySpawn(spawnPointIndex, kEnemyRespawnDelayFrames);
				}
				songGauge_ = (std::min)(songGauge_ + 20.0f, 100.0f);
				it = enemies_.erase(it); // 鬮ｯ貅ｷ繝ｻ關難ｽｭ髫ｨ・ｳ郢晢ｽｻ繝ｻ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・｣鬩搾ｽｵ繝ｻ・ｺ髮倶ｼ・ｽｦ・ｴ陝ｲ繝ｻ縺励・・ｺ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢隴主・讓溘・繧托ｽｽ・ｰ鬩幢ｽ｢繝ｻ・ｧ鬨ｾ蛹・ｽｽ・ｻ郢晢ｽｻ繝ｻ・ｶ髣費｣ｰ繝ｻ・･髫ｰ遒第ｭ薙・・ｲ驗呻ｽｫ郢晢ｽｻ

				// 鬩幢ｽ｢隴弱・魃ｵ驍ｵ・ｺ陝ｶ・ｷ繝ｻ・ｸ繝ｻ・ｺ髫ｰ逍ｲ・ｺ・ｷ繝ｻ・ｰ陷托ｽｰ隰ｫ螟頑､ｶ繝ｻ・ｹ郢晢ｽｻ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ髮狗ｿｫ繝ｻ繝ｻ・ｰ郢晢ｽｻ繝ｻ・ｬ繝ｻ・ｲ髯橸ｽ｢繝ｻ・ｽ鬯ｮ・ｮ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ髫ｴ・ｴ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｮ髣包ｽｵ隴擾ｽｶ陞滂ｽ｢鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｦ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ驕ｯ・ｶ繝ｻ・ｻ鬩幢ｽ｢繝ｻ・ｧ驛｢・ｧ郢晢ｽｻ・つ驕ｶ荳橸ｽ｣・ｹ郢晢ｽｻ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬮ｫ・ｴ陝ｷ・｢繝ｻ・ｽ繝ｻ・ｬ鬮｣蜴・ｽｽ・ｴ鬮ｦ・ｮ陷ｻ・ｻ繝ｻ・ｽ陞ｳ螢ｼ・ｱ蜊螻√・・ｵ郢晢ｽｻ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ髮矩｡板ｧ郢晢ｽｻ鬮ｴ髮｣・ｽ・､郢晢ｽｻ繝ｻ・ｹ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・｢鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ霑｢證ｦ・ｽ・ｸ繝ｻ・ｲ驛｢譎｢・ｽ・ｻ
				if (defeatedBoss && !IsSimulationMode() && !isGameOver_) {
					SceneManager::GetInstance()->ChangeScene("CLEAR");
					return;
				}
			} else {
				++it;
			}
		}
		UpdateAmmoPickups();
		UpdateEnemyRespawns();

		if (!IsSimulationMode() && !isGameOver_ && enemies_.empty() && !HasPendingEnemySpawns()) {
			if (!bossSpawned_) {
				OutputDebugStringA("[GamePlayScene] All enemies defeated! Spawning Boss...\n");
				auto boss = std::make_unique<Boss>();
				boss->Initialize({ playerPos.x, playerPos.y + 18.0f, playerPos.z + 90.0f });
				enemies_.push_back(std::move(boss));
				bossSpawned_ = true;
			} else {
				OutputDebugStringA("[GamePlayScene] Boss defeated! Changing scene to CLEAR.\n");
				SceneManager::GetInstance()->ChangeScene("CLEAR");
				return;
			}
		}

		// 鬯ｯ・ｮ繝ｻ・ｫ髫ｲ蟷｢・ｽ・ｷ郢晢ｽｻ繝ｻ・ｮ郢晢ｽｻ繝ｻ・ｳ鬮ｴ螟ｧ・､・ｲ繝ｻ・ｽ繝ｻ・ｩ鬯ｮ・｢繝ｻ・ｾ郢晢ｽｻ繝ｻ・ｪ鬯ｮ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｫ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮUpdate鬩幢ｽ｢繝ｻ・ｧ鬮ｮ蛹ｺ・ｧ・ｫ繝ｻ・ｱ鬪ｰ蜈ｷ・ｽ・ｸ繝ｻ・ｺ髯ｷ・ｻ繝ｻ・ｻ郢晢ｽｻ繝ｻ・ｼ鬮｢・ｧ繝ｻ・ｲ髫ｶ謐ｺ・ｺ・ｯ繝ｻ・ｿ繝ｻ・･郢晢ｽｻ繝ｻ・ｶ鬮｣蛹・ｽｽ・ｳ郢晢ｽｻ繝ｻ・ｭ鬯ｮ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｫ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｯ鬯ｩ蛹・ｽｽ・ｨ郢晢ｽｻ繝ｻ・ｺ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬯ｮ・ｴ陷ｿ・ｰ繝ｻ・ｻ繝ｻ・｣郢晢ｽｻ隶捺慣・ｽ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ驕ｯ・ｶ繝ｻ・ｲ鬮｣蛹・ｽｽ・ｳ繝ｻ縺､ﾂ鬮ｯ貊ゑｽｽ・｢髫ｲ蟷｢・ｽ・ｷ髯橸ｽｻ鬪ｰ蜈ｷ・ｽ・ｸ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ驕ｶ謫ｾ・ｽ・ｪ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｻ繝ｻ・ｻ郢晢ｽｻ繝ｻ・ｼ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｼ驛｢譎｢・ｽ・ｻ
		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	} else {
		for (auto &enemy : enemies_) {
			enemy->UpdateModel();
		}
	}

	// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・｡鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ
	if (isDebugCameraActive_) {
		debugFlyCamera_->SetCanUseKeyboard(canUseKeyboardInput);
		debugFlyCamera_->Update(); // FlyCamera鬩搾ｽｵ繝ｻ・ｺ髫ｰ逍ｲ・ｺ蛟･繝ｻ鬯ｯ・ｩ陝ｷ・｢繝ｻ・ｽ繝ｻ・ｨ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬩幢ｽ｢隴弱・・ｽ・ｧ繝ｻ・ｭ驍ｵ・ｺ髢ｧ・ｲ繝ｻ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢隴取得・ｽ・ｸ陷ｷ・ｶ・趣ｽ｣鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬮ｯ蜈ｷ・ｽ・ｻ郢晢ｽｻ繝ｻ・､鬮ｯ讖ｸ・ｽ・ｳ髯橸ｽ｢繝ｻ・ｹ郢晢ｽｻ陝ｶ譎剰ｷ晞辧蜍滂ｽｨ・ｯ陞滂ｽ｢鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｦ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ郢晢ｽｻ
		
		if (isAnimationEditor && simulationManager_) {
			simulationManager_->UpdateShortcuts();
			ImGuiIO& io = ImGui::GetIO();
			Vector2 localMousePos;
			
			static bool s_clickedOnBone = false;
			static bool s_isDraggingBone = false;
			static ImVec2 s_mouseDownPos = {0, 0};
			
			if (FlyCamera::GetGameViewMousePos(io.MousePos.x, io.MousePos.y, localMousePos)) {
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					float gw, gh;
					FlyCamera::GetGameViewSize(gw, gh);
					Ray ray = MyMath::ScreenToRay(localMousePos, gw, gh, MyMath::Inverse(debugFlyCamera_->GetViewProjectionMatrix()));
					
					float closestDist = 999999.0f;
					std::string closestBone = "";
					
					if (this->player_) {
						const Skeleton& playerSkeleton = this->player_->GetSkeleton();
						for (const auto& joint : playerSkeleton.joints) {
							Vector3 pos = {
								joint.skeletonSpaceMatrix.m[3][0],
								joint.skeletonSpaceMatrix.m[3][1],
								joint.skeletonSpaceMatrix.m[3][2]
							};
						
							Vector3 currentScale = {1.0f, 1.0f, 1.0f};
							if (this->player_ && this->player_->GetObject3d()) {
								currentScale = this->player_->GetObject3d()->GetScale();
								Matrix4x4 worldMat = MyMath::MakeAffineMatrix(currentScale, this->player_->GetQuaternion(), this->player_->GetPosition());
								pos = MyMath::Transform(pos, worldMat);
							}

							Sphere s;
							s.center = pos;
							s.radius = 0.5f * currentScale.x; // 鬮ｯ蜈ｷ・ｽ・ｻ郢晢ｽｻ繝ｻ・､鬮ｯ讖ｸ・ｽ・ｳ髯橸ｽ｢繝ｻ・ｼ髮趣ｽｼ繝ｻ・ｰ鬮ｯ貅ｯ・ｼ譁舌・郢晢ｽｻ陞ｳ螢ｽ・ｰ・｣髣比ｼ夲ｽｽ・｣郢晢ｽｻ繝ｻ・ｰ鬮ｯ譏ｴ繝ｻ繝ｻ・ｸ陞ゅ・・ｽ・ｼ郢晢ｽｻ繝ｻ・ｹ繝ｻ・ｧ驕ｶ荳橸ｽ｣・ｺ郢晢ｽｻ鬯ｮ・ｫ繝ｻ・ｱ郢晢ｽｻ繝ｻ・ｿ鬮ｫ・ｰ繝ｻ・ｨ郢晢ｽｻ繝ｻ・ｴ
						
						float dist;
						if (MyMath::IntersectRaySphere(ray, s, &dist)) {
							if (dist < closestDist) {
								closestDist = dist;
								closestBone = joint.name;
							}
						}
					}
					}
					
					s_clickedOnBone = !closestBone.empty();
					s_mouseDownPos = io.MousePos;

					if (s_clickedOnBone) {
						if (io.KeyShift) {
							if (simulationManager_->IsBoneSelected(closestBone)) {
								simulationManager_->RemoveSelectedBoneName(closestBone);
							} else {
								simulationManager_->AddSelectedBoneName(closestBone);
							}
						} else {
							// 鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ驍ｵ・ｲ陜｣・､繝ｻ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬯ｯ・ｩ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｸ鬮ｫ・ｰ陞｢・ｽ繝ｻ・ｨ陞ゅ・・ｽ・ｽ繝ｻ・ｸ髯具ｽｹ繝ｻ・ｻ驕ｶ謫ｾ・ｽ・ｩ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬩幢ｽ｢隴弱・魃ｵ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ髯句ｹ｢・ｽ・ｵ驍ｵ・ｺ鬩｢謳ｾ・ｽ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驍ｵ・ｺ鬩｢謳ｾ・ｽ・ｸ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ髫ｨ・ｳ郢晢ｽｻ隰ｦ・ｻ郢晢ｽｻ繝ｻ・ｴ鬮ｯ・ｷ繝ｻ・ｷ髯具ｽｹ繝ｻ・ｻ驛｢譎｢・ｽ・ｻ鬩搾ｽｵ繝ｻ・ｲ驕ｶ謫ｾ・ｽ・ｬ郢晢ｽｻ繝ｻ・､驛｢譎｢・ｽ・ｻ髴取ｺｷ・､謦ｰ・ｽ・ｩ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｸ鬮ｫ・ｰ陞｢・ｽ繝ｻ・ｧ繝ｻ・ｭ郢晢ｽｻ陝ｶ譏ｴ・郢晢ｽｻ繝ｻ・ｭ鬮ｫ・ｰ陜荳翫・郢晢ｽｻ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｦ鬩幢ｽ｢隴取得・ｽ・ｳ繝ｻ・ｨ繝ｻ荳ｻ・ｸ・ｷ繝ｻ・ｹ隴擾ｽｴ郢晢ｽｻ驍ｵ・ｺ陜｣・､繝ｻ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬩搾ｽｵ繝ｻ・ｺ鬯ｮ・ｦ繝ｻ・ｪ郢晢ｽｻ霑｢證ｦ・ｽ・ｹ繝ｻ・ｧ髯具ｽｹ繝ｻ・ｻ驕ｶ蛹・ｽｽ・ｧ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ郢晢ｽｻ
							if (!simulationManager_->IsBoneSelected(closestBone)) {
								simulationManager_->ClearSelectedBones();
								simulationManager_->AddSelectedBoneName(closestBone);
							}
						}
						isBoxSelecting_ = false;
						s_isDraggingBone = true;
					} else {
						// 鬮｣蜴・ｽｽ・ｴ鬮ｴ驛・ｽｲ・ｻ繝ｻ・ｽ郢ｧ莨夲ｽｽ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｪ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｩ郢晢ｽｻ繝ｻ・ｺ鬯ｯ・ｮ繝ｻ・｢鬮ｦ・ｮ陷ｻ・ｻ繝ｻ・ｽ陜｣・､繝ｻ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驍ｵ・ｺ鬩｢謳ｾ・ｽ・ｸ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ髫ｨ・ｳ郢晢ｽｻ繝ｻ・ｭ陟托ｽｱ郢晢ｽｻ
						if (io.KeyShift || simulationManager_->GetSelectedBoneNames().empty()) {
							// Shift鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｭ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ髯ｷ・ｻ陜捺ｻゑｽｽ・ｬ繝ｻ・ｾ鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ驕ｯ・ｶ繝ｻ・ｻ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ郢晢ｽｻ霑｢證ｦ・ｽ・ｸ繝ｻ・ｺ髣包ｽｵ隰ｨ魑ｴﾂ驕ｶ謫ｾ・ｽ・ｽ郢晢ｽｻ繝ｻ・ｽ鬮ｴ驛・ｽｲ・ｻ繝ｻ・ｽ郢ｧ蜈ｷ・ｽ・ｩ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｸ鬮ｫ・ｰ陞｢・ｽ繝ｻ・ｧ繝ｻ・ｭ郢晢ｽｻ郢晢ｽｻ繝ｻ・ｹ繝ｻ・ｧ髯溷供・ｨ・ｯ・つ繝ｻ・ｻ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ驕ｶ莨√・繝ｻ・ｸ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｰ郢晢ｽｻ繝ｻ・ｴ鬮ｯ・ｷ繝ｻ・ｷ髯具ｽｹ繝ｻ・ｻ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢隴弱・魃ｵ驛｢譎｢・ｽ・｣鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬯ｯ・ｩ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｸ鬮ｫ・ｰ陞｢・ｽ繝ｻ・ｧ繝ｻ・ｭ郢晢ｽｻ陝ｶ譎｢・ｽ・ｫ繝ｻ・｢髯ｷ・ｿ繝ｻ・･郢晢ｽｻ繝ｻ・ｧ驛｢譎｢・ｽ・ｻ
							isBoxSelecting_ = true;
							s_isDraggingBone = false;
							boxSelectStartPos_ = localMousePos;
							boxSelectEndPos_ = localMousePos;
						} else {
							// 鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ驍ｵ・ｲ陜｣・､繝ｻ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢隴弱・魃ｵ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ鬩募●豐也ｹ晢ｽｻ鬮ｫ・ｰ陞｢・ｽ繝ｻ・ｧ繝ｻ・ｭ郢晢ｽｻ郢晢ｽｻ繝ｻ・ｹ繝ｻ・ｧ髯溷供・ｨ・ｯ・つ繝ｻ・ｻ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ郢晢ｽｻ驍・戟謐礼ｹ晢ｽｻ繝ｻ・ｴ鬮ｯ・ｷ繝ｻ・ｷ髯具ｽｹ繝ｻ・ｻ繝ｻ縺､ﾂ驕ｶ荳橸ｽ｣・ｹ・主ｹπ碑ｭ趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驍ｵ・ｺ陜｣・､繝ｻ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬮ｯ諛・ｻｸ繝ｻ・ｫ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｻ郢晢ｽｻ繝ｻ・｢鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｧ鬩搾ｽｵ繝ｻ・ｺ鬯ｮ・ｦ繝ｻ・ｪ郢晢ｽｻ霑｢證ｦ・ｽ・ｹ繝ｻ・ｧ髯具ｽｹ繝ｻ・ｻ驕ｶ蛹・ｽｽ・ｧ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬮ｯ貅ｯ・ｼ譁舌・郢晢ｽｻ繝ｻ・ｩ驛｢譎｢・ｽ・ｻ
							isBoxSelecting_ = false;
							s_isDraggingBone = true;
						}
					}
				}
				
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
					const auto& selBones = simulationManager_->GetSelectedBoneNames();
					float dx = io.MouseDelta.x;
					float dy = io.MouseDelta.y;
					
					if (isBoxSelecting_) {
						Vector2 localMousePos;
						if (FlyCamera::GetGameViewMousePos(io.MousePos.x, io.MousePos.y, localMousePos)) {
							boxSelectEndPos_ = localMousePos;
						}
					} else if (s_isDraggingBone && !selBones.empty()) {
						if (dx != 0.0f || dy != 0.0f) {
							bool doTranslate = io.KeyCtrl;
							for (const auto& boneName : selBones) {
								if (doTranslate) {
									simulationManager_->AddBoneTranslationFromDrag(boneName, dx, dy);
								} else {
									simulationManager_->AddBoneRotationFromDrag(boneName, dx, dy);
								}
							}
						}
					}
				}

				if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
					if (isBoxSelecting_) {
						isBoxSelecting_ = false;
						
						float minX = (std::min)(boxSelectStartPos_.x, boxSelectEndPos_.x);
						float maxX = (std::max)(boxSelectStartPos_.x, boxSelectEndPos_.x);
						float minY = (std::min)(boxSelectStartPos_.y, boxSelectEndPos_.y);
						float maxY = (std::max)(boxSelectStartPos_.y, boxSelectEndPos_.y);
						
						if (maxX - minX > 2.0f && maxY - minY > 2.0f) {
							if (this->player_) {
								float gw, gh;
								FlyCamera::GetGameViewSize(gw, gh);
								Matrix4x4 vpMat = debugFlyCamera_->GetViewProjectionMatrix();
								const Skeleton& playerSkeleton = this->player_->GetSkeleton();
								
								for (const auto& joint : playerSkeleton.joints) {
									Vector3 pos = {
										joint.skeletonSpaceMatrix.m[3][0],
										joint.skeletonSpaceMatrix.m[3][1],
										joint.skeletonSpaceMatrix.m[3][2]
									};
									
									if (this->player_->GetObject3d()) {
										Vector3 currentScale = this->player_->GetObject3d()->GetScale();
										Matrix4x4 worldMat = MyMath::MakeAffineMatrix(currentScale, this->player_->GetQuaternion(), this->player_->GetPosition());
										pos = MyMath::Transform(pos, worldMat);
									}
									
									Vector3 screenPos = MyMath::WorldToScreen(pos, vpMat, gw, gh);
									
									if (screenPos.z > 0.0f && screenPos.z < 1.0f) {
										if (screenPos.x >= minX && screenPos.x <= maxX &&
											screenPos.y >= minY && screenPos.y <= maxY) {
											simulationManager_->AddSelectedBoneName(joint.name);
										}
									}
								}
							}
						}
					} else {
						// 鬩幢ｽ｢隴取得・ｽ・ｳ繝ｻ・ｨ繝ｻ荳ｻ・ｸ・ｷ繝ｻ・ｹ隴擾ｽｴ郢晢ｽｻ驍ｵ・ｺ陜｣・､繝ｻ・ｸ繝ｻ・ｺ髯晢ｽｶ陷ｷ・ｮ郢晢ｽｻ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驍ｵ・ｺ鬩｢謳ｾ・ｽ・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｰ鬩搾ｽｵ繝ｻ・ｺ髣比ｼ夲ｽｽ・｣驍ｵ・ｲ陝ｶ譎｢・ｽ・ｫ繝ｻ・ｮ郢晢ｽｻ繝ｻ・｢鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ髫ｨ・ｳ郢晢ｽｻ隰ｦ・ｻ郢晢ｽｻ繝ｻ・ｴ鬮ｯ・ｷ繝ｻ・ｷ髯具ｽｹ繝ｻ・ｻ驛｢譎｢・ｽ・ｻ鬯ｯ・ｩ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｸ鬮ｫ・ｰ陞｢・ｽ繝ｻ・ｫ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｧ郢晢ｽｻ繝ｻ・｣鬯ｯ・ｮ繝ｻ・ｯ郢晢ｽｻ繝ｻ・､鬮ｯ・ｷ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｦ鬯ｨ・ｾ郢晢ｽｻ郢晢ｽｻ
						if (!s_clickedOnBone && !io.KeyShift) {
							ImVec2 dragDelta(io.MousePos.x - s_mouseDownPos.x, io.MousePos.y - s_mouseDownPos.y);
							if (std::abs(dragDelta.x) < 2.0f && std::abs(dragDelta.y) < 2.0f) {
								simulationManager_->ClearSelectedBones();
							}
						}
					}
					s_isDraggingBone = false;
				}

				if (isBoxSelecting_) {
					float minViewX, minViewY, maxViewX, maxViewY;
					if (FlyCamera::GetGameViewBounds(minViewX, minViewY, maxViewX, maxViewY)) {
						ImVec2 start(minViewX + boxSelectStartPos_.x, minViewY + boxSelectStartPos_.y);
						ImVec2 end(minViewX + boxSelectEndPos_.x, minViewY + boxSelectEndPos_.y);
						ImU32 colFill = IM_COL32(100, 150, 250, 80);
						ImU32 colBorder = IM_COL32(150, 200, 255, 200);
						ImGui::GetForegroundDrawList()->AddRectFilled(start, end, colFill);
						ImGui::GetForegroundDrawList()->AddRect(start, end, colBorder, 0.0f, 0, 1.5f);
					}
				}
			}
		}
	} else {
		if (player_) {
			Vector3* targetPos = nullptr;
			Vector3 enemyPos;
			if (lockedEnemy_) {
				enemyPos = lockedEnemy_->GetPosition();
				targetPos = &enemyPos;
			}
			player_->UpdateCamera(camera.get(), targetPos);
		}
		camera->Update();
	}

	if (isSelectedOnlyPreview) {
		for (auto &enemy : enemies_) {
			enemy->UpdateModel();
		}
		if (enemyBulletManager_) {
			enemyBulletManager_->UpdateModels();
		}
		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	}

	for (Object3d *object3d : objects) {
		object3d->Update();
	}

	Vector2 size = sprite->GetSize();
	size.x = 300.0f;
	size.y = 300.0f;
	sprite->SetSize(size);

	if (environmentRenderer_->GetShowParticles() && updateSelectedParticles) {
	}


	// ==========================================
	// 鬩幢ｽ｢隴弱・・ｽ・ｺ陋滂ｽ･繝ｻ・ｰ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｨ・ｾ陷茨ｽｷ繝ｻ・ｽ繝ｻ・ｺ鬮ｯ譏ｴ繝ｻ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE鬯ｨ・ｾ郢晢ｽｻ郢晢ｽｻ
	// ==========================================
	if (allowMouseMissileFire && player_ && !isGameOver_ && !isSpecialAttackActive_) {
		Input *input = Input::GetInstance();
		// 鬮ｯ譎｢・ｽ・ｾ郢晢ｽｻ繝ｻ・ｦ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬯ｯ・ｨ繝ｻ・ｾ髮九・竏槭・・ｿ繝ｻ・･鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｦ鬮ｴ雜｣・ｽ・｣髯ｷ・ｷ繝ｻ・ｶ驕ｯ・ｶ繝ｻ・ｲ鬮ｯ・ｷ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｺ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｪ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬮ｯ譎｢・ｽ・ｶ郢晢ｽｻ繝ｻ・ｸ鬮ｯ貊捺ｱ壹・・ｽ繝ｻ・ｾ
		if (input->TriggerMouseButton(0)) {
			Enemy* aimTarget = nullptr;
			if (lockedEnemy_ && lockOnManager_->IsLockedEnemyAlive()) {
				aimTarget = lockedEnemy_;
			} else if (aimAssistEnemy_) {
				aimTarget = aimAssistEnemy_;
			}
			missilePresetManager_->FirePlayerMissile(MissileType::Normal, aimTarget);
		}

		// 鬮ｯ・ｷ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢繝ｻ・ｧ鬮ｮ蛹ｺ・ｩ・ｸ繝ｻ・ｽ繝ｻ・ｼ鬮ｴ蝓溷繭・つ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｪ鬩搾ｽｵ繝ｻ・ｺ髯滓坩・ｯ莨夲ｽｽ・ｽ髣・ｽｽ繝ｻ・ｬ繝ｻ・ｨ郢晢ｽｻ繝ｻ・ｵ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｸ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｲ鬩搾ｽｵ繝ｻ・ｺ髯滓坩・ｯ莨夲ｽｽ・ｽ霑｢證ｦ・ｽ・ｹ隴取得・ｽ・ｹ繝ｻ・｢郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢隴弱・・ｽ・ｺ闖ｴ・ｩ繝ｻ・ｦ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｰ鬮ｯ貊捺ｱ壹・・ｽ繝ｻ・ｾ
		if (input->TriggerMouseButton(1)) {
			lockOnManager_->BeginMultiLock();
		}
		if (isMultiLockCharging_ && input->PushMouseButton(1)) {
			lockOnManager_->UpdateMultiLock(activeCamera);
		}
		if (isMultiLockCharging_ && !input->PushMouseButton(1)) {
			lockOnManager_->FireMultiLockMissiles();
		}
	} else if (isMultiLockCharging_) {
		lockOnManager_->CancelMultiLock();
	}

	// ==========================================
	// 鬮ｯ貊捺ｱ壹・・ｽ繝ｻ・ｾ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ鬮ｯ・ｷ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｦ鬯ｨ・ｾ郢晢ｽｻ郢晢ｽｻ
	// ==========================================
	std::vector<Vector3> hitPositions;
	std::vector<Vector3> destroyedPositions;
	if (updateSelectedMissiles) {
		if (missileManager_) {
			missileManager_->Update(activeCamera, enemies_, obstacles_, hitPositions, destroyedPositions);
		}

		if (explosionManager_ && !hitPositions.empty()) {
			explosionManager_->CreateHitEffects(hitPositions);
		}
		if (explosionManager_ && !destroyedPositions.empty()) {
			explosionManager_->CreateDestructionEffects(destroyedPositions);
		}

	} else {
		if (missileManager_) {
			missileManager_->UpdateModels(activeCamera);
		}
	}

	// 鬮ｴ雜｣・ｽ・ｷ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢隴弱・・ｽ・ｧ繝ｻ・ｭ驛｢譎｢・ｽ・ｭ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｸ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・｣鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ
	if ((!isSimulation || updateSelectedMissiles || updateSelectedParticles || (shouldUpdateGame && isFullFlowPreview)) && explosionManager_) {
		explosionManager_->Update();
	}

	// 鬮ｯ讓奇ｽｻ繧托ｽｽ・ｽ繝ｻ・ｧ鬮ｯ・ｷ陋ｹ・ｻ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢隴弱・・ｱ螢ｹ繝ｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ鬮ｯ・ｷ髣鯉ｽｨ繝ｻ・ｽ繝ｻ・ｨ鬮｣蜴・ｽｽ・ｴ鬯ｮ・ｮ繝ｻ・｣郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｴ鬮ｫ・ｴ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｰ
	if (!isSimulation || updateSelectedMissiles || updateSelectedParticles || (shouldUpdateGame && isFullFlowPreview)) {
	}

	// ==========================================
	// 鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｨ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｳ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隰ｨ魑ｴﾂ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬯ｯ・ｯ郢晢ｽｻ繝ｻ閾･・ｸ・ｺ陝ｷ繝ｻ・ｽ・ｮ陜｣・､髴肴亢繝ｻ繝ｻ・ｯ驛｢譎｢・ｽ・ｻ
	// ==========================================
	if (showDebugColliders && updateDebugWireframes && debugColliderLinesObject && debugColliderLinesObject->GetModel()) {
		std::vector<VertexData> colliderVertices;
		const bool drawAllDebugFrames = !isSelectedOnlyPreview || isFullFlowPreview;
		const bool drawPlayerDebugFrame = drawAllDebugFrames || uiManager_->currentSimulationTarget_ == 0;
		const bool drawMissileDebugFrame = drawAllDebugFrames || uiManager_->currentSimulationTarget_ == 1;
		const bool drawEnemyDebugFrame = drawAllDebugFrames || uiManager_->currentSimulationTarget_ == 2;
		const bool drawObstacleDebugFrame = drawAllDebugFrames;

		auto pushLine = [&](const Vector3& start, const Vector3& end, const Vector4& color) {
			VertexData v1{};
			VertexData v2{};
			v1.position = { start.x, start.y, start.z, 1.0f };
			v2.position = { end.x, end.y, end.z, 1.0f };
			v1.normal = { 0.0f, 1.0f, 0.0f, 0.0f };
			v2.normal = { 0.0f, 1.0f, 0.0f, 0.0f };
			v1.texcoord = { 0.0f, 0.0f, 0.0f, 0.0f };
			v2.texcoord = { 1.0f, 1.0f, 0.0f, 0.0f };
			v1.color = color;
			v2.color = color;
			colliderVertices.push_back(v1);
			colliderVertices.push_back(v2);
		};

		auto addAABB = [&](const Vector3& center, const Vector3& extents, const Vector4& color) {
			Vector3 p[8] = {
				{ center.x - extents.x, center.y - extents.y, center.z - extents.z },
				{ center.x + extents.x, center.y - extents.y, center.z - extents.z },
				{ center.x + extents.x, center.y + extents.y, center.z - extents.z },
				{ center.x - extents.x, center.y + extents.y, center.z - extents.z },
				{ center.x - extents.x, center.y - extents.y, center.z + extents.z },
				{ center.x + extents.x, center.y - extents.y, center.z + extents.z },
				{ center.x + extents.x, center.y + extents.y, center.z + extents.z },
				{ center.x - extents.x, center.y + extents.y, center.z + extents.z },
			};

			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addOBB = [&](const Vector3& center, const Vector3& extents, const Vector3& rotation, const Vector4& color) {
			Vector3 local[8] = {
				{ -extents.x, -extents.y, -extents.z },
				{  extents.x, -extents.y, -extents.z },
				{  extents.x,  extents.y, -extents.z },
				{ -extents.x,  extents.y, -extents.z },
				{ -extents.x, -extents.y,  extents.z },
				{  extents.x, -extents.y,  extents.z },
				{  extents.x,  extents.y,  extents.z },
				{ -extents.x,  extents.y,  extents.z },
			};
			const Matrix4x4 rotationMatrix = MyMath::Multiply(
				MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation.x), MyMath::MakeRotateYMatrix(rotation.y)),
				MyMath::MakeRotateZMatrix(rotation.z));

			Vector3 p[8]{};
			for (int i = 0; i < 8; ++i) {
				const Vector3 rotated = MyMath::Transform(local[i], rotationMatrix);
				p[i] = AddVector3(center, rotated);
			}

			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addOBBShape = [&](const OBB& obb, const Vector4& color) {
			Vector3 p[8] = {
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
			};
			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addSphere = [&](const Vector3& center, float radius, const Vector4& color) {
			constexpr int segmentCount = 24;
			constexpr float twoPi = 6.283185307f;
			for (int i = 0; i < segmentCount; ++i) {
				const float angle1 = twoPi * static_cast<float>(i) / static_cast<float>(segmentCount);
				const float angle2 = twoPi * static_cast<float>(i + 1) / static_cast<float>(segmentCount);
				const float cos1 = std::cos(angle1);
				const float sin1 = std::sin(angle1);
				const float cos2 = std::cos(angle2);
				const float sin2 = std::sin(angle2);

				pushLine(
					{ center.x + radius * cos1, center.y + radius * sin1, center.z },
					{ center.x + radius * cos2, center.y + radius * sin2, center.z },
					color);
				pushLine(
					{ center.x, center.y + radius * cos1, center.z + radius * sin1 },
					{ center.x, center.y + radius * cos2, center.z + radius * sin2 },
					color);
				pushLine(
					{ center.x + radius * sin1, center.y, center.z + radius * cos1 },
					{ center.x + radius * sin2, center.y, center.z + radius * cos2 },
					color);
			}
		};

		// 1. 鬩幢ｽ｢隴惹ｸ橸ｽｹ・ｲ繝ｻ蜿厄ｽｨ謚ｵ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮAABB鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｨ鬯ｨ・ｾ郢晢ｽｻ郢晢ｽｻ
		if (drawPlayerDebugFrame && player_ && !player_->IsDead()) {
			addOBBShape(player_->GetOBB(), { 0.0f, 1.0f, 0.0f, 1.0f });
		}

		// 2. 鬯ｯ・ｮ繝ｻ・ｫ髫ｲ蟷｢・ｽ・ｷ郢晢ｽｻ繝ｻ・ｮ郢晢ｽｻ繝ｻ・ｳ鬮ｴ螟ｧ・､・ｲ繝ｻ・ｽ繝ｻ・ｩ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮAABB
		if (drawObstacleDebugFrame) {
			for (const auto& obstacle : obstacles_) {
				if (!obstacle || obstacle->IsStageBounds()) {
					continue;
				}
				// Model驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ讖ｸ・ｽ・ｳ髮狗ｿｫ繝ｻ隲､蜥弱＠繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬩幢ｽ｢隴寂・繝ｻ驍ｵ・ｺ髢ｧ・ｲ繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢隴擾ｽｴ郢晢ｽｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｰ鬩幢ｽ｢隴弱・魃ｵ驛｢譎｢・ｽ・｣鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ 郢晢ｽｻ郢晢ｽｻ郢晢ｽｻBlender鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｱ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ = 鬮ｮ蠑ｱ繝ｻ繝ｻ・ｽ繝ｻ・｣鬯ｩ蠅捺・繝ｻ・ｽ繝ｻ・ｺ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｯ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ鬩幢ｽ｢隴取ｧｫ蛯羨BB
				addOBBShape(obstacle->GetOBB(), { 0.0f, 1.0f, 1.0f, 1.0f });
			}
		}

		// 3. 鬮ｫ・ｰ繝ｻ・ｨ郢晢ｽｻ繝ｻ・ｵ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮAABB鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｨ鬯ｨ・ｾ郢晢ｽｻ郢晢ｽｻ
		if (drawEnemyDebugFrame) {
			for (const auto& enemy : enemies_) {
				if (!enemy->IsDead()) {
					addOBBShape(enemy->GetOBB(), { 1.0f, 0.0f, 0.0f, 1.0f });
				}
			}
		}

		if ((drawEnemyDebugFrame || drawMissileDebugFrame) && lockedEnemy_ && !lockedEnemy_->IsDead()) {
			addSphere(lockedEnemy_->GetPosition(), lockedEnemy_->GetCollisionRadius() + 0.35f, { 1.0f, 0.95f, 0.0f, 1.0f });
		}

		// 4. 鬯ｮ・｢繝ｻ・ｾ郢晢ｽｻ繝ｻ・ｪ鬮ｫ・ｶ陋ｹ繝ｻ・ｽ・ｺ闖ｴ・ｩ鬩｢謳ｾ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｵ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽElayer Bullets驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE
		if (drawMissileDebugFrame && missileManager_) {
			for (const auto& missile : missileManager_->GetMissiles()) {
				if (!missile->IsDead()) {
					// Sphere: Magenta (radius: 0.5f)
					addSphere(missile->GetPosition(), missile->GetCollisionRadius(), { 1.0f, 0.0f, 1.0f, 1.0f });
				}
			}
		}

		// 5. 鬮ｫ・ｰ繝ｻ・ｨ郢晢ｽｻ繝ｻ・ｵ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ貊捺ｱ壹・・ｽ繝ｻ・ｾ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽEnemy Bullets驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE
		if (drawEnemyDebugFrame && enemyBulletManager_) {
			for (const auto& bullet : enemyBulletManager_->GetBullets()) {
				if (!bullet.isDead) {
					// Sphere: Orange (radius: 0.5f)
					addSphere(bullet.position, 0.5f, { 1.0f, 0.5f, 0.0f, 1.0f });
				}
			}
		}
		// 鬯ｩ蛹・ｽｽ・ｨ郢晢ｽｻ繝ｻ・ｺ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ諛ｶ・ｽ・｣郢晢ｽｻ繝ｻ・ｴ鬮ｯ・ｷ繝ｻ・ｷ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢隰ｨ魑ｴﾂ鬩幢ｽ｢隴弱・・ｽ・ｪ繝ｻ・ｸ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬯ｯ・ｨ繝ｻ・ｾ髫ｲ・｡繝ｻ・ｾ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｪ鬯ｩ謳ｾ・ｽ・ｱ髯橸ｽ｢繝ｻ・ｹ郢晢ｽｻ陝ｶ譎・鴬郢晢ｽｻ繝ｻ・ｽ鬮ｯ・ｷ闔ｨ螟ｲ・ｽ・｣繝ｻ・ｰE鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ stuck 鬯ｯ・ｮ繝ｻ・ｦ郢晢ｽｻ繝ｻ・ｲ鬮ｮ蠑ｱ繝ｻ繝ｻ・ｽ繝ｻ・｢EE
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

	if (environmentRenderer_) {
		environmentRenderer_->Update(camera.get());
	}

#ifdef ENABLE_IMGUI
	if (uiManager_) {
		uiManager_->UpdateUI();
	}
#endif
	sprite->Update();
}

void GamePlayScene::Draw() {
	//3D鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢隴弱・ﾂｧ驍ｵ・ｺ陞溘ｑ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｧ鬩幢ｽ｢隴惹ｹ暦ｽｲ・ｺ鬩搾ｽｱ陝ｶ謨鳴陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｻ鬮ｮ荵昴・髷ｫ・ｩ郢晢ｽｻ郢晢ｽｻ
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	// 鬩幢ｽ｢隴惹ｸ橸ｽｹ・ｲ繝ｻ蜿厄ｽｨ謚ｵ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・
	if (player_) {
		player_->Draw();
	}

	bool isAnimationEditor = IsSimulationMode() && uiManager_ && uiManager_->currentSimulationTarget_ == 5;
	if (isAnimationEditor) {
		// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・｢鬩幢ｽ｢隴乗・・ｽ・ｹ隴∵ｻ・ｱｪ繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｧ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬯ｩ謳ｾ・ｽ・ｱ郢晢ｽｻ繝ｻ・ｨ鬯ｯ・ｮ繝ｻ・ｮ驛｢譎｢・ｽ・ｻ髯ｷ繝ｻ・ｽ・ｾ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｯ鬮ｯ譎｢・ｽ・ｶ郢晢ｽｻ繝ｻ・ｸ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢隴弱・魃ｵ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ髯ｷ・ｻ髢ｧ・ｲ繝ｻ・ｷ陝ｶ謨鳴陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｻ
		Object3dCommon::GetInstance()->SetCommonDrawSettings();
		if (skeletonLinesObject && skeletonLinesObject->GetModel()) {
			skeletonLinesObject->Draw();
		}
		return; // 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・｢鬩幢ｽ｢隴乗・・ｽ・ｹ隴∵ｻ・ｱｪ繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｧ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬯ｩ謳ｾ・ｽ・ｱ郢晢ｽｻ繝ｻ・ｨ鬯ｯ・ｮ繝ｻ・ｮ驛｢譎｢・ｽ・ｻ髯ｷ繝ｻ・ｽ・ｾ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴惹ｸ橸ｽｹ・ｲ繝ｻ蜿厄ｽｨ謚ｵ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｨ鬩幢ｽ｢隴弱・魃ｵ驛｢譎｢・ｽ・ｻ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｿ鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・
	}

	// 鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ驕ｶ蜀苓ｷ昴・・ｸ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｦ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬩幢ｽ｢隴弱・・ｽ・ｺ陋滂ｽ･繝ｻ・ｰ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｫ鬩幢ｽ｢繝ｻ・ｧ髯ｷ・ｻ髢ｧ・ｲ繝ｻ・ｷ陝ｶ謨鳴陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｻ
	if (missileManager_) {
		missileManager_->Draw();
	}

	// 鬮ｫ・ｰ繝ｻ・ｨ郢晢ｽｻ繝ｻ・ｵ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ貊捺ｱ壹・・ｽ繝ｻ・ｾ鬩幢ｽ｢繝ｻ・ｧ髯ｷ・ｻ髢ｧ・ｲ繝ｻ・ｷ陝ｶ謨鳴陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｻ
	if (enemyBulletManager_) {
		enemyBulletManager_->Draw();
	}

	Vector4 frustumPlanes[6];
	MyMath::ExtractFrustumPlanes(camera->GetViewProjectionMatrix(), frustumPlanes);

	// 鬮ｫ・ｰ繝ｻ・ｨ郢晢ｽｻ繝ｻ・ｵ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・
	for (const auto &enemy : enemies_) {
		Sphere enemySphere;
		enemySphere.center = enemy->GetPosition();
		enemySphere.radius = enemy->GetCollisionRadius();

		// 鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｻ鬯ｯ・ｮ繝ｻ・ｱ郢晢ｽｻ繝ｻ・｢鬮ｯ讓奇ｽｻ阮卍ｧ驛｢譎｢・ｽ・ｻ鬮ｯ諛ｶ・ｽ・｣郢晢ｽｻ繝ｻ・ｴ鬮ｯ・ｷ繝ｻ・ｷ髯具ｽｹ繝ｻ・ｻ驕ｶ鬆托ｽ･・｢繝ｻ・ｬ繝ｻ・ｰ髯ｷﾂ隲､諛医・鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ驕ｶ莨√・繝ｻ・ｸ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｼ髯具ｽｹ繝ｻ・ｻ驍ｵ・ｺ陷･・ｲ繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｰ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ
		if (MyMath::IsInFrustum(enemySphere, frustumPlanes)) {
			enemy->Draw();
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}
	for (const AmmoPickup &pickup : ammoPickups_) {
		if (pickup.object) {
			pickup.object->Draw();
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}

	// 鬯ｯ・ｮ繝ｻ・ｫ髫ｲ蟷｢・ｽ・ｷ郢晢ｽｻ繝ｻ・ｮ郢晢ｽｻ繝ｻ・ｳ鬮ｴ螟ｧ・､・ｲ繝ｻ・ｽ繝ｻ・ｩ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・
	for (const auto &obstacle : obstacles_) {
		Sphere obsSphere;
		obsSphere.center = obstacle->GetPosition();
		obsSphere.radius = MyMath::Length(obstacle->GetWorldHalfExtents());

		// 鬯ｨ・ｾ陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｻ鬯ｯ・ｮ繝ｻ・ｱ郢晢ｽｻ繝ｻ・｢鬮ｯ讓奇ｽｺ・ｷ驕倪・繝ｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬮ｯ諛ｶ・ｽ・｣郢晢ｽｻ繝ｻ・ｴ鬮ｯ・ｷ繝ｻ・ｷ鬮｣魃会ｽｽ・ｨ郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・鬩搾ｽｵ繝ｻ・ｺ髯ｷ莨夲ｽｽ・ｱ驕ｶ莨√・繝ｻ・ｸ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｫ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｰ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽE
		if (MyMath::IsInFrustum(obsSphere, frustumPlanes)) {
			obstacle->Draw();
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	//3D鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｪ鬩幢ｽ｢隴弱・ﾂｧ驍ｵ・ｺ陞溘ｑ・ｽ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｧ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴寂握縺狗ｹ晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・
	if (showPlane) {
		for (Object3d* object3d : objects) {
			object3d->Draw();
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・｢鬩幢ｽ｢隴乗・・ｽ・ｹ隴∵ｻ・ｱｪ繝ｻ・ｹ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｼ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｷ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｧ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳModel驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｽ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ蛹ｺ・ｺ・ｷ陷夲ｽｱ髫ｰ蜴・ｽｽ・ｨ鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・鬮ｯ蜈ｷ・ｽ・ｻ郢晢ｽｻ繝ｻ・ｶ鬮ｯ貅ｷ譯√・・ｽ繝ｻ・｡
	if (showModel && myModelObject) {
		myModelObject->Draw();
	}

	if (player_ && boundaryAlertObject_ && ceilingBoundaryAlertObject_) {
		static float pulseTime = 0.0f;
		pulseTime += 0.05f;
		float pulseAlpha = 0.5f + 0.5f * std::sin(pulseTime);

		auto drawBoundaryAlert = [&](Object3d* alertObject, const Vector3& position, const Vector3& normal, float intensity) {
			alertObject->SetScale({ 2.0f, 2.0f, 2.0f });

			// The plane model is already upright on the XY plane. Walls only need yaw;
			// the ceiling needs a pitch so the alert lies on the horizontal surface.
			Vector3 rotate = { 0.0f, std::atan2(normal.x, normal.z), 0.0f };
			if (normal.y > 0.5f) {
				rotate = { -1.570796f, 0.0f, 0.0f };
			}

			Model* m = alertObject->GetModel();
			if (m) {
				m->SetColor({ 1.0f, 1.0f, 1.0f, intensity * pulseAlpha });
			}

			alertObject->SetTranslate({
				position.x + normal.x * 0.5f,
				position.y + normal.y * 0.5f,
				position.z + normal.z * 0.5f
			});

			alertObject->SetRotate(rotate);
			alertObject->Update();
			alertObject->Draw();
		};

		if (player_->IsNearWallBoundary()) {
			drawBoundaryAlert(
				boundaryAlertObject_.get(),
				player_->GetWallBoundaryAlertPosition(),
				player_->GetWallBoundaryAlertNormal(),
				player_->GetWallBoundaryWarningIntensity());
		}
		if (player_->IsNearCeilingBoundary()) {
			drawBoundaryAlert(
				ceilingBoundaryAlertObject_.get(),
				player_->GetCeilingBoundaryAlertPosition(),
				player_->GetCeilingBoundaryAlertNormal(),
				player_->GetCeilingBoundaryWarningIntensity());
		}
		if (player_->IsNearBoundary()) {
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}
	
	if (showBones) {
		// 鬩幢ｽ｢隴弱・繝ｻ郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｯ・ｷ魄・ｽｹ闔繧会ｽｫ莨・ｽｦ・ｴ陜ｮ蠑ｱ繝ｻ繝ｻ・ｭ鬮ｯ讖ｸ・ｽ・ｳ髯橸ｽ｢繝ｻ・ｹ郢晢ｽｻ陝ｶ譎｢・ｿ・｡郢晢ｽｻ繝ｻ・ｺ鬮ｯ讖ｸ・ｽ・ｳ髮九・ﾂ・ｪ郢晢ｽｻ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ郢晢ｽｻ
		Object3dCommon::GetInstance()->SetCommonDrawSettings();

		// 鬩幢ｽ｢隴弱・繝ｻ郢晢ｽｻ繝ｻ・ｿ郢晢ｽｻ繝ｻ・ｽE鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｩ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・
		if (skeletonLinesObject && skeletonLinesObject->GetModel()) {
			skeletonLinesObject->Draw();
		}
	}

	if (showDebugColliders && debugColliderLinesObject && debugColliderLinesObject->GetModel()) {
		debugColliderLinesObject->Draw();
	}
	
	// 鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｨ鬩幢ｽ｢隴弱・・ｽ・ｼ隴∫ｵｶ蜃ｾ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｯ鬩幢ｽ｢隴寂・・・ｹ晢ｽｻ繝ｻ・ｳ郢晢ｽｻ繝ｻ・ｻ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・ (鬮ｮ雜｣・ｽ・ｺ郢晢ｽｻ繝ｻ・ｱ鬮ｯ貅ｯ・ｶ・｣繝ｻ・ｽ繝ｻ・ｦ鬮ｫ・ｴ陷ｴ繝ｻ・ｽ・ｽ繝ｻ・ｸ鬩搾ｽｵ繝ｻ・ｺ髯晢｣ｰ髮懶ｽ｣繝ｻ・ｽ繝ｻ・ｾ郢晢ｽｻ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｿ鬮ｴ蜿厄ｽｻ繧托ｽｽ・ｽ繝ｻ・｡鬮ｯ・ｷ闔ｨ螟ｲ・ｽ・ｽ繝ｻ・ｹ)
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (environmentRenderer_) environmentRenderer_->Draw();

	// explosionManager鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｯObject3d(鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｪ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｳ鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｰ)鬩幢ｽ｢繝ｻ・ｧ髯ｷ・ｻ髢ｧ・ｲ繝ｻ・ｷ陝ｶ謨鳴陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｻ鬩搾ｽｵ繝ｻ・ｺ髯ｷ・ｷ繝ｻ・ｶ郢晢ｽｻ霑｢證ｦ・ｽ・ｸ繝ｻ・ｺ髮九・竏槭・・ｽ遶擾ｽｫ繝ｻ・ｸ繝ｻ・ｲ驕ｶ荵嶺ｺ｢郢晢ｽｻ鬮ｯ貅ｯ・ｶ・｣繝ｻ・ｽ繝ｻ・ｦ鬯ｮ・ｫ繝ｻ・ｪ郢晢ｽｻ繝ｻ・ｭ鬮ｯ讖ｸ・ｽ・ｳ髯橸ｽ｢繝ｻ・ｹ郢晢ｽｻ陞ｳ螢ｽﾎ､郢晢ｽｻ繝ｻ・ｼ鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｳ鬮ｯ・ｷ郢晢ｽｻ繝ｻ・ｽ繝ｻ・ｺ鬩搾ｽｵ繝ｻ・ｺ驛｢譎｢・ｽ・ｻ
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (explosionManager_) explosionManager_->Draw();


	//Sprite鬩搾ｽｵ繝ｻ・ｺ郢晢ｽｻ繝ｻ・ｮ鬮ｫ・ｰ繝ｻ・ｰ髯ｷﾂ隲､諛医・鬮ｯ諞ｺ螻ｮ繝ｻ・ｽ繝ｻ・ｺ鬮ｮ荵昴・郢晢ｽｻ
	SpriteCommon::GetInstance()->SetCommonPipelineState();
	//鬩幢ｽ｢繝ｻ・ｧ郢晢ｽｻ繝ｻ・ｹ鬩幢ｽ｢隴惹ｸ橸ｽｹ・ｲ繝ｻ荳ｻ・ｸ・ｷ繝ｻ・ｹ繝ｻ・ｧ郢晢ｽｻ繝ｻ・､鬩幢ｽ｢隴惹ｹ暦ｽｲ・ｺ鬩搾ｽｱ陝ｶ謨鳴陋ｹ繝ｻ・ｽ・ｽ繝ｻ・ｻ
	if (showSprite) {
		sprite->Draw();
	}

	DrawOverlay();
}

void GamePlayScene::DrawOverlay() {
	if (isDebugCameraActive_ && !debugFlyCamera_) return;
	if (!isDebugCameraActive_ && !camera) return;

	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	if (!activeCamera) return;

	// HUD
	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
	const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
	const float statusPanelX = 20.0f;
	const float statusPanelY = screenHeight - 145.0f;
	const float ammoPanelX = screenWidth - 420.0f;
	const float ammoPanelY = screenHeight - 170.0f;
	if (hudPanelSprite_) {
		hudPanelSprite_->SetPosition({ statusPanelX, statusPanelY });
		hudPanelSprite_->SetSize({ 390.0f, 125.0f });
		hudPanelSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.92f });
		hudPanelSprite_->Update();
		hudPanelSprite_->Draw();
	}
	if (hudAmmoPanelSprite_) {
		hudAmmoPanelSprite_->SetPosition({ ammoPanelX, ammoPanelY });
		hudAmmoPanelSprite_->SetSize({ 400.0f, 150.0f });
		hudAmmoPanelSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.92f });
		hudAmmoPanelSprite_->Update();
		hudAmmoPanelSprite_->Draw();
	}

	auto drawLabel = [](Sprite *label, const Vector2 &position, const Vector2 &size) {
		if (!label) return;
		label->SetPosition(position);
		label->SetSize(size);
		label->Update();
		label->Draw();
	};
	drawLabel(hudHpLabelSprite_.get(), { statusPanelX + 30.0f, statusPanelY + 29.0f }, { 42.0f, 25.0f });
	drawLabel(hudSpLabelSprite_.get(), { statusPanelX + 30.0f, statusPanelY + 77.0f }, { 39.0f, 25.0f });
	drawLabel(hudAmmoLabelSprite_.get(), { ammoPanelX + 32.0f, ammoPanelY + 22.0f }, { 82.0f, 25.0f });

	auto drawText = [](const std::string &text, auto &sprites, float rightX, float y, const Vector4 &color) {
		constexpr float digitWidth = 22.0f;
		constexpr float digitHeight = 28.0f;
		const float startX = rightX - digitWidth * static_cast<float>(text.size());
		for (size_t index = 0; index < text.size() && index < sprites.size(); ++index) {
			Sprite *digitSprite = sprites[index].get();
			const int glyphIndex = (text[index] == '/') ? 10 : text[index] - '0';
			digitSprite->SetTextureLeftTop({ static_cast<float>(glyphIndex * 64), 0.0f });
			digitSprite->SetTextureSize({ 64.0f, 80.0f });
			digitSprite->SetPosition({ startX + digitWidth * static_cast<float>(index), y });
			digitSprite->SetSize({ digitWidth, digitHeight });
			digitSprite->SetColor(color);
			digitSprite->Update();
			digitSprite->Draw();
		}
	};
	const float statusGaugeX = statusPanelX + 88.0f;
	const float statusGaugeWidth = 265.0f;
	const float statusGaugeHeight = 18.0f;
	if (hpGaugeBackgroundSprite_ && hpGaugeFillSprite_) {
		hpGaugeBackgroundSprite_->SetPosition({ statusGaugeX - 2.0f, statusPanelY + 32.0f });
		hpGaugeBackgroundSprite_->SetSize({ statusGaugeWidth + 4.0f, statusGaugeHeight + 4.0f });
		hpGaugeBackgroundSprite_->SetColor({ 0.04f, 0.03f, 0.08f, 0.92f });
		hpGaugeBackgroundSprite_->Update();
		hpGaugeBackgroundSprite_->Draw();
		const float hpRatio = std::clamp(
			static_cast<float>(player_ ? player_->GetHP() : 0) / static_cast<float>(Player::kMaxHP),
			0.0f,
			1.0f);
		hpGaugeFillSprite_->SetPosition({ statusGaugeX, statusPanelY + 34.0f });
		hpGaugeFillSprite_->SetSize({ statusGaugeWidth * hpRatio, statusGaugeHeight });
		hpGaugeFillSprite_->SetColor({ 1.0f, 0.12f, 0.18f, 1.0f });
		hpGaugeFillSprite_->Update();
		hpGaugeFillSprite_->Draw();
	}
	if (hudNormalAmmoIconSprite_ && hudHomingAmmoIconSprite_) {
		hudNormalAmmoIconSprite_->SetTextureLeftTop({ 0.0f, 0.0f });
		hudNormalAmmoIconSprite_->SetTextureSize({ 887.0f, 887.0f });
		hudNormalAmmoIconSprite_->SetPosition({ ammoPanelX + 34.0f, ammoPanelY + 53.0f });
		hudNormalAmmoIconSprite_->SetSize({ 48.0f, 48.0f });
		hudNormalAmmoIconSprite_->Update();
		hudNormalAmmoIconSprite_->Draw();
		hudHomingAmmoIconSprite_->SetTextureLeftTop({ 887.0f, 0.0f });
		hudHomingAmmoIconSprite_->SetTextureSize({ 887.0f, 887.0f });
		hudHomingAmmoIconSprite_->SetPosition({ ammoPanelX + 34.0f, ammoPanelY + 91.0f });
		hudHomingAmmoIconSprite_->SetSize({ 48.0f, 48.0f });
		hudHomingAmmoIconSprite_->Update();
		hudHomingAmmoIconSprite_->Draw();
	}
	const Vector4 normalAmmoColor = isNormalReloading_
		? Vector4{ 1.0f, 1.0f, 0.65f, 1.0f }
		: Vector4{ 1.0f, 0.78f, 0.08f, 1.0f };
	const Vector4 homingAmmoColor = isHomingReloading_
		? Vector4{ 1.0f, 0.65f, 0.65f, 1.0f }
		: Vector4{ 1.0f, 0.18f, 0.12f, 1.0f };
	drawText(std::to_string(normalAmmoInMagazine_) + "/" + std::to_string(normalAmmoReserve_), hudNormalAmmoDigitSprites_, ammoPanelX + 365.0f, ammoPanelY + 60.0f, normalAmmoColor);
	drawText(std::to_string(homingAmmoInMagazine_) + "/" + std::to_string(homingAmmoReserve_), hudHomingAmmoDigitSprites_, ammoPanelX + 365.0f, ammoPanelY + 98.0f, homingAmmoColor);
	if (isNormalReloading_ && hudNormalReloadGaugeSprite_) {
		hudNormalReloadGaugeSprite_->SetPosition({ ammoPanelX + 90.0f, ammoPanelY + 89.0f });
		hudNormalReloadGaugeSprite_->SetSize({ 275.0f * static_cast<float>(normalReloadFrame_) / static_cast<float>(kReloadDurationFrames), 4.0f });
		hudNormalReloadGaugeSprite_->SetColor({ 1.0f, 0.78f, 0.08f, 1.0f });
		hudNormalReloadGaugeSprite_->Update();
		hudNormalReloadGaugeSprite_->Draw();
	}
	if (isHomingReloading_ && hudHomingReloadGaugeSprite_) {
		hudHomingReloadGaugeSprite_->SetPosition({ ammoPanelX + 90.0f, ammoPanelY + 127.0f });
		hudHomingReloadGaugeSprite_->SetSize({ 275.0f * static_cast<float>(homingReloadFrame_) / static_cast<float>(kReloadDurationFrames), 4.0f });
		hudHomingReloadGaugeSprite_->SetColor({ 1.0f, 0.18f, 0.12f, 1.0f });
		hudHomingReloadGaugeSprite_->Update();
		hudHomingReloadGaugeSprite_->Draw();
	}

	// gauge
	const float gaugeX = statusGaugeX;
	const float gaugeY = statusPanelY + 82.0f;
	const float gaugeWidth = statusGaugeWidth;
	const float gaugeHeight = 18.0f;
	if (spGaugeBackgroundSprite_ && spGaugeFillSprite_ && spGaugeCostMarkerSprite_) {
		spGaugeBackgroundSprite_->SetPosition({ gaugeX - 2.0f, gaugeY - 2.0f });
		spGaugeBackgroundSprite_->SetSize({ gaugeWidth + 4.0f, gaugeHeight + 4.0f });
		spGaugeBackgroundSprite_->SetColor({ 0.03f, 0.05f, 0.12f, 0.90f });
		spGaugeBackgroundSprite_->Update();
		spGaugeBackgroundSprite_->Draw();

		spGaugeFillSprite_->SetPosition({ gaugeX, gaugeY });
		spGaugeFillSprite_->SetSize({ gaugeWidth * std::clamp(spGauge_ / 100.0f, 0.0f, 1.0f), gaugeHeight });
		spGaugeFillSprite_->SetColor(isSpecialAttackActive_
			? Vector4{ 1.0f, 0.35f, 0.05f, 1.0f }
			: Vector4{ 0.05f, 0.75f, 1.0f, 1.0f });
		spGaugeFillSprite_->Update();
		spGaugeFillSprite_->Draw();

		spGaugeCostMarkerSprite_->SetPosition({ gaugeX + gaugeWidth * 0.5f - 1.0f, gaugeY });
		spGaugeCostMarkerSprite_->SetSize({ 2.0f, gaugeHeight });
		spGaugeCostMarkerSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.9f });
		spGaugeCostMarkerSprite_->Update();

	constexpr float kRadarRange = 250.0f;
	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
		spGaugeCostMarkerSprite_->Draw();
	}

	DrawRadar();

	bool isJammed = lockOnManager_->IsPlayerJammed(activeCamera);

	if (isMultiLockCharging_ && !multiLockTargets_.empty()) {
		std::map<Enemy*, int> targetCounts;
		for (Enemy *target : multiLockTargets_) {
			targetCounts[target]++;
		}
		for (auto const& [target, count] : targetCounts) {
			DrawMissileLockOnOverlaySprite(target, activeCamera->GetViewProjectionMatrix(), multiLockMarkerSprite_.get(), isJammed, count);
		}
	} else if (Enemy *overlayTarget = lockedEnemy_ ? lockedEnemy_ : aimAssistEnemy_) {
		DrawLockOnOverlaySprite(overlayTarget, activeCamera->GetViewProjectionMatrix(), lockOnReticleSprite_.get(), isJammed);
	} else {
		DrawAimCursorOverlaySprite(aimCursorSprite_.get(), isJammed);
	}
}

void GamePlayScene::DrawRadar() {
	if (!player_ || !radarFrameSprite_) {
		return;
	}

	constexpr float kRadarRange = 250.0f;
	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
	const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
	const float radarSize = (std::min)(230.0f, screenHeight * 0.30f);
	const float radarRadius = radarSize * 0.39f;
	const Vector2 radarCenter = {
		screenWidth - radarSize * 0.5f - 20.0f,
		radarSize * 0.5f + 20.0f
	};

	radarFrameSprite_->SetPosition(radarCenter);
	radarFrameSprite_->SetSize({ radarSize, radarSize });
	radarFrameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.96f });
	radarFrameSprite_->Update();
	radarFrameSprite_->Draw();

	radarSweepAngle_ += 0.025f;
	if (radarSweepAngle_ >= 6.2831853f) {
		radarSweepAngle_ -= 6.2831853f;
	}
	if (radarSweepSprite_) {
		radarSweepSprite_->SetPosition(radarCenter);
		radarSweepSprite_->SetSize({ radarRadius, 2.0f });
		radarSweepSprite_->SetRotation(radarSweepAngle_ - 1.5707963f);
		radarSweepSprite_->SetColor({ 0.0f, 0.9f, 1.0f, 0.48f });
		radarSweepSprite_->Update();
		radarSweepSprite_->Draw();
	}

	const Vector3 playerPosition = player_->GetPosition();
	const Vector3 playerForward = player_->GetForwardVector();
	const float forwardLength = std::sqrt(
		playerForward.x * playerForward.x + playerForward.z * playerForward.z);
	const float forwardX = forwardLength > 0.0001f ? playerForward.x / forwardLength : 0.0f;
	const float forwardZ = forwardLength > 0.0001f ? playerForward.z / forwardLength : 1.0f;
	const float rightX = forwardZ;
	const float rightZ = -forwardX;

	size_t blipIndex = 0;
	for (const auto &enemy : enemies_) {
		if (!enemy || enemy->IsDead() || blipIndex >= radarBlipSprites_.size()) {
			continue;
		}

		const Vector3 enemyPosition = enemy->GetPosition();
		const float deltaX = enemyPosition.x - playerPosition.x;
		const float deltaZ = enemyPosition.z - playerPosition.z;
		const float localRight = deltaX * rightX + deltaZ * rightZ;
		const float localForward = deltaX * forwardX + deltaZ * forwardZ;
		const float distance = std::sqrt(localRight * localRight + localForward * localForward);
		const float positionScale = distance > kRadarRange && distance > 0.0001f
			? kRadarRange / distance
			: 1.0f;

		Sprite *blip = radarBlipSprites_[blipIndex++].get();
		blip->SetPosition({
			radarCenter.x + (localRight * positionScale / kRadarRange) * radarRadius,
			radarCenter.y - (localForward * positionScale / kRadarRange) * radarRadius
		});
		const float blipSize = enemy->IsBoss() ? 12.0f : (enemy.get() == lockedEnemy_ ? 9.0f : 6.0f);
		blip->SetSize({ blipSize, blipSize });
		if (enemy.get() == lockedEnemy_) {
			blip->SetColor({ 1.0f, 0.95f, 0.05f, 1.0f });
		} else if (enemy->IsBoss()) {
			blip->SetColor({ 1.0f, 0.12f, 0.05f, 1.0f });
		} else {
			blip->SetColor({ 1.0f, 0.38f, 0.05f, 1.0f });
		}
		blip->Update();
		blip->Draw();
	}
}
