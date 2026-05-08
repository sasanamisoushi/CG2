ModelData Model::LoadObjFile(const std::string &directoryPath, const std::string &filename) {
	ModelData modelData;
	std::vector<Vector4> positions; //位置
	std::vector<Vector3> normals;  //法線
	std::vector<Vector2> texcoords; //テクスチャ座標
	std::string line;  //ファイルから読んだ1行を格納するもの
	std::map<std::string, uint32_t> vertexMap; // 頂点データの重複を防ぐためのマップ

	std::ifstream file(directoryPath + "/" + filename);  //ファイルを開く
	assert(file.is_open()); //とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; //先頭の識別子を読む


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
			//面は三角形限定
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				// マップにこの頂点が存在するか確認
				if (vertexMap.find(vertexDefinition) == vertexMap.end()) {
					//頂点の要素へのindexは「位置/uv/法線」で格納されているので、分解してIndexを取得する
					std::istringstream v(vertexDefinition);
					uint32_t elementIndices[3];
					for (int32_t element = 0; element < 3; ++element) {
						std::string index;
						std::getline(v, index, '/');  //区切りでインデックスを読んでいく
						elementIndices[element] = std::stoi(index);
					}

					//要素へのindexから、実際の要素の値を取得して頂点を構築する
					Vector4 position = positions[elementIndices[0] - 1];
					Vector2 texcord = texcoords[elementIndices[1] - 1];
					Vector3 normal = normals[elementIndices[2] - 1];
					VertexData vertex = { position,texcord,normal, {1.0f, 1.0f, 1.0f, 1.0f} };
					
					// マップに登録し、頂点配列に追加
					vertexMap[vertexDefinition] = (uint32_t)modelData.vertices.size();
					modelData.vertices.push_back(vertex);
				}

				// インデックス配列に追加
				modelData.indices.push_back(vertexMap[vertexDefinition]);
			}
		} else if (identifier == "mtllib") {
			//matrialTemplateLidraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			//基本的にobjファイルと同一階層にmtlは存在されるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
}
