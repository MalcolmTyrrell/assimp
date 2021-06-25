
#ifndef AI_EXCHANGEMATERIALLOADER_H_INCLUDED
#define AI_EXCHANGEMATERIALLOADER_H_INCLUDED

#include <assimp/BaseImporter.h>
#include <assimp/types.h>
#include <unordered_map>
#include "A3DSDKIncludes.h"

namespace Assimp {

class ExchangeMaterialLoader {
public:
    ExchangeMaterialLoader(aiScene *pScene);

    int getMaterialIndex(A3DGraphStyleData const &style_data);
    //static bool isDefaultMaterial(A3DGraphStyleData const &style_data);

    static aiColor3D DEFAULT_COLOR;

    using ExchangeIndexToAssimpIndexMap = std::unordered_map<unsigned int, unsigned int>;
private:
    aiScene *mScene;
    ExchangeIndexToAssimpIndexMap mTextureMaterialMap;
    ExchangeIndexToAssimpIndexMap mPlainMaterialMap;
    ExchangeIndexToAssimpIndexMap mRgbMaterialMap;
};

} // end of namespace Assimp

#endif // AI_EXCHANGEMATERIALLOADER_H_INCLUDED
