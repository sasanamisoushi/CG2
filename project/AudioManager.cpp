#include "AudioManager.h"
#include <fstream>
#include <cassert>
#include "StringUtility.h"
#include <vector>

#pragma comment(lib,"Mfplat.lib")
#pragma comment(lib,"Mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")

using namespace Microsoft::WRL;


AudioManager *AudioManager::instance = nullptr;

AudioManager *AudioManager::GetInstance() {
    if (instance == nullptr) {
        instance = new AudioManager();
    }
    return instance;
}

void AudioManager::Finalize() {

	//Foundationの終了処理
	MFShutdown();
	CoUninitialize();

    delete instance;
    instance = nullptr;
}

void AudioManager::Initialize() {
    HRESULT hr;

	//comの初期化
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// Foundationの初期化を追加
	hr = MFStartup(MF_VERSION);
	assert(SUCCEEDED(hr));

    //XAudio2の初期化
    hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(hr));

	//マスターボイスの生成
	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(hr));
}

SoundData AudioManager::LoadWave(const std::string &filename) {
	//ファイル入力ストリームのインスタンス
	std::ifstream file;
	//wavファイルをバイナリモードで開く
	file.open(filename, std::ios_base::binary);
	//ファイルオープン失敗を検出する
	assert(file.is_open());

	//RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char *)&riff, sizeof(riff));
	//ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}
	//タイプかWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}

	//Formatチャンクの読み込み
	FormatChunk format = {};
	//チャンクヘッダーの確認
	file.read((char *)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(0);
	}

	//チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char *)&format.fmt, format.chunk.size);

	//Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char *)&data, sizeof(data));
	//JUNKチャンクを検出
	if (strncmp(data.id, "JUNK", 4) == 0) {
		//読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		//再度読み込み
		file.read((char *)&data, sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}

	//Dataチャンクのデータ部
	std::vector<BYTE> buffer(data.size);
	file.read(reinterpret_cast<char *>(buffer.data()), data.size);

	//Waveファイルを閉じる
	file.close();

	//returnする為の音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.buffer = std::move(buffer);

	return soundData;
}

SoundData AudioManager::LoadAudio(const std::string &filename) {
	HRESULT hr;

	// stringをwstringに変換
	StringUtility su;
	std::wstring wFilename = su.ConvertString(filename);

	// SourceReaderを作成
	ComPtr<IMFSourceReader> pReader; 
	hr = MFCreateSourceReaderFromURL(wFilename.c_str(), nullptr, &pReader);
	assert(SUCCEEDED(hr) && "Audio file not found or unsupported format!");

	//オーディオストリームだけを選択
	pReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
	pReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

	//出力フォーマットをPCMに指定
	ComPtr<IMFMediaType> pPartialType;
	MFCreateMediaType(&pPartialType);
	pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType.Get());
	assert(SUCCEEDED(hr));

	//設定後の正確なフォーマット情報を取得
	ComPtr<IMFMediaType> pUncompressedAudioTYpe;
	hr = pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pUncompressedAudioTYpe);
	assert(SUCCEEDED(hr));

	//XAudio2で使える　WAVEFORMATEX 形式に変換
	WAVEFORMATEX *pWavFormat = nullptr;
	UINT32 formatSize = 0;
	MFCreateWaveFormatExFromMFMediaType(pUncompressedAudioTYpe.Get(), &pWavFormat, &formatSize);

	//データをすべて読み込む
	std::vector<BYTE> audioData;
	while (true) {
		DWORD flags = 0;
		ComPtr<IMFSample> pSample;
		hr = pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &pSample);
		assert(SUCCEEDED(hr));

		//最後まで読み込んだら終了
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
			break;
		}

		if (pSample) {
			ComPtr<IMFMediaBuffer> pBuffer;
			hr = pSample->ConvertToContiguousBuffer(&pBuffer);
			assert(SUCCEEDED(hr));

			BYTE *pAudioData = nullptr;
			DWORD currentLength = 0;
			hr = pBuffer->Lock(&pAudioData, nullptr, &currentLength);
			assert(SUCCEEDED(hr));

			//読み込んだデータを配列に追加
			audioData.insert(audioData.end(), pAudioData, pAudioData + currentLength);

			pBuffer->Unlock();
		}
	}

	//戻り値用のデータを構築
	SoundData soundData = {};
	soundData.wfex = *pWavFormat;
	soundData.wfex.cbSize = 0;
	soundData.buffer = std::move(audioData);

	//Media Foundationが確保したメモリを解放
	CoTaskMemFree(pWavFormat);

	return soundData;
}

IXAudio2SourceVoice *AudioManager::PlayWave(const SoundData &soundData, bool loop) {
	HRESULT result;

	//波形ファーマットを元にSourceVoiceの生成
	IXAudio2SourceVoice *pSourceVoice = nullptr;
	result = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	//再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.buffer.data();
	buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
	buf.Flags = XAUDIO2_END_OF_STREAM;

	if (loop) {
		buf.LoopCount = XAUDIO2_LOOP_INFINITE; // ループ再生
	}

	//波形データ
	result = pSourceVoice->SubmitSourceBuffer(&buf);

	//再生開始
	result = pSourceVoice->Start(0);
	assert(SUCCEEDED(result));

	return pSourceVoice;

}

void AudioManager::UnloadWave(SoundData &soundData) {
	// vectorの機能を使ってメモリを解放
	soundData.buffer.clear();
	soundData.buffer.shrink_to_fit();
	soundData.wfex = {};
}
