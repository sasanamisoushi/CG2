#pragma once
class BaseScene {
public:

	//仮想デストラクタを追加
	virtual ~BaseScene() = default;

	virtual void Initialize()=0;

	virtual void Finalize()=0;

	virtual void Update()=0;

	virtual void Draw()=0;

};

