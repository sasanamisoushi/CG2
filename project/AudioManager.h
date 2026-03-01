#pragma once
#include <xaudio2.h>
#include <wrl.h>
#include <string>
#include <cstdint>

//音声データ
struct SoundData {
	//波形フォーマット
	WAVEFORMATEX wfex;
	//バッファの先頭アドレス
	BYTE *pBuffer;
	//バッファのサイズ
	unsigned int bufferSize;
};


//チャンクヘッダ
struct ChunkHeader {
	char id[4];  //チャンク毎のID
	int32_t size; //チャンクサイズ
};

//RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk; //"RIFF"
	char type[4];   //"WAVE"
};

//FMTチャンク
struct FormatChunk {
	ChunkHeader chunk; //"fmt"
	WAVEFORMATEX fmt;  //波形フォーマット
};

class AudioManager {
public:
	//シングルトンインスタンスの取得
	static AudioManager *GetInstance();
	
	//終了処理
	void Finalize();

	//初期化
	void Initialize();

	//WAVファイルの読み込み
	SoundData LoadWave(const std::string &filename);

	//音声再生
	IXAudio2SourceVoice *PlayWave(const SoundData &soundData, bool loop = false);

	//音声データの解放
	void UnloadWave(SoundData &soundData);

private:
	static AudioManager *instance;

	AudioManager() = default;
	~AudioManager() = default;
	AudioManager(const AudioManager &)=delete;
	AudioManager &operator=(const AudioManager &) = delete;

	//XAudio2のインターフェース
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice *masterVoice_=nullptr;

};

