#include "myModel.h"

myModel::myModel(std::string path) {
	loadModel(path);
	optimize();
}

void myModel::loadModel(std::string path) {

	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
		return;
	}

	directory = path.substr(0, path.find_last_of('/'));
	processNode(scene->mRootNode, scene);

}

//一个node含有mesh和子node，所以需要递归，将所有的mesh都拿出来
void myModel::processNode(aiNode* node, const aiScene* scene) {

	for (uint32_t i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		this->meshs.push_back(processMesh(mesh, scene));
	}

	for (uint32_t i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene);
	}

}

Mesh myModel::processMesh(aiMesh* mesh, const aiScene* scene) {

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Texture> textures;

	for (uint32_t i = 0; i < mesh->mNumVertices; i++) {

		Vertex vertex;
		glm::vec3 vector;

		vector.x = mesh->mVertices[i].x;
		vector.y = mesh->mVertices[i].y;
		vector.z = mesh->mVertices[i].z;
		vertex.pos = vector;

		if (mesh->HasNormals()) {

			vector.x = mesh->mNormals[i].x;
			vector.y = mesh->mNormals[i].y;
			vector.z = mesh->mNormals[i].z;
			vertex.normal = vector;

		}

		if (mesh->HasTangentsAndBitangents()) {

			vector.x = mesh->mTangents[i].x;
			vector.y = mesh->mTangents[i].y;
			vector.z = mesh->mTangents[i].z;
			vertex.tangent = vector;

		}

		if (mesh->mTextureCoords[0]) // 网格是否有纹理坐标？
		{
			glm::vec2 vec;
			vec.x = mesh->mTextureCoords[0][i].x;
			vec.y = mesh->mTextureCoords[0][i].y;
			vertex.texCoord = vec;
		}
		else {
			vertex.texCoord = glm::vec2(0.0f, 0.0f);
		}

		vertices.push_back(vertex);

	}

	for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (uint32_t j = 0; j < face.mNumIndices; j++) {
			indices.push_back(face.mIndices[j]);
		}
	}

	if (mesh->mMaterialIndex >= 0) {
		
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_albedo");
		textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

		//std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
		//textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

		std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
		textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

	}

	return Mesh(vertices, indices, textures);

}

std::vector<Texture> myModel::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName) {

	std::vector<Texture> textures;
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
	{
		aiString str;
		mat->GetTexture(type, i, &str);
		bool skip = false;
		for (unsigned int j = 0; j < textures_loaded.size(); j++)
		{
			if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
			{
				textures.push_back(textures_loaded[j]);
				skip = true;
				break;
			}
		}
		if (!skip)
		{   // 如果纹理还没有被加载，则加载它
			Texture texture;
			//texture.id = TextureFromFile(str.C_Str(), directory);
			texture.type = typeName;
			texture.path = directory + '/' + str.C_Str();
			textures.push_back(texture);
			textures_loaded.push_back(texture); // 添加到已加载的纹理中
		}
	}

	return textures;

}

/*
unsigned int myModel::TextureFromFile(const char* path, const std::string& directory, bool gamma)
{
	std::string filename = std::string(path);
	filename = directory + '/' + filename;

	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
	if (data)
	{
		GLenum format;
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}

	return textureID;

}
*/

//obj中很多时候其indices和vertices是一样的，那么我们可以优化，去掉重复的顶点，使得vertexBuffer减少
void myModel::optimize() {

	for (int i = 0; i < this->meshs.size(); i++) {

		//就当模型创建者已经优化了
		if (this->meshs[i].indices.size() != this->meshs[i].vertices.size()) {
			break;
		}

		std::unordered_map<Vertex, uint32_t> uniqueVerticesMap{};
		std::vector<Vertex> uniqueVertices;
		std::vector<uint32_t> uniqueIndices;
		uint32_t verticesSize = 0;
		for (uint32_t j = 0; j < this->meshs[i].vertices.size(); j++) {
			Vertex vertex = this->meshs[i].vertices[j];
			if (uniqueVerticesMap.count(vertex) == 0) {
				uniqueVerticesMap[vertex] = static_cast<uint32_t>(verticesSize);
				verticesSize++;
				uniqueVertices.push_back(vertex);
			}
			uniqueIndices.push_back(uniqueVerticesMap[vertex]);
		}
		this->meshs[i].vertices = uniqueVertices;
		this->meshs[i].indices = uniqueIndices;

	}

}