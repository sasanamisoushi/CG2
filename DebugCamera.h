#pragma once
#include "Math.h"

class DebugCamera {

	//X,Y,Z軸周りのローカル回転角度
	Vector3 rotation_ = { 0,0,0 };

	//ローカル座標
	Vector3 translation_ = { 0,0,-50 };

	
public:

	void Initialize();

	void Update();

	bool ForwardMove;

	bool BackwardMove;

	bool UpMove;

	bool DownMove;

	bool RightMove;

	bool LeftMove;

	Math math_;

};

