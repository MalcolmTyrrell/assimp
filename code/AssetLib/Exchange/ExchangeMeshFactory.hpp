
#ifndef AI_EXCHANGEMESHFACTORY_H_INCLUDED
#define AI_EXCHANGEMESHFACTORY_H_INCLUDED

#include <assimp/BaseImporter.h>
#include <assimp/types.h>
#include <assimp/scene.h>
#include <unordered_map>

#include <iostream>
#include <set>
#include "A3DSDKIncludes.h"
#include "ExchangeToolkit.h"
#include "ExchangeMaterialLoader.hpp"

namespace Assimp {

class ExchangeMeshFactory {
public:
    ExchangeMeshFactory(aiScene *pScene);

    // Returns the set of mesh index values required to accurately represent a representation item.
    std::set<unsigned int> getMeshIndices(ts3d::InstancePath const &ri_instance_path);

private:

    std::vector<aiMesh*> createMeshes(ts3d::InstancePath const &ri_instance_path);
    //aiMesh *createMesh(A3DRiRepresentationItem *rep_item, unsigned int topo_face_idx);

    aiScene *mScene;
    std::shared_ptr<ExchangeMaterialLoader> mMaterialLoader;

    struct StyledMesh {
        std::set<unsigned int> mMeshIndices;
        A3DGraphStyleData mStyleData;
    };
    std::unordered_map<A3DRiRepresentationItem *, std::vector<StyledMesh>> mExistingMeshes;
};

} // end of namespace Assimp

#endif // AI_EXCHANGEMESHFACTORY_H_INCLUDED
