#include <model.hpp>
#include <material.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <stb_image.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <map>

void Model::Draw(Shader& shader) {
    for (unsigned int i = 0; i < meshes.size(); i++)
        meshes[i].Draw(shader);
}

void Model::loadModel(Scene& scene) {
    Assimp::Importer import;
    const aiScene *aiscene = import.ReadFile(scene.filename_in, aiProcess_Triangulate |
                                                 aiProcess_FlipUVs |
                                                 aiProcess_GenNormals);

    if(!aiscene || aiscene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !aiscene->mRootNode) {
        std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << "\n";
        return;
    }
    directory = scene.filename_in.substr(0, scene.filename_in.find_last_of('/'));
    processNode(aiscene->mRootNode, aiscene);

    unsigned numLights = 0;
    for(auto& mesh : meshes) {
        if(mesh.getMaterial().emissive != glm::vec3(0.0f, 0.0f, 0.0f)) {
            for(uint i=0; i<mesh.indices.size();) {
                LightTriangle lTriangle;

                lTriangle.color = mesh.getMaterial().emissive;
                lTriangle.intensity = mesh.getMaterial().intensity;

                lTriangle.pos[0] = mesh.vertices[mesh.indices[i]].Position;
                lTriangle.normal[0] = mesh.vertices[mesh.indices[i++]].Normal;
                lTriangle.pos[1] = mesh.vertices[mesh.indices[i]].Position;
                lTriangle.normal[1] = mesh.vertices[mesh.indices[i++]].Normal;
                lTriangle.pos[2] = mesh.vertices[mesh.indices[i]].Position;
                lTriangle.normal[2] = mesh.vertices[mesh.indices[i++]].Normal;

                scene.lightTriangle.push_back(lTriangle);
                numLights++;
            }
        }
    }
    std::cerr << numLights << " triangle lights loaded." << std::endl;
}

void Model::processNode(aiNode *node, const aiScene *scene) {
    // process all the node's meshes
    for(unsigned int i=0; i< node->mNumMeshes; i++) {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }
    // process all the children's meshes
    for(unsigned int i=0; i< node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene) {
    std::vector <Vertex> vertices;
    std::vector <unsigned int> indices;
    std::vector <Texture> textures;

    for(unsigned int i=0; i<mesh->mNumVertices; i++) {
        Vertex vertex;
        // process vertex positions, normals and texture coords
        glm::vec3 vector;
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.Position = vector;
        // vertex.Position /= 50.0f;
        
        vector.x = mesh->mNormals[i].x;
        vector.y = mesh->mNormals[i].y;
        vector.z = mesh->mNormals[i].z;
        vertex.Normal = vector;
        if(mesh->mTextureCoords[0]) { // DRAWBACK: Only cares about the first texture
            glm::vec2 vec;
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.TexCoords = vec;
        } else {
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }

    // process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }
    // process material
    Material color;
    aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
    color.set = true;

    aiColor3D tmpColor;
    material->Get(AI_MATKEY_COLOR_AMBIENT, tmpColor);
    color.ambient = glm::vec3(tmpColor.r, tmpColor.g, tmpColor.b);
    material->Get(AI_MATKEY_COLOR_DIFFUSE, tmpColor);
    color.diffuse = glm::vec3(tmpColor.r, tmpColor.g, tmpColor.b);
    material->Get(AI_MATKEY_COLOR_SPECULAR, tmpColor);
    color.specular = glm::vec3(tmpColor.r, tmpColor.g, tmpColor.b);
    material->Get(AI_MATKEY_COLOR_EMISSIVE, tmpColor);
    color.emissive = glm::vec3(tmpColor.r, tmpColor.g, tmpColor.b);
    material->Get(AI_MATKEY_SHININESS, color.shininess);
    material->Get(AI_MATKEY_REFRACTI, color.refraction);

    std::vector<Texture> diffuseMaps =
        loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    std::vector<Texture> specularMaps =
        loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    std::vector<Texture> normalMaps =
        loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    std::vector<Texture> heightMaps =
        loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

    return Mesh(vertices, indices, textures, color);
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName) {
    std::vector <Texture> textures;
    for(unsigned int i=0; i<mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        bool skip = false;
        for(unsigned int j=0; j<textures_loaded.size(); j++) {
            if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
                textures.push_back(textures_loaded[j]);
                skip = true;
                break;
            }
        }
        if(!skip) { // if it hasn't been loaded before
            Texture texture;
            texture.id = TextureFromFile(str.C_Str(), directory);
            texture.type = typeName;
            texture.path = str.C_Str();
            textures.push_back(texture);
            textures_loaded.push_back(texture);
        }
    }
    return textures;
}

unsigned int TextureFromFile(const char *path, const std::string &directory, bool gamma)
{
    std::string filename = std::string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGB;
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