#include "Model.h"
#include "engine/Resource/TextureManager.h"
#include <fstream>
#include <sstream>
#include <string>
#include <wrl.h>
#include <cmath>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>


using Microsoft::WRL::ComPtr;

void Model::Initialize(ModelCommon *modelCommon, const std::string &directorypath, const std::string &filename) {

	//蠑墓焚縺ｧ蜿励￠蜿悶▲縺ｦ繝｡繝ｳ繝仙､画焚縺ｫ險倬鹸縺吶ｋ
	this->modelCommon_ = modelCommon;

	//繝｢繝・Ν縺ｮ隱ｭ縺ｿ霎ｼ縺ｿ
	if (filename.find(".gltf") != std::string::npos || filename.find(".GLTF") != std::string::npos) {
		modelData = LoadGltfFile(directorypath, filename);
	} else {
		modelData = LoadObjFile(directorypath, filename);
	}


	//鬆らせ繝・・繧ｿ菴懈・
	CreateVertexData();

	//繝槭ユ繝ｪ繧｢繝ｫ繝・・繧ｿ菴懈・
	CreateMaterialData();


	//.obj縺ｮ蜿ら・縺励※縺・ｋ繝・け繧ｹ繝√Ε繝輔ぃ繧､繝ｫ隱ｭ縺ｿ霎ｼ縺ｿ
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	//隱ｭ縺ｿ霎ｼ繧薙□繝・け繧ｹ繝√Ε縺ｮ逡ｪ蜿ｷ繧貞叙蠕・
	modelData.material.textureIndex =
		TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData.material.textureFilePath);

	// 繝・け繧ｹ繝√Ε繝輔ぃ繧､繝ｫ繝代せ
	//textureFilePath_= filename;

	textureFilePath_ = modelData.material.textureFilePath;
}


void Model::Draw() {
	SrvManager::GetInstance()->PreDraw();

	modelCommon_->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	//modelCommon_->GetDxCommon()->GetCommandList()->IASetIndexBuffer(nullptr);

	//蠖｢迥ｶ繧定ｨｭ螳壹１SO縺ｫ險ｭ螳壹＠縺ｦ縺・ｋ繧ゅ・縺ｨ縺ｯ縺ｾ縺溷挨縲ょ酔縺倥ｂ縺ｮ繧定ｨｭ螳壹☆繧九→閠・∴縺ｦ鄂ｮ縺代・繧医＞
	modelCommon_->GetDxCommon()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//繝槭ユ繝ｪ繧｢繝ｫCBuffer縺ｮ蝣ｴ謇繧定ｨｭ螳・
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, matetialResource->GetGPUVirtualAddress());

	//SRV縺ｮDescriptor縺ｮ蜈磯ｭ繧定ｨｭ螳・
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
	//謠冗判
	modelCommon_->GetDxCommon()->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
}


void Model::CreateVertexData() {
	//鬆らせ繝ｪ繧ｽ繝ｼ繧ｹ繧剃ｽ懈・
	vertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());



	////繝ｪ繧ｽ繝ｼ繧ｹ縺ｮ蜈磯ｭ縺ｮ繧｢繝峨Ξ繧ｹ縺九ｉ菴ｿ縺・
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();

	//菴ｿ逕ｨ縺吶ｋ繝ｪ繧ｽ繝ｼ繧ｹ縺ｮ繧ｵ繧､繧ｺ縺ｯ鬆らせ3縺､縺ｮ繧ｵ繧､繧ｺ
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	//1鬆らせ蠖薙◆繧翫・繧ｵ繧､繧ｺ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	//鬆らせ繝ｪ繧ｽ繝ｼ繧ｹ繝・・繧ｿ繧呈嶌縺崎ｾｼ繧
	VertexData *vertexData = nullptr;

	//譖ｸ縺崎ｾｼ繧轤ｺ縺ｮ繧｢繝峨Ξ繧ｹ繧貞叙蠕・
	vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));

	//鬆らせ繝・・繧ｿ繧偵Μ繧ｽ繝ｼ繧ｹ縺ｫ繧ｳ繝斐・
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
}


void Model::CreateMaterialData() {
	matetialResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));

	//譖ｸ縺崎ｾｼ繧縺溘ａ縺ｮ繧｢繝峨Ξ繧ｹ繧貞叙蠕・
	matetialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
	//莉雁屓縺ｯ襍､繧呈嶌縺崎ｾｼ繧薙〒隕九ｋ
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = 1;
	materialData->uvTransform = math->MakeIdentity4x4();

	materialData->shininess = 40.0f;
	materialData->alphaReference = modelData.material.alphaReference;
}

MaterialData Model::LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename) {
	//讒狗ｯ峨☆繧貴aterialData
	MaterialData materialData;
	//繝輔ぃ繧､繝ｫ縺九ｉ隱ｭ繧薙□1陦後ｒ譬ｼ邏阪☆繧九ｂ縺ｮ
	std::string line;
	//繝輔ぃ繧､繝ｫ繧帝幕縺・
	std::ifstream file(directoryPath + "/" + filename);
	//縺ｨ繧翫≠縺医★髢九￠縺ｪ縺九▲縺溘ｉ豁｢繧√ｋ
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		//identifier縺ｫ蠢懊§縺溷・逅・
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			//騾｣邯壹＠縺ｦ繝輔ぃ繧､繝ｫ繝代せ縺ｫ縺吶ｋ
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

ModelData Model::LoadObjFile(const std::string &directoryPath, const std::string &filename) {
	ModelData modelData;
	std::vector<Vector4> positions; //菴咲ｽｮ
	std::vector<Vector3> normals;  //豕慕ｷ・
	std::vector<Vector2> texcoords; //繝・け繧ｹ繝√Ε蠎ｧ讓・
	std::string line;  //繝輔ぃ繧､繝ｫ縺九ｉ隱ｭ繧薙□1陦後ｒ譬ｼ邏阪☆繧・

	std::ifstream file(directoryPath + "/" + filename);  //繝輔ぃ繧､繝ｫ繧帝幕縺・
	assert(file.is_open()); //縺ｨ繧翫≠縺医★髢九￠縺ｪ縺九▲縺溘ｉ豁｢繧√ｋ

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; //蜈磯ｭ縺ｮ隴伜挨蟄舌ｒ隱ｭ繧


		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f") {
			VertexData triangle[3];
			//髱｢縺ｯ荳芽ｧ貞ｽ｢髯仙ｮ・
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				//鬆らせ縺ｮ隕∫ｴ縺ｸ縺ｮindex縺ｯ縲御ｽ咲ｽｮ/uv/豕慕ｷ壹阪〒譬ｼ邏阪＆繧後※縺・ｋ縺ｮ縺ｧ縲∝・隗｣縺励※Index繧貞叙蠕励☆繧・
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');  //蛹ｺ蛻・ｊ縺ｧ繧､繝ｳ繝・ャ繧ｯ繧ｹ繧定ｪｭ繧薙〒縺・￥
					elementIndices[element] = std::stoi(index);
				}

				//隕∫ｴ縺ｸ縺ｮindex縺九ｉ縲∝ｮ滄圀縺ｮ隕∫ｴ縺ｮ蛟､繧貞叙蠕励＠縺ｦ鬆らせ繧呈ｧ狗ｯ峨☆繧・
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				VertexData vertex = { position,texcord,normal, {1.0f, 1.0f, 1.0f, 1.0f} };
				modelData.vertices.push_back(vertex);
				triangle[faceVertex] = vertex;
			}
		} else if (identifier == "mtllib") {
			//matrialTemplateLidrary繝輔ぃ繧､繝ｫ縺ｮ蜷榊燕繧貞叙蠕励☆繧・
			std::string materialFilename;
			s >> materialFilename;
			//蝓ｺ譛ｬ逧・↓obj繝輔ぃ繧､繝ｫ縺ｨ蜷御ｸ髫主ｱ､縺ｫmtl縺ｯ蟄伜惠縺輔ｌ繧九・縺ｧ縲√ョ繧｣繝ｬ繧ｯ繝医Μ蜷阪→繝輔ぃ繧､繝ｫ蜷阪ｒ貂｡縺・
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
}

ModelData Model::LoadGltfFile(const std::string &directoryPath, const std::string &filename) {
	ModelData modelData;
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);
	assert(scene && scene->HasMeshes());

	aiMesh* mesh = scene->mMeshes[0];
	std::vector<VertexData> tempVertices;
	for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
		VertexData vertex;
		vertex.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f };
		// 蜿ｳ謇九°繧牙ｷｦ謇九∈
		vertex.position.x *= -1.0f;
		
		if (mesh->HasNormals()) {
			vertex.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
			vertex.normal.x *= -1.0f;
		} else {
			vertex.normal = { 0.0f, 0.0f, -1.0f };
		}
		
		if (mesh->HasTextureCoords(0)) {
			vertex.texcoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
		} else {
			vertex.texcoord = { 0.0f, 0.0f };
		}
		
		vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		tempVertices.push_back(vertex);
	}
	
	for (uint32_t i = 0; i < mesh->mNumFaces; ++i) {
		aiFace face = mesh->mFaces[i];
		assert(face.mNumIndices == 3);
		// 髱｢縺ｮ蜷代″繧貞ｷｦ謇狗ｳｻ縺ｫ縺ゅｏ縺帙ｋ (0, 1, 2) -> (0, 2, 1)
		modelData.vertices.push_back(tempVertices[face.mIndices[0]]);
		modelData.vertices.push_back(tempVertices[face.mIndices[2]]);
		modelData.vertices.push_back(tempVertices[face.mIndices[1]]);
	}

	if (mesh->HasBones()) {
		modelData.isSkinned = true;
		for (uint32_t i = 0; i < mesh->mNumBones; ++i) {
			aiBone* bone = mesh->mBones[i];
			std::string jointName = bone->mName.C_Str();
			modelData.boneNames.push_back(jointName);
			JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

			aiMatrix4x4 m = bone->mOffsetMatrix;
			Matrix4x4 invBind = {
				m.a1, m.b1, m.c1, m.d1,
				m.a2, m.b2, m.c2, m.d2,
				m.a3, m.b3, m.c3, m.d3,
				m.a4, m.b4, m.c4, m.d4
			};
			
			// x霆ｸ蜿崎ｻ｢繧帝←逕ｨ (m_left = flipX * invBind * flipX)
			invBind.m[0][1] *= -1.0f;
			invBind.m[0][2] *= -1.0f;
			invBind.m[0][3] *= -1.0f;
			invBind.m[1][0] *= -1.0f;
			invBind.m[2][0] *= -1.0f;
			invBind.m[3][0] *= -1.0f;

			jointWeightData.inverseBindMatrix = invBind;

			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
				jointWeightData.vertexWeights.push_back({ bone->mWeights[weightIndex].mWeight, bone->mWeights[weightIndex].mVertexId });
				uint32_t vId = bone->mWeights[weightIndex].mVertexId;
				float weight = bone->mWeights[weightIndex].mWeight;
				for (int slot = 0; slot < 4; ++slot) {
					if (tempVertices[vId].weight[slot] == 0.0f) {
						tempVertices[vId].weight[slot] = weight;
						tempVertices[vId].index[slot] = i; // 繝｢繝・Ν縺ｮ繝懊・繝ｳIndex
						break;
					}
				}
			}
		}
	}

	// tempVertices縺ｫ譖ｸ縺崎ｾｼ繧薙□繧ｦ繧ｧ繧､繝医→繧､繝ｳ繝・ャ繧ｯ繧ｹ繧知odelData.vertices縺ｫ蜿肴丐縺吶ｋ縺溘ａ縺ｫ蜀肴ｧ狗ｯ峨☆繧・
	modelData.vertices.clear();
	for (uint32_t i = 0; i < mesh->mNumFaces; ++i) {
		aiFace face = mesh->mFaces[i];
		// 髱｢縺ｮ蜷代″繧貞ｷｦ謇狗ｳｻ縺ｫ縺ゅｏ縺帙ｋ (0, 1, 2) -> (0, 2, 1)
		modelData.vertices.push_back(tempVertices[face.mIndices[0]]);
		modelData.vertices.push_back(tempVertices[face.mIndices[2]]);
		modelData.vertices.push_back(tempVertices[face.mIndices[1]]);
	}

	if (scene->HasMaterials()) {
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString aiPath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
			std::string texFile = aiPath.C_Str();
			std::string baseDir = filePath.substr(0, filePath.find_last_of('/'));
			modelData.material.textureFilePath = baseDir + "/" + texFile;
		}
	}
	return modelData;
}

Node Model::ReadNode(aiNode* node) {
    Node result;
    aiVector3D scale, translate;
    aiQuaternion rotate;
    node->mTransformation.Decompose(scale, rotate, translate); // assimp縺ｮ陦悟・縺九ｉSRT繧呈歓蜃ｺ縺吶ｋ髢｢謨ｰ繧貞茜逕ｨ
    result.transform.scale = { scale.x, scale.y, scale.z }; // Scale縺ｯ縺昴・縺ｾ縺ｾ
    result.transform.rotate = { rotate.x, -rotate.y, -rotate.z, rotate.w }; // x霆ｸ繧貞渚霆｢縲√＆繧峨↓蝗櫁ｻ｢譁ｹ蜷代′騾・↑縺ｮ縺ｧ霆ｸ繧貞渚霆｢縺輔○繧・
    result.transform.translate = { -translate.x, translate.y, translate.z }; // x霆ｸ繧貞渚霆｢
    
    MyMath math;
    result.localMatrix = math.MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);
    
    result.name = node->mName.C_Str();
    result.children.resize(node->mNumChildren);
    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }
    return result;
}

Node Model::LoadNodeHierarchy(const std::string& directoryPath, const std::string& filename) {
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;
    const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
    assert(scene && scene->mRootNode);
    return ReadNode(scene->mRootNode);
}

int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
    Joint joint;
    joint.name = node.name;
    joint.localMatrix = node.localMatrix;
    MyMath math;
    joint.skeletonSpaceMatrix = math.MakeIdentity4x4();
    joint.transform = node.transform;
    joint.index = int32_t(joints.size()); // 迴ｾ蝨ｨ逋ｻ骭ｲ縺輔ｌ縺ｦ縺・ｋ謨ｰ繧棚ndex縺ｫ
    joint.parent = parent;
    joints.push_back(joint); // Skeleton縺ｮJoint蛻励↓霑ｽ蜉
    for (const Node& child : node.children) {
        // 蟄辱oint繧剃ｽ懈・縺励√◎縺ｮIndex繧堤匳骭ｲ
        int32_t childIndex = CreateJoint(child, joint.index, joints);
        joints[joint.index].children.push_back(childIndex);
    }
    // 閾ｪ霄ｫ縺ｮIndex繧定ｿ斐☆
    return joint.index;
}

void Update(Skeleton& skeleton) {
    // 縺吶∋縺ｦ縺ｮJoint繧呈峩譁ｰ縲りｦｪ縺瑚凶縺・・縺ｧ騾壼ｸｸ繝ｫ繝ｼ繝励〒蜃ｦ逅・庄閭ｽ縺ｫ縺ｪ縺｣縺ｦ縺・ｋ
    for (Joint& joint : skeleton.joints) {
        MyMath math;
        joint.localMatrix = math.MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
        if (joint.parent) { // 隕ｪ縺後＞繧後・隕ｪ縺ｮ陦悟・繧呈寺縺代ｋ
            joint.skeletonSpaceMatrix = joint.localMatrix * skeleton.joints[*joint.parent].skeletonSpaceMatrix;
        } else { // 隕ｪ縺後＞縺ｪ縺・・縺ｧlocalMatrix縺ｨskeletonSpaceMatrix縺ｯ荳閾ｴ縺吶ｋ
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}

Skeleton CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton;
    skeleton.root = CreateJoint(rootNode, std::nullopt, skeleton.joints);

    // 蜷榊燕縺ｨindex縺ｮ繝槭ャ繝斐Φ繧ｰ繧定｡後＞繧｢繧ｯ繧ｻ繧ｹ縺励ｄ縺吶￥縺吶ｋ
    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }

    Update(skeleton);

    return skeleton;
}

void Model::InitializeSphere(ModelCommon *modelCommon, int subdivision) {
	// 蠑墓焚縺ｧ蜿励￠蜿悶▲縺ｦ繝｡繝ｳ繝仙､画焚縺ｫ險倬鹸縺吶ｋ
	this->modelCommon_ = modelCommon;

	// 逅・・鬆らせ繧定ｨ育ｮ励☆繧九◆繧√・螟画焚
	const float pi = 3.1415926535f;
	const float latEvery = pi / subdivision;
	const float lonEvey = (2.0f * pi) / subdivision;

	std::vector<VertexData> tempVertices;

	// 逅・擇荳翫・鬆らせ繧定ｨ育ｮ・
	for (int latIndex = 0; latIndex <= subdivision; ++latIndex) {
		float lat = -pi / 2.0f + latEvery * latIndex;
		for (int lonIndex = 0; lonIndex <= subdivision; ++lonIndex) {
			float lon = lonEvey * lonIndex;
			VertexData vertex{};

			// 豕慕ｷ壹→蠎ｧ讓吶・險育ｮ・
			vertex.normal.x = std::cos(lat) * std::cos(lon);
			vertex.normal.y = std::sin(lat);
			vertex.normal.z = std::cos(lat) * std::sin(lon);
			vertex.position = { vertex.normal.x,vertex.normal.y,vertex.normal.z,1.0f };

			// UV蠎ｧ讓吶・險育ｮ・
			vertex.texcoord = { float(lonIndex) / subdivision,1.0f - float(latIndex) / subdivision };

			// 鬆らせ繧ｫ繝ｩ繝ｼ縺ｮ繝・ヵ繧ｩ繝ｫ繝・
			vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			tempVertices.push_back(vertex);
		}
	}

	// 荳芽ｧ貞ｽ｢繝ｪ繧ｹ繝医→縺励※縲modelData.vertices縺ｫ螻暮幕
	modelData.vertices.clear();
	for (int latIndex = 0; latIndex < subdivision; ++latIndex) {
		for (int lonIndex = 0; lonIndex < subdivision; ++lonIndex) {
			int start = (latIndex * (subdivision + 1)) + lonIndex;

			int a = start;
			int b = start + 1;
			int c = start + (subdivision + 1);
			int d = start + (subdivision + 1) + 1;

			// 荳芽ｧ貞ｽ｢1
			modelData.vertices.push_back(tempVertices[a]);
			modelData.vertices.push_back(tempVertices[c]);
			modelData.vertices.push_back(tempVertices[b]);

			// 荳芽ｧ貞ｽ｢2
			modelData.vertices.push_back(tempVertices[b]);
			modelData.vertices.push_back(tempVertices[c]);
			modelData.vertices.push_back(tempVertices[d]);
		}
	}

	// 鬆らせ繝・・繧ｿ縺ｨ繝槭ユ繝ｪ繧｢繝ｫ繝・・繧ｿ縺ｮ繝舌ャ繝輔ぃ菴懈・
	CreateVertexData();
	CreateMaterialData();
}

void Model::InitializePlane(ModelCommon *modelCommon) {
	this->modelCommon_ = modelCommon;
	modelData.vertices.clear();

	// 蟷ｳ髱｢繧呈ｧ区・縺吶ｋ4縺､縺ｮ鬆らせ・・Y蟷ｳ髱｢縲∝ｹ・縲・ｫ倥＆2・・
	VertexData v[4];
	// 蟾ｦ荳・
	v[0].position = { -1.0f, -1.0f, 0.0f, 1.0f };
	v[0].texcoord = { 0.0f, 1.0f };
	v[0].normal = { 0.0f, 0.0f, -1.0f };
	v[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 蟾ｦ荳・
	v[1].position = { -1.0f, 1.0f, 0.0f, 1.0f };
	v[1].texcoord = { 0.0f, 0.0f };
	v[1].normal = { 0.0f, 0.0f, -1.0f };
	v[1].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 蜿ｳ荳・
	v[2].position = { 1.0f, -1.0f, 0.0f, 1.0f };
	v[2].texcoord = { 1.0f, 1.0f };
	v[2].normal = { 0.0f, 0.0f, -1.0f };
	v[2].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 蜿ｳ荳・
	v[3].position = { 1.0f, 1.0f, 0.0f, 1.0f };
	v[3].texcoord = { 1.0f, 0.0f };
	v[3].normal = { 0.0f, 0.0f, -1.0f };
	v[3].color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 荳芽ｧ貞ｽ｢繝ｪ繧ｹ繝医→縺励※螻暮幕・医う繝ｳ繝・ャ繧ｯ繧ｹ繧剃ｽｿ繧上↑縺・峩譖ｸ縺搾ｼ・
	// 荳芽ｧ貞ｽ｢1・亥ｷｦ荳九∝ｷｦ荳翫∝承荳具ｼ・
	modelData.vertices.push_back(v[0]);
	modelData.vertices.push_back(v[1]);
	modelData.vertices.push_back(v[2]);
	// 荳芽ｧ貞ｽ｢2・亥ｷｦ荳翫∝承荳翫∝承荳具ｼ・
	modelData.vertices.push_back(v[1]);
	modelData.vertices.push_back(v[3]);
	modelData.vertices.push_back(v[2]);

	// 繝舌ャ繝輔ぃ菴懈・
	CreateVertexData();
	CreateMaterialData();

	// 繝・け繧ｹ繝√Ε縺ｮ險ｭ螳・
	textureFilePath_ = "resources/uvChecker.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
}

void Model::InitializeBox(ModelCommon *modelCommon) {
	this->modelCommon_ = modelCommon;
	modelData.vertices.clear();

	// 邂ｱ縺ｮ隗偵↓縺ｪ繧・縺､縺ｮ鬆らせ蠎ｧ讓呻ｼ亥ｹ・縲・ｫ倥＆2縲∝･･陦後″2・・
	Vector4 p[8] = {
		{-1.0f, -1.0f, -1.0f, 1.0f}, // 0: 蟾ｦ荳九・謇句燕
		{-1.0f,  1.0f, -1.0f, 1.0f}, // 1: 蟾ｦ荳翫・謇句燕
		{ 1.0f, -1.0f, -1.0f, 1.0f}, // 2: 蜿ｳ荳九・謇句燕
		{ 1.0f,  1.0f, -1.0f, 1.0f}, // 3: 蜿ｳ荳翫・謇句燕
		{-1.0f, -1.0f,  1.0f, 1.0f}, // 4: 蟾ｦ荳九・螂･
		{-1.0f,  1.0f,  1.0f, 1.0f}, // 5: 蟾ｦ荳翫・螂･
		{ 1.0f, -1.0f,  1.0f, 1.0f}, // 6: 蜿ｳ荳九・螂･
		{ 1.0f,  1.0f,  1.0f, 1.0f}  // 7: 蜿ｳ荳翫・螂･
	};

	// 6縺､縺ｮ髱｢縺悟髄縺・※縺・ｋ譁ｹ蜷托ｼ域ｳ慕ｷ夲ｼ・
	Vector3 nFront = { 0.0f,  0.0f, -1.0f };
	Vector3 nBack = { 0.0f,  0.0f,  1.0f };
	Vector3 nLeft = { -1.0f,  0.0f,  0.0f };
	Vector3 nRight = { 1.0f,  0.0f,  0.0f };
	Vector3 nTop = { 0.0f,  1.0f,  0.0f };
	Vector3 nBottom = { 0.0f, -1.0f,  0.0f };

	// 繝・け繧ｹ繝√Ε縺ｮUV蠎ｧ讓・
	Vector2 uv00 = { 0.0f, 1.0f };
	Vector2 uv01 = { 0.0f, 0.0f };
	Vector2 uv10 = { 1.0f, 1.0f };
	Vector2 uv11 = { 1.0f, 0.0f };

	// 髱｢・亥屁隗貞ｽ｢・昜ｸ芽ｧ貞ｽ｢2譫夲ｼ峨ｒ菴懊ｋ萓ｿ蛻ｩ髢｢謨ｰ
	auto addFace = [&](int i0, int i1, int i2, int i3, Vector3 n) {
		Vector4 c = { 1.0f, 1.0f, 1.0f, 1.0f };
		modelData.vertices.push_back({ p[i0], uv00, n, c });
		modelData.vertices.push_back({ p[i1], uv01, n, c });
		modelData.vertices.push_back({ p[i2], uv10, n, c });

		modelData.vertices.push_back({ p[i1], uv01, n, c });
		modelData.vertices.push_back({ p[i3], uv11, n, c });
		modelData.vertices.push_back({ p[i2], uv10, n, c });
		};

	// 蜑・ 螂･, 蟾ｦ, 蜿ｳ, 荳・ 荳・縺ｮ6髱｢繧剃ｽ懊ｋ
	addFace(0, 1, 2, 3, nFront);
	addFace(6, 7, 4, 5, nBack);
	addFace(4, 5, 0, 1, nLeft);
	addFace(2, 3, 6, 7, nRight);
	addFace(1, 5, 3, 7, nTop);
	addFace(4, 0, 6, 2, nBottom);

	// 繝舌ャ繝輔ぃ菴懈・
	CreateVertexData();
	CreateMaterialData();

	// 繝・け繧ｹ繝√Ε縺ｮ險ｭ螳・
	textureFilePath_ = "resources/uvChecker.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
}

void Model::InitializeRing(ModelCommon *modelCommon, int subdivision, float outerRadius, float innerRadius,
	bool isUvHorizontal, const Vector4& innerColor, const Vector4& outerColor,
	float startAngleDegree, float endAngleDegree, float fadeAngleDegree) {
	this->modelCommon_ = modelCommon;
	modelData.vertices.clear();

	const float pi = 3.1415926535f;
	
	// 隗貞ｺｦ縺ｮ陬懈ｭ｣・育ｵゆｺ・ｧ貞ｺｦ縺碁幕蟋玖ｧ貞ｺｦ莉･荳九↑繧・60蠎ｦ雜ｳ縺吶↑縺ｩ・・
	if (endAngleDegree <= startAngleDegree) endAngleDegree += 360.0f;
	
	float startRad = startAngleDegree * pi / 180.0f;
	float endRad = endAngleDegree * pi / 180.0f;
	float fadeRad = fadeAngleDegree * pi / 180.0f;
	float totalRad = endRad - startRad;

	std::vector<VertexData> tempVertices;

	// 繝ｪ繝ｳ繧ｰ縺ｮ鬆らせ繧定ｨ育ｮ・
	for (int i = 0; i <= subdivision; ++i) {
		float t = (float)i / subdivision;
		float theta = startRad + t * totalRad;
		float c = std::cos(theta);
		float s = std::sin(theta);
		
		// 繧｢繝ｫ繝輔ぃ繝輔ぉ繝ｼ繝峨・險育ｮ・
		float alpha = 1.0f;
		if (fadeRad > 0.0f) {
			float distFromStart = theta - startRad;
			float distFromEnd = endRad - theta;
			float minEdgeDist = (std::min)(distFromStart, distFromEnd);
			if (minEdgeDist < fadeRad) {
				alpha = minEdgeDist / fadeRad;
			}
		}

		Vector4 outColor = { outerColor.x, outerColor.y, outerColor.z, outerColor.w * alpha };
		Vector4 inColor = { innerColor.x, innerColor.y, innerColor.z, innerColor.w * alpha };

		// 繝・け繧ｹ繝√Ε蠎ｧ讓吶・險育ｮ・
		float uv_u = isUvHorizontal ? t : 0.0f;
		float uv_v = isUvHorizontal ? 0.0f : t;

		// 螟門・縺ｮ鬆らせ
		VertexData vOuter;
		vOuter.position = { outerRadius * c, outerRadius * s, 0.0f, 1.0f };
		vOuter.texcoord = { uv_u, uv_v };
		vOuter.normal = { 0.0f, 0.0f, -1.0f };
		vOuter.color = outColor;

		// 蜀・・縺ｮ鬆らせ
		VertexData vInner;
		vInner.position = { innerRadius * c, innerRadius * s, 0.0f, 1.0f };
		vInner.texcoord = { isUvHorizontal ? t : 1.0f, isUvHorizontal ? 1.0f : t };
		vInner.normal = { 0.0f, 0.0f, -1.0f };
		vInner.color = inColor;

		tempVertices.push_back(vOuter); // index 2*i
		tempVertices.push_back(vInner); // index 2*i + 1
	}

	// 荳芽ｧ貞ｽ｢繝ｪ繧ｹ繝医→縺励※螻暮幕
	// 蜑埼擇縺九ｉ隕九※譎りｨ亥屓繧奇ｼ亥ｷｦ謇句ｺｧ讓咏ｳｻ縺ｮ讓呎ｺ厄ｼ峨↓縺ｪ繧九ｈ縺・↓鬆らせ鬆・ｺ上ｒ險ｭ螳・
	for (int i = 0; i < subdivision; ++i) {
		int outer0 = 2 * i;
		int inner0 = 2 * i + 1;
		int outer1 = 2 * (i + 1);
		int inner1 = 2 * (i + 1) + 1;

		// 荳芽ｧ貞ｽ｢1: outer0 -> inner0 -> inner1
		modelData.vertices.push_back(tempVertices[outer0]);
		modelData.vertices.push_back(tempVertices[inner0]);
		modelData.vertices.push_back(tempVertices[inner1]);

		// 荳芽ｧ貞ｽ｢2: outer0 -> inner1 -> outer1
		modelData.vertices.push_back(tempVertices[outer0]);
		modelData.vertices.push_back(tempVertices[inner1]);
		modelData.vertices.push_back(tempVertices[outer1]);
	}

	// 鬆らせ繝・・繧ｿ縺ｨ繝槭ユ繝ｪ繧｢繝ｫ繝・・繧ｿ縺ｮ繝舌ャ繝輔ぃ菴懈・
	CreateVertexData();
	CreateMaterialData();

	// 繝・け繧ｹ繝√Ε縺ｮ險ｭ螳・
	textureFilePath_ = "resources/gradationLine.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
}

void Model::InitializeCylinder(ModelCommon *modelCommon,
	int subdivision, int verticalSubdivision,
	float topRadiusX, float topRadiusZ,
	float bottomRadiusX, float bottomRadiusZ,
	float height,
	const Vector4& topColor, const Vector4& bottomColor,
	float startAngleDegree, float endAngleDegree,
	bool isUvFlipped) {
	this->modelCommon_ = modelCommon;
	modelData.vertices.clear();

	const float pi = 3.1415926535f;
	if (endAngleDegree <= startAngleDegree) endAngleDegree += 360.0f;

	float startRad = startAngleDegree * pi / 180.0f;
	float endRad = endAngleDegree * pi / 180.0f;
	float totalRad = endRad - startRad;

	// 鬆らせ譬ｼ蟄舌ｒ逕滓・
	std::vector<std::vector<VertexData>> grid(verticalSubdivision + 1, std::vector<VertexData>(subdivision + 1));

	for (int v = 0; v <= verticalSubdivision; ++v) {
		float vt = (float)v / verticalSubdivision;
		float y = height * vt;
		float radiusX = bottomRadiusX + (topRadiusX - bottomRadiusX) * vt;
		float radiusZ = bottomRadiusZ + (topRadiusZ - bottomRadiusZ) * vt;
		Vector4 color = {
			bottomColor.x + (topColor.x - bottomColor.x) * vt,
			bottomColor.y + (topColor.y - bottomColor.y) * vt,
			bottomColor.z + (topColor.z - bottomColor.z) * vt,
			bottomColor.w + (topColor.w - bottomColor.w) * vt
		};

		for (int h = 0; h <= subdivision; ++h) {
			float ht = (float)h / subdivision;
			float theta = startRad + ht * totalRad;
			float sin = std::sin(theta);
			float cos = std::cos(theta);

			VertexData vertex;
			vertex.position = { -sin * radiusX, y, cos * radiusZ, 1.0f };

			float u = ht;
			float v_coord = isUvFlipped ? vt : (1.0f - vt);
			vertex.texcoord = { u, v_coord };

			// 豕慕ｷ壹・螟門髄縺・
			vertex.normal = { -sin, 0.0f, cos };
			vertex.color = color;

			grid[v][h] = vertex;
		}
	}

	// 荳芽ｧ貞ｽ｢繝ｪ繧ｹ繝医→縺励※螻暮幕
	for (int v = 0; v < verticalSubdivision; ++v) {
		for (int h = 0; h < subdivision; ++h) {
			const VertexData& v0 = grid[v][h];
			const VertexData& v1 = grid[v + 1][h];
			const VertexData& v2 = grid[v][h + 1];
			const VertexData& v3 = grid[v + 1][h + 1];

			// 荳芽ｧ貞ｽ｢1: v0 -> v1 -> v3
			modelData.vertices.push_back(v0);
			modelData.vertices.push_back(v3);
			modelData.vertices.push_back(v1);

			// 荳芽ｧ貞ｽ｢2: v0 -> v3 -> v2
			modelData.vertices.push_back(v0);
			modelData.vertices.push_back(v2);
			modelData.vertices.push_back(v3);
		}
	}

	// 鬆らせ繝・・繧ｿ縺ｨ繝槭ユ繝ｪ繧｢繝ｫ繝・・繧ｿ縺ｮ繝舌ャ繝輔ぃ菴懈・
	CreateVertexData();
	CreateMaterialData();

	// 繝・ヵ繧ｩ繝ｫ繝医〒繝ｩ繧､繝・ぅ繝ｳ繧ｰ繧堤┌蜉ｹ蛹・
	if (materialData) {
		materialData->enableLighting = 0;
	}

	// 繝・け繧ｹ繝√Ε縺ｮ險ｭ螳・
	textureFilePath_ = "resources/gradationLine.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
}

SkinCluster Model::CreateSkinCluster(const Skeleton& skeleton) {
	SkinCluster skinCluster;
	if (!modelData.isSkinned || modelData.boneNames.empty()) return skinCluster;

	skinCluster.paletteResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(WellKnownPalette) * modelData.boneNames.size());
	skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&skinCluster.mappedPalette));
	skinCluster.paletteAddress = skinCluster.paletteResource->GetGPUVirtualAddress();

	skinCluster.inverseBindMatrices.resize(modelData.boneNames.size());
	skinCluster.boneIndexToJointIndex.resize(modelData.boneNames.size());

	for (size_t i = 0; i < modelData.boneNames.size(); ++i) {
		const std::string& jointName = modelData.boneNames[i];
		auto it = skeleton.jointMap.find(jointName);
		if (it != skeleton.jointMap.end()) {
			skinCluster.boneIndexToJointIndex[i] = it->second;
		} else {
			skinCluster.boneIndexToJointIndex[i] = -1;
		}
		skinCluster.inverseBindMatrices[i] = modelData.skinClusterData[jointName].inverseBindMatrix;
	}
	return skinCluster;
}

void Model::UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton) {
	if (!skinCluster.mappedPalette) return;

	for (size_t i = 0; i < skinCluster.boneIndexToJointIndex.size(); ++i) {
		int32_t jointIndex = skinCluster.boneIndexToJointIndex[i];
		if (jointIndex >= 0) {
			Matrix4x4 skeletonSpaceMatrix = skeleton.joints[jointIndex].skeletonSpaceMatrix;
			skinCluster.mappedPalette[i].skeletonSpaceMatrix = math->Multiply(skinCluster.inverseBindMatrices[i], skeletonSpaceMatrix);
			
			// 法線用行列は逆転置
			Matrix4x4 inv = math->Inverse(skinCluster.mappedPalette[i].skeletonSpaceMatrix);
			skinCluster.mappedPalette[i].skeletonSpaceNormalMatrix = math->Transpose(inv);
		} else {
			skinCluster.mappedPalette[i].skeletonSpaceMatrix = math->MakeIdentity4x4();
			skinCluster.mappedPalette[i].skeletonSpaceNormalMatrix = math->MakeIdentity4x4();
		}
	}
}
