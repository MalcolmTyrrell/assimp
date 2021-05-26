#include "ExchangeMeshFactory.hpp"


#include <cassert>
#include "assimp/scene.h"

namespace {
using VertexNormalTextureIndices = std::tuple<unsigned int, unsigned int, unsigned int>;
}

namespace std {
template <>
struct hash<VertexNormalTextureIndices> {
    std::size_t operator()(VertexNormalTextureIndices const &indices) const noexcept {
        std::size_t hash = 23;
        hash = hash * 31 + std::get<0>(indices);
        hash = hash * 31 + std::get<1>(indices);
        hash = hash * 31 + std::get<2>(indices);
        return hash;
    }
};
}

namespace {

struct MeshData {
    A3DGraphStyleData style_data;
    std::vector<aiFace> assimp_faces;
    std::vector<aiVector3D> assimp_vertices, assimp_normals, assimp_texcoords;
    std::unordered_map<VertexNormalTextureIndices, unsigned int> exchange_vn_to_assimp_index_map;
};


unsigned int getAssimpVertexIndex(VertexNormalTextureIndices const &exchange_indices, MeshData &mesh_data, std::shared_ptr<ts3d::Tess3DInstance> const &tess3d) {
    auto const it = mesh_data.exchange_vn_to_assimp_index_map.find(exchange_indices);
    if (std::end(mesh_data.exchange_vn_to_assimp_index_map) != it) {
        return it->second;
    }

    auto const vertex_index = std::get<0>(exchange_indices);
    aiVector3D const new_vertex(
            { static_cast<float>(tess3d->coords()[vertex_index + 0]),
                    static_cast<float>(tess3d->coords()[vertex_index + 1]),
                    static_cast<float>(tess3d->coords()[vertex_index + 2]) });
    mesh_data.assimp_vertices.emplace_back(new_vertex);

    auto const normal_index = std::get<1>(exchange_indices);
    aiVector3D const new_normal(
            { static_cast<float>(tess3d->normals()[normal_index + 0]),
                    static_cast<float>(tess3d->normals()[normal_index + 1]),
                    static_cast<float>(tess3d->normals()[normal_index + 2]) });
    mesh_data.assimp_normals.emplace_back(new_normal);

    auto const texture_index = std::get<2>(exchange_indices);
    if (texture_index < tess3d->texCoordsSize()) {
        aiVector3D const new_texcoord(
                { static_cast<float>(tess3d->texCoords()[texture_index + 0]),
                        static_cast<float>(tess3d->texCoords()[texture_index + 1]),
                        0.f });
        mesh_data.assimp_texcoords.emplace_back(new_texcoord);
    }

    return mesh_data.exchange_vn_to_assimp_index_map[exchange_indices] = static_cast<unsigned int>(mesh_data.assimp_vertices.size() - 1);
}

void addFace(std::shared_ptr<ts3d::Tess3DInstance> const &tess3d,
        unsigned int &topo_face_idx,
        MeshData &mesh_data ) {

    auto const topo_face_mesh = tess3d->getIndexMeshForFace(topo_face_idx);
    auto const n_vertices = topo_face_mesh.vertices().size();
    auto const n_triangles = n_vertices / 3;

    mesh_data.assimp_faces.reserve(mesh_data.assimp_faces.size() + n_triangles);
    auto const has_tex_coords = (0 != tess3d->texCoordsSize());
    for (auto tri_idx = 0u; tri_idx < n_triangles; ++tri_idx) {
        aiFace new_face;
        new_face.mNumIndices = 3u;
        new_face.mIndices = new unsigned int[3];

        auto const offset = tri_idx * 3;
        for (auto vertex_idx = 0u; vertex_idx < 3u; ++vertex_idx) {
            auto const vertex_index = topo_face_mesh.vertices()[offset + vertex_idx];
            auto const normal_index = topo_face_mesh.normals()[offset + vertex_idx];
            auto const tex_index = has_tex_coords ? topo_face_mesh.textures()[offset + vertex_idx] : 0u;
            new_face.mIndices[vertex_idx] = getAssimpVertexIndex(std::make_tuple(vertex_index, normal_index, tex_index), mesh_data, tess3d);
        }
        mesh_data.assimp_faces.emplace_back(new_face);
    }
}
} // namespace

Assimp::ExchangeMeshFactory::ExchangeMeshFactory(aiScene *pScene) :
    mScene(pScene),
    mMaterialLoader( std::make_shared<ExchangeMaterialLoader>(pScene)) {
}

std::set<unsigned int> Assimp::ExchangeMeshFactory::getMeshIndices(ts3d::InstancePath const& ri_instance_path) {
    auto const ri_style = ts3d::Instance(ri_instance_path).getNetStyle();
    auto const ri = ri_instance_path.back();
    auto const it = mExistingMeshes.find(ri);
    if (std::end(mExistingMeshes) != it) {
        for (auto const &existing_mesh : it->second) {
            if (0 == memcmp(&ri_style, &existing_mesh.mStyleData, sizeof(A3DGraphStyleData))) {
                return existing_mesh.mMeshIndices;
            }
        }
    }

    std::vector<aiMesh *> scene_meshes;
    if (mScene->mNumMeshes) {
        scene_meshes = std::vector<aiMesh *>(mScene->mMeshes, mScene->mMeshes + mScene->mNumMeshes);
        mScene->mNumMeshes = 0;
        delete[] mScene->mMeshes;
        mScene->mMeshes = nullptr;
    }

    std::set<unsigned int> mesh_indices;
    auto const meshes = createMeshes(ri_instance_path);
    for (auto mesh : meshes) {
        mesh_indices.insert(static_cast<unsigned int>(scene_meshes.size()));
        scene_meshes.push_back(mesh);
    }
    mScene->mNumMeshes = static_cast<unsigned int>(scene_meshes.size());
    mScene->mMeshes = new aiMesh *[scene_meshes.size()];
    memcpy(mScene->mMeshes, scene_meshes.data(), sizeof(aiMesh *) * scene_meshes.size());

    StyledMesh styled_mesh;
    styled_mesh.mMeshIndices = mesh_indices;
    styled_mesh.mStyleData = ri_style;
    mExistingMeshes[ri].push_back(styled_mesh);
    
    return mesh_indices;
}

std::vector<aiMesh*> Assimp::ExchangeMeshFactory::createMeshes(ts3d::InstancePath const &ri_instance_path) {
    ts3d::RepresentationItemInstance ri_instance(ri_instance_path);
    auto const tess3d = std::dynamic_pointer_cast<ts3d::Tess3DInstance>(ri_instance.getTessellation());
    if (nullptr == tess3d) {
        return std::vector<aiMesh*>();
    }

    auto const ri_style_data = ri_instance.Instance::getNetStyle();
    std::vector<MeshData> mesh_data;
    for (auto topo_face_idx = 0u; topo_face_idx < tess3d->faceSize(); ++topo_face_idx) {
        if (!ri_instance.getNetShow(topo_face_idx) || ri_instance.getNetRemoved(topo_face_idx)) {
            continue;
        }
        auto const face_style_data = ri_instance.getNetStyle(topo_face_idx);        
        auto it = std::find_if(std::begin(mesh_data), std::end(mesh_data), [=](MeshData const &md) {
            return 0 == memcmp(&md.style_data, &face_style_data, sizeof(A3DGraphStyleData));
        });

        if (std::end(mesh_data) == it) {
            MeshData md;
            md.style_data = face_style_data;
            mesh_data.emplace_back(md);
            it = std::end(mesh_data) - 1;
        }

        addFace(tess3d, topo_face_idx, *it);
    }

    std::vector<aiMesh *> meshes;
    for (auto const &md : mesh_data) {
        auto mesh = new aiMesh();
        mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
        mesh->mNumVertices = static_cast<unsigned int>(md.assimp_vertices.size());
        mesh->mVertices = new aiVector3D[mesh->mNumVertices];
        memcpy(mesh->mVertices, md.assimp_vertices.data(), sizeof(aiVector3D) * mesh->mNumVertices);
        mesh->mNormals = new aiVector3D[mesh->mNumVertices];
        memcpy(mesh->mNormals, md.assimp_normals.data(), sizeof(aiVector3D) * mesh->mNumVertices);
        mesh->mNumFaces = static_cast<unsigned int>(md.assimp_faces.size());
        mesh->mFaces = new aiFace[mesh->mNumFaces];
        for (auto face_idx = 0u; face_idx < md.assimp_faces.size();  ++face_idx) {
            mesh->mFaces[face_idx] = md.assimp_faces[face_idx];
        }
        mesh->mMaterialIndex = mMaterialLoader->getMaterialIndex(md.style_data);  
        meshes.push_back(mesh);
    }

    return meshes;
}
