#include <A3DSDKIncludes.h>
#include <ExchangeToolkit.h>

#include "ExchangeLoader.hpp"
#include "ExchangeLoaderConfig.hpp"
#include "ExchangeMeshFactory.hpp"
#include "assimp/scene.h"

namespace {

    inline aiVector3D getVector(A3DVector3dData const &v) {
    return aiVector3D(static_cast<float>(v.m_dX), static_cast<float>(v.m_dY), static_cast<float>(v.m_dZ));
}

inline aiMatrix4x4 getMatrixFromCartesian(A3DMiscCartesianTransformation *xform) {
    ts3d::A3DMiscCartesianTransformationWrapper d(xform);
    auto const mirror = (d->m_ucBehaviour & kA3DTransformationMirror) ? -1.f : 1.f;
    auto const s = getVector(d->m_sScale);
    auto const o = getVector(d->m_sOrigin);
    auto const x = getVector(d->m_sXVector);
    auto const y = getVector(d->m_sYVector);
    auto const z = x ^ y * mirror;

    return aiMatrix4x4(
            x.x * s.x, y.x * s.x, z.x * s.x, o.x,
            x.y * s.y, y.y * s.y, z.y * s.y, o.y,
            x.z * s.z, y.z * s.z, z.z * s.z, o.z,
            .0f, 0.f, 0.f, 1.f);
}

inline aiMatrix4x4 getMatrixFromGeneralTransformation(A3DMiscGeneralTransformation *xform) {
    ts3d::A3DMiscGeneralTransformationWrapper d(xform);
    return aiMatrix4x4(
            static_cast<float>(d->m_adCoeff[0]), static_cast<float>(d->m_adCoeff[4]), static_cast<float>(d->m_adCoeff[8]), static_cast<float>(d->m_adCoeff[12]),
            static_cast<float>(d->m_adCoeff[1]), static_cast<float>(d->m_adCoeff[5]), static_cast<float>(d->m_adCoeff[9]), static_cast<float>(d->m_adCoeff[13]),
            static_cast<float>(d->m_adCoeff[2]), static_cast<float>(d->m_adCoeff[6]), static_cast<float>(d->m_adCoeff[10]), static_cast<float>(d->m_adCoeff[14]),
            static_cast<float>(d->m_adCoeff[3]), static_cast<float>(d->m_adCoeff[7]), static_cast<float>(d->m_adCoeff[11]), static_cast<float>(d->m_adCoeff[15]));
}

inline aiMatrix4x4 getMatrix(A3DEntity *ntt) {
    auto const t = ts3d::getEntityType(ntt);
    switch (t) {
    case kA3DTypeMiscCartesianTransformation:
        return getMatrixFromCartesian(ntt);
        break;
    case kA3DTypeMiscGeneralTransformation:
        return getMatrixFromGeneralTransformation(ntt);
        break;
    case kA3DTypeAsmProductOccurrence:
        return getMatrix(getLocation(ntt));
        break;
    default:
        if (isRepresentationItem(t)) {
            A3DRiRepresentationItemWrapper d(ntt);
            if (d->m_pCoordinateSystem) {
                A3DRiCoordinateSystemWrapper csw(d->m_pCoordinateSystem);
                return getMatrix(d->m_pCoordinateSystem) * getMatrix(csw->m_pTransformation);
            }
        }
        break;
    }
    return aiMatrix4x4(); // identity
}


bool populateMaterialMetadata(A3DEntity *entity, aiMetadata *md) {
    if (nullptr == entity || nullptr == md) {
        return false;
    }

    auto const entity_type = ts3d::getEntityType(entity);
    if (!ts3d::isRepresentationItem(entity_type) && kA3DTypeAsmProductOccurrence != entity_type) {
        return false;
    }

    A3DMiscMaterialPropertiesData material_property_data;
    A3D_INITIALIZE_DATA(A3DMiscMaterialPropertiesData, material_property_data);
    if (A3D_SUCCESS != A3DMiscGetMaterialProperties(entity, &material_property_data)) {
        return false;
    }

    auto found_metadata = false;
    if (material_property_data.m_pcMaterialName) {
        md->Add(Assimp::ExchangeLoader::METADATA_KEY_MATERIAL_NAME, material_property_data.m_pcMaterialName);
        found_metadata = true;
    }

    if (-1 != material_property_data.m_dDensity) {
        md->Add(Assimp::ExchangeLoader::METADATA_KEY_MATERIAL_DENSITY, material_property_data.m_dDensity);
        found_metadata = true;
    }

    A3DMiscGetMaterialProperties(nullptr, &material_property_data);

    return found_metadata;
}
void populateMetadataWithAttributeData(A3DMiscAttribute *attrib, aiMetadata *md) {
    if (nullptr == attrib || nullptr == md) {
        return;
    }

    ts3d::A3DMiscAttributeWrapper attrib_data(attrib);
    A3DUns32 as_int;
    A3DDouble as_double;
    std::stringstream ss;
    if (attrib_data->m_bTitleIsInt) {
        memcpy(&as_int, attrib_data->m_pcTitle, sizeof(A3DUns32));
        ss << as_int;
    } else {
        ss << attrib_data->m_pcTitle;
    }
    auto const attrib_title = ss.str();

    if (attrib_data->m_uiSize == 1 && nullptr == attrib_data->m_asSingleAttributesData[0].m_pcTitle) {
        // in many cases, the top level attribute object has a name, and one "single attrib data"
        // containing *no name*. In this case, I am attaching the attribute value from the one
        // "single attrib data" directly with the named top level attrib
        auto const &single_attrib_data = attrib_data->m_asSingleAttributesData[0];
        switch (single_attrib_data.m_eType) {
        case kA3DModellerAttributeTypeNull:
        default:
            break;
        case kA3DModellerAttributeTypeInt:
        case kA3DModellerAttributeTypeTime:
            memcpy(&as_int, single_attrib_data.m_pcData, sizeof(A3DUns32));
            md->Add(attrib_title, static_cast<uint64_t>(as_int));
            break;
        case kA3DModellerAttributeTypeReal:
            memcpy(&as_double, single_attrib_data.m_pcData, sizeof(A3DDouble));
            md->Add(attrib_title, as_double);
            break;
        case kA3DModellerAttributeTypeString:
            md->Add(attrib_title, single_attrib_data.m_pcData);
            break;
        }
    } else {
        auto *single_attribute_md = new aiMetadata();
        for (auto idx = 0u; idx < attrib_data->m_uiSize; ++idx) {
            auto const &single_attrib_data = attrib_data->m_asSingleAttributesData[idx];
            ss.clear();
            if (single_attrib_data.m_bTitleIsInt) {
                memcpy(&as_int, single_attrib_data.m_pcTitle, sizeof(A3DUns32));
                ss << as_int;
            } else {
                ss << single_attrib_data.m_pcTitle;
            }
            auto const single_attrib_title = ss.str();
            switch (single_attrib_data.m_eType) {
            case kA3DModellerAttributeTypeNull:
            default:
                break;
            case kA3DModellerAttributeTypeInt:
            case kA3DModellerAttributeTypeTime:
                memcpy(&as_int, single_attrib_data.m_pcData, sizeof(A3DUns32));
                single_attribute_md->Add(single_attrib_title, static_cast<uint64_t>(as_int));
                break;
            case kA3DModellerAttributeTypeReal:
                memcpy(&as_double, single_attrib_data.m_pcData, sizeof(A3DDouble));
                single_attribute_md->Add(single_attrib_title, as_double);
                break;
            case kA3DModellerAttributeTypeString:
                single_attribute_md->Add(single_attrib_title, single_attrib_data.m_pcData);
                break;
            }
        }
        md->Add(attrib_title, single_attribute_md);
    }
    return;
}

void populateMetadata(A3DEntity *entity, aiNode *n) {
    if (nullptr == entity || nullptr == n) {
        return;
    }

    auto md_created = false;
    if (nullptr == n->mMetaData) {
        n->mMetaData = new aiMetadata();
        md_created = true;
    }
    auto md = n->mMetaData;

    auto material_md = new aiMetadata();
    if (populateMaterialMetadata(entity, material_md)) {
        md->Add(Assimp::ExchangeLoader::METADATA_KEY_MATERIAL, material_md);
    } else {
        delete material_md;
        material_md = nullptr;
    }

    ts3d::A3DRootBaseWrapper root_base_data(entity);
    for (auto attrib : ts3d::toVector(root_base_data->m_ppAttributes, root_base_data->m_uiSize)) {
        populateMetadataWithAttributeData(attrib, md);
    }

    if (md_created && 0 == md->mNumProperties) {
        delete md;
        md = n->mMetaData = nullptr;
    }
}


aiNode *createRepresentationItemNode(InstancePath instance_path, std::shared_ptr<Assimp::ExchangeMeshFactory> const &mesh_factory) {
    if (instance_path.empty() || !ts3d::isRepresentationItem(ts3d::getEntityType(instance_path.back()))) {
        return nullptr;
    }

    auto const rep_item = instance_path.back();
    aiNode *node = new aiNode();
    node->mName.Set(ts3d::Instance(instance_path).getName());
    populateMetadata(rep_item, node);
    node->mTransformation = getMatrix(rep_item);

    auto const ri_type = ts3d::getEntityType(rep_item);
    if (kA3DTypeRiBrepModel == ri_type || kA3DTypeRiPolyBrepModel == ri_type) {
        auto const mesh_indices = mesh_factory->getMeshIndices(instance_path);
        node->mNumMeshes = static_cast<unsigned int>(mesh_indices.size());
        node->mMeshes = new unsigned int[node->mNumMeshes];
        auto idx = 0u;
        for(auto mesh_index : mesh_indices) {
            node->mMeshes[idx++] = mesh_index;
        }
    } else if (kA3DTypeRiSet == ri_type) {
        std::vector<aiNode *> ri_nodes;
        auto const rep_items = ts3d::getChildren(rep_item, kA3DTypeRiRepresentationItem);
        for (auto const sub_rep_item : rep_items) {
            instance_path.push_back(sub_rep_item);
            ri_nodes.push_back(createRepresentationItemNode(instance_path, mesh_factory));
            instance_path.pop_back();
        }
        node->addChildren(static_cast<unsigned int>(ri_nodes.size()), ri_nodes.data());
    }
    return node;
}

aiNode *createPartDefinitionNode(ts3d::InstancePath instance_path, std::shared_ptr<Assimp::ExchangeMeshFactory> const &mesh_factory) {
    std::vector<aiNode *> ri_nodes;
    auto const rep_items = ts3d::getChildren(instance_path.back(), kA3DTypeRiRepresentationItem);
    for (auto const rep_item : rep_items) {
        instance_path.push_back(rep_item);
        ts3d::Instance ri_instance(instance_path);
        if (ri_instance.getNetShow() && !ri_instance.getNetRemoved()) {
            ri_nodes.push_back(createRepresentationItemNode(instance_path, mesh_factory));
        }
        instance_path.pop_back();
    }

    auto part_node = new aiNode();
    part_node->mName.Set(ts3d::Instance(instance_path).getName());
    populateMetadata(instance_path.back(), part_node);
    part_node->addChildren(static_cast<unsigned int>(ri_nodes.size()), ri_nodes.data());
    return part_node;
}




aiNode *createProductOccurrenceNode(ts3d::InstancePath instance_path, std::shared_ptr<Assimp::ExchangeMeshFactory> const &mesh_factory ) {
    if (instance_path.empty() || nullptr == instance_path.back()) {
        return nullptr;
    }
    auto const po = instance_path.back();

    aiNode *this_node = new aiNode();
    this_node->mName.Set(ts3d::Instance({ po }).getName());
    this_node->mTransformation = getMatrix(po);
    populateMetadata(po, this_node);

    std::vector<aiNode *> child_nodes;
    if (auto const part_def = ts3d::getPartDefinition(po)) {
        instance_path.push_back(part_def);
        if (auto part_def_node = createPartDefinitionNode(instance_path, mesh_factory)) {
            child_nodes.push_back(part_def_node);
        }
        instance_path.pop_back();
    }

    for (auto child_po : ts3d::getChildren(po, kA3DTypeAsmProductOccurrence)) {
        instance_path.push_back(child_po);
        child_nodes.push_back(createProductOccurrenceNode(instance_path, mesh_factory));
        instance_path.pop_back();
    }
    this_node->addChildren(static_cast<unsigned int>(child_nodes.size()), child_nodes.data());
    return this_node;
}

}

std::string const Assimp::ExchangeLoader::METADATA_KEY_MATERIAL = "METADATA_KEY_MATERIAL";
std::string const Assimp::ExchangeLoader::METADATA_KEY_MATERIAL_DENSITY = "METADATA_KEY_MATERIAL_DENSITY";
std::string const Assimp::ExchangeLoader::METADATA_KEY_MATERIAL_NAME = "METADATA_KEY_MATERIAL_NAME";
std::string const Assimp::ExchangeLoader::METADATA_KEY_UNIT_FACTOR = "METADATA_KEY_UNIT_FACTOR";
std::string const Assimp::ExchangeLoader::METADATA_KEY_FORMAT = "METADATA_KEY_FORMAT"; // value - one of A3DEModellerType

Assimp::ExchangeLoader::ExchangeLoader() {
}

Assimp::ExchangeLoader::~ExchangeLoader() {

}

bool Assimp::ExchangeLoader::CanRead(const std::string& pFile, IOSystem*, bool) const {
    auto const extension = GetExtension(pFile);
    auto const &enabled_extensions = ExchangeLoaderConfig::instance().GetFileExtensionSet();
    if (std::end(enabled_extensions) != enabled_extensions.find(extension)) {
        return true;
    }

    return false;
}
const aiImporterDesc* Assimp::ExchangeLoader::GetInfo() const {
    static const aiImporterDesc desc = {
        "HOOPS Exchange Multi-Format Importer",
        "",
        "",
        "",
        aiImporterFlags_SupportTextFlavour | aiImporterFlags_SupportBinaryFlavour,
        0,
        0,
        0,
        0,
        "prc"
    };
    return &desc;
}

void Assimp::ExchangeLoader::InternReadFile(const std::string& pFile, aiScene* pScene, IOSystem*) {

    // Implicit Exchange Initialization
    // This block handles the case where the consumer of the Exchange Assimp Loader failed to
    // previously make an explicit call to ExchangeLoaderConfig::instance().Initialize()
    {
        if (ExchangeLoaderConfig::instance().GetInitializationStatus() == ExchangeLoaderConfig::InitializeStatus::FAILURE_NOT_INITIALIZED) {
            ExchangeLoaderConfig::instance().Initialize();
        }

        auto const initialize_status = ExchangeLoaderConfig::instance().GetInitializationStatus();
        if (initialize_status != ExchangeLoaderConfig::InitializeStatus::OK) {
            switch (initialize_status) {
            case ExchangeLoaderConfig::InitializeStatus::FAILURE_LIBRARY_VERSION_MISMATCH:
                throw DeadlyImportError("Library version mismatch");
                break;
            case ExchangeLoaderConfig::InitializeStatus::FAILURE_LICENSE_INVALID:
                throw DeadlyImportError("Invalid license");
                break;
            case ExchangeLoaderConfig::InitializeStatus::FAILURE_LOAD_LIBRARIES:
                throw DeadlyImportError("Libraries failed to load");
                break;
            default:
                break;
            }
        }
    }
    // End of implicit initialization

    // Load the requested file
    A3DAsmModelFile *model_file = nullptr;
    auto const load_status = A3DAsmModelFileLoadFromFile(pFile.c_str(), &ExchangeLoaderConfig::instance().GetLoadParams(), &model_file);
    if (A3D_SUCCESS != load_status && A3D_LOAD_MISSING_COMPONENTS != load_status) {
        std::stringstream ss;
        ss << "Failure code: (" << load_status << ") - \"" << A3DMiscGetErrorMsg(load_status) << "\"";
        throw DeadlyImportError(ss.str());
    }

    pScene->mRootNode = new aiNode();
    pScene->mRootNode->mName.Set(ts3d::A3DRootBaseWrapper(model_file)->m_pcName);

    ts3d::A3DAsmModelFileWrapper model_file_data(model_file);
    populateMetadata(model_file, pScene->mRootNode);
    aiMetadata *md = pScene->mRootNode->mMetaData;
    if (nullptr == md) {
        md = pScene->mRootNode->mMetaData = new aiMetadata;
    }
    auto const unit_factor = ts3d::getUnit(model_file);
    md->Add(METADATA_KEY_UNIT_FACTOR, unit_factor);
    md->Add(METADATA_KEY_FORMAT, model_file_data->m_eModellerType);

    InstancePath instance_path({ model_file });
    auto mesh_factory = std::make_shared<ExchangeMeshFactory>(pScene);
    std::vector<aiNode *> child_nodes;
    for(auto po : ts3d::getChildren(model_file, kA3DTypeAsmProductOccurrence)) {
        instance_path.push_back(po);
        child_nodes.push_back(createProductOccurrenceNode(instance_path, mesh_factory));
        instance_path.pop_back();
    }
    pScene->mRootNode->addChildren(static_cast<unsigned int>(child_nodes.size()), child_nodes.data());
}
