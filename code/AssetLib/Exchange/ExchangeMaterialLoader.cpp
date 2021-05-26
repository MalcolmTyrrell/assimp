#include "ExchangeMaterialLoader.hpp"
#include "ExchangeLoaderConfig.hpp"
#include <A3DSDKIncludes.h>
#include <ExchangeToolkit.h>

#include <cassert>
#include <fstream>
#include <sstream>
#include "assimp/scene.h"

aiColor3D Assimp::ExchangeMaterialLoader::DEFAULT_COLOR(1.0, 0.0, 0.0);

namespace {

A3DGraphRgbColorData getGlobalColorByIndex(A3DUns32 const &idx) {
    A3DGraphRgbColorData rgb_data;
    A3D_INITIALIZE_DATA(A3DGraphRgbColorData, rgb_data);
    CheckResult(A3DGlobalGetGraphRgbColorData(idx, &rgb_data));
    return rgb_data;
}

aiColor4D toColor4D(A3DGraphRgbColorData const &rgb, double const &a) {
    return aiColor4D(static_cast<float>(rgb.m_dRed), static_cast<float>(rgb.m_dGreen), static_cast<float>(rgb.m_dBlue), static_cast<float>(a));
}

void populateAssimpMaterial(aiMaterial *m, A3DGraphTextureDefinitionData const &texture_def_data, int const texture_n) {
    if (nullptr == m) {
        return;
    }

    aiTextureType texture_type = aiTextureType_NONE;
    auto const mapping_attributes = texture_def_data.m_uiMappingAttributes;
    if (mapping_attributes & kA3DTextureMappingDiffuse) {
        texture_type = aiTextureType_DIFFUSE;
    }
    if (mapping_attributes & kA3DTextureMappingSphericalReflection) {
        // unsupported
    }
    if (mapping_attributes & kA3DTextureMappingCubicalReflection) {
        // unsuppported 
    }
    if (mapping_attributes & kA3DTextureMappingNormal) {
        texture_type = aiTextureType_NORMALS;
    }
    if (mapping_attributes & kA3DTextureMappingMetalness) {
        texture_type = aiTextureType_METALNESS;
    }
    if (mapping_attributes & kA3DTextureMappingRoughness) {
        texture_type = aiTextureType_DIFFUSE_ROUGHNESS;
    }
    if (mapping_attributes & kA3DTextureMappingOcclusion) {
        texture_type = aiTextureType_AMBIENT_OCCLUSION;
    }

    ts3d::A3DMiscCartesianTransformationWrapper cart_d(texture_def_data.m_pOperatorTransfo);
    ts3d::A3DGraphTextureTransformationWrapper tex_transf_d(texture_def_data.m_pTextureTransfo);
    auto const alpha_test_reference = texture_def_data.m_dAlphaTestReference;


    auto mapping_mode = aiTextureMapping_UV;
    switch (texture_def_data.m_eMappingOperator) {
    default:
    case kA3DTextureMappingOperatorUnknown:
        break;
    case kA3DTextureMappingOperatorPlanar:
        mapping_mode = aiTextureMapping_PLANE;
        break;
    case kA3DTextureMappingOperatorCylindrical:
        mapping_mode = aiTextureMapping_CYLINDER;
        break;
    case kA3DTextureMappingOperatorSpherical:
        mapping_mode = aiTextureMapping_SPHERE;
        break;
    case kA3DTextureMappingOperatorCubical:
        mapping_mode = aiTextureMapping_BOX;
        break;
    }
    m->AddProperty(&mapping_mode, 1, AI_MATKEY_MAPPING(texture_type, texture_n));

    auto wrap_s = aiTextureMapMode_Wrap;
    switch (texture_def_data.m_eTextureWrappingModeS) {
    default:
    case kA3DTextureWrappingModeUnknown:
    case kA3DTextureWrappingModeRepeat:
        wrap_s = aiTextureMapMode_Wrap;
        break;
    case kA3DTextureWrappingModeClampToBorder:
        wrap_s = aiTextureMapMode_Clamp;
        break;
    }
    m->AddProperty(&wrap_s, 1, AI_MATKEY_MAPPINGMODE_U(texture_type, texture_n));

    auto wrap_t = aiTextureMapMode_Wrap;
    switch (texture_def_data.m_eTextureWrappingModeT) {
    default:
    case kA3DTextureWrappingModeUnknown:
    case kA3DTextureWrappingModeRepeat:
        wrap_t = aiTextureMapMode_Wrap;
        break;
    case kA3DTextureWrappingModeClampToBorder:
        wrap_t = aiTextureMapMode_Clamp;
        break;
    }
    m->AddProperty(&wrap_t, 1, AI_MATKEY_MAPPINGMODE_V(texture_type, texture_n));

    auto const applying_mode = texture_def_data.m_ucTextureApplyingMode;
    if (applying_mode & kA3DTextureApplyingModeAlphaTest) {
        // Alpha test enabled
        aiTextureFlags f = aiTextureFlags_UseAlpha;
        m->AddProperty(&f, 1, AI_MATKEY_TEXFLAGS(texture_type, texture_n));
    }
    if (applying_mode & kA3DTextureApplyingModeLighting) {
        // Lighting enabled
    }
    if (applying_mode & kA3DTextureApplyingModeNone) {
        // All states disabled
    }
    if (applying_mode & kA3DTextureApplyingModeVertexColor) {
        // Use vertex color (combine texture with one-color-per-vertex)
    }

    if (texture_def_data.m_uiMappingAttributesIntensitySize) {
        auto const mapping_attributes_intensity = ts3d::toVector(texture_def_data.m_pdMappingAttributesIntensity, texture_def_data.m_uiMappingAttributesIntensitySize);
        m->AddProperty(mapping_attributes_intensity.data(), static_cast<unsigned int>(mapping_attributes_intensity.size()), "$mat.MappingAttributesIntensity");
    }

    if (texture_def_data.m_uiMappingAttributesComponentsSize) {
        auto const mapping_attributes_components = ts3d::toVector(texture_def_data.m_pucMappingAttributesComponents, texture_def_data.m_uiMappingAttributesComponentsSize);
        m->AddProperty(mapping_attributes_components.data(), static_cast<unsigned int>(mapping_attributes_components.size()), "$mat.MappingAttributesComponents");
    }

    if (texture_def_data.m_pTextureTransfo) {
        ts3d::A3DGraphTextureTransformationWrapper tex_xform_data(texture_def_data.m_pTextureTransfo);
        if (tex_xform_data->m_bIs2D) {
            auto const &matrix = tex_xform_data->m_dMatrix;
            aiUVTransform uv_xform;
            uv_xform.mTranslation.x = static_cast<float>(matrix[3]);
            uv_xform.mTranslation.y = static_cast<float>(matrix[7]);
            
            uv_xform.mScaling.x = static_cast<float>(sqrt(matrix[0] * matrix[0] + matrix[4] * matrix[4] + matrix[8] * matrix[8]));
            uv_xform.mScaling.y = static_cast<float>(sqrt(matrix[1] * matrix[1] + matrix[5] * matrix[5] + matrix[9] * matrix[9]));

            uv_xform.mRotation = static_cast<float>(atan2(matrix[4], matrix[0]));

            // unused -- how to encode this?
            tex_xform_data->m_bTextureFlipS;
            tex_xform_data->m_bTextureFlipT;

            m->AddProperty(&uv_xform, 1, AI_MATKEY_UVTRANSFORM(texture_type, texture_n));
        }
    }

    if (texture_def_data.m_pOperatorTransfo) {
        // unused
    }

    A3DGraphPictureData picture_data;
    A3D_INITIALIZE_DATA(A3DGraphPictureData, picture_data);
    if (!CheckResult(A3DGlobalGetGraphPictureData(texture_def_data.m_uiPictureIndex, &picture_data))) {
        return;
    }

    std::string extension;
    switch (picture_data.m_eFormat) {
    case kA3DPicturePng:
        extension = "png";
        break;
    case kA3DPictureJpg:
        extension = "jpg";
        break;
    case kA3DPictureBmp:
        extension = "bmp";
        break;
    case kA3DPictureBitmapRgbByte:
        extension = "rgb";
        break;
    case kA3DPictureBitmapRgbaByte:
        extension = "rgba";
        break;
    case kA3DPictureBitmapGreyByte:
        extension = "grey";
        break;
    case kA3DPictureBitmapGreyaByte:
        extension = "greya";
        break;
    }

    std::stringstream fn_stream;
    fn_stream << Assimp::ExchangeLoaderConfig::instance().GetTextureFolder();
#ifdef _MSC_VER
    fn_stream << "\\";
#else
    fn_stream << "/";
#endif
    fn_stream << "texture" << texture_def_data.m_uiPictureIndex << "." << extension;

    if (auto fptr = fopen(fn_stream.str().c_str(), "wb")) {
        fwrite(picture_data.m_pucBinaryData, sizeof(A3DUns8), picture_data.m_uiSize, fptr);
        fclose(fptr);
    }
    aiString s;
    s.Set(fn_stream.str());
    m->AddProperty(&s, AI_MATKEY_TEXTURE(texture_type, texture_n));
}

void populateAssimpMaterial(aiMaterial *m, A3DGraphMaterialData const &material_data) {
    if (nullptr == m) {
        return;
    }

    auto const ambient = getGlobalColorByIndex(material_data.m_uiAmbient);
    auto const ambient_alpha = material_data.m_dAmbientAlpha;
    auto const ambient4d = toColor4D(ambient, ambient_alpha);
    m->AddProperty(&ambient4d, 1, AI_MATKEY_COLOR_AMBIENT);

    auto const diffuse = getGlobalColorByIndex(material_data.m_uiDiffuse);
    auto const diffuse_alpha = material_data.m_dDiffuseAlpha;
    auto const diffuse4d = toColor4D(diffuse, diffuse_alpha);
    m->AddProperty(&diffuse4d, 1, AI_MATKEY_COLOR_DIFFUSE);

    auto const emissive = getGlobalColorByIndex(material_data.m_uiEmissive);
    auto const emissive_alpha = material_data.m_dEmissiveAlpha;
    auto const emissive4d = toColor4D(emissive, emissive_alpha);
    m->AddProperty(&emissive4d, 1, AI_MATKEY_COLOR_EMISSIVE);

    auto const specular = getGlobalColorByIndex(material_data.m_uiSpecular);
    auto const specular_alpha = material_data.m_dSpecularAlpha;
    auto const specular4d = toColor4D(specular, specular_alpha);
    m->AddProperty(&specular4d, 1, AI_MATKEY_COLOR_SPECULAR);

    auto const shininess = material_data.m_dShininess;
    m->AddProperty(&shininess, 1, AI_MATKEY_SHININESS);
}

void populateAssimpMaterial(aiMaterial *m, A3DGraphTextureApplicationData const &texture_app_data, int texture_n) {
    if (nullptr == m) {
        return;
    }

    A3DGraphTextureDefinitionData texture_def_data;
    A3D_INITIALIZE_DATA(A3DGraphTextureDefinitionData, texture_def_data);
    if (!CheckResult(A3DGlobalGetGraphTextureDefinitionData(texture_app_data.m_uiTextureDefinitionIndex, &texture_def_data))) {
        return;
    }
    populateAssimpMaterial(m, texture_def_data, texture_n);

    A3DGraphMaterialData material_data;
    A3D_INITIALIZE_DATA(A3DGraphMaterialData, material_data);
    if (!CheckResult(A3DGlobalGetGraphMaterialData(texture_app_data.m_uiMaterialIndex, &material_data))) {
        return;
    }
    populateAssimpMaterial(m, material_data);

    A3DEntity *material = nullptr;
    if (!CheckResult(A3DMiscPointerFromIndexGet(texture_app_data.m_uiMaterialIndex, kA3DTypeGraphMaterial, &material))) {
        return;
    }

    if (material) {
        ts3d::A3DRootBaseWrapper root_base_d(material);
        if (root_base_d->m_pcName) {
            aiString material_name(root_base_d->m_pcName);
            m->AddProperty(&material_name, AI_MATKEY_NAME);
        }

        auto attributes = ts3d::toVector(root_base_d->m_ppAttributes, root_base_d->m_uiSize);
        for (auto attribute : attributes) {
            ts3d::A3DMiscAttributeWrapper attrib_d(attribute);
            auto const title = attrib_d->m_pcTitle ? std::string(attrib_d->m_pcTitle) : std::string();
            auto const single_attribs = ts3d::toVector(attrib_d->m_asSingleAttributesData, attrib_d->m_uiSize);
            for (auto const &single_attrib : single_attribs) {
                switch (single_attrib.m_eType) {
                case kA3DModellerAttributeTypeInt:
                    if (title == kA3DPBRAttributeNameAlphaMode) {
                        auto const alpha_mode = *reinterpret_cast<A3DInt32 *>(single_attrib.m_pcData);
                        m->AddProperty(&alpha_mode, 1, "$mat.AlphaMode");
                    }
                    break;
                case kA3DModellerAttributeTypeReal:
                    if (title == kA3DPBRAttributeNameAlphaMode) {
                        auto const alpha_cut_off = *reinterpret_cast<A3DDouble *>(single_attrib.m_pcData);
                        m->AddProperty(&alpha_cut_off, 1, "$mat.AlphaCutOff");
                    } else if (title == kA3DPBRAttributeNameMetallicFactor) {
                        auto const metallic_factor = *reinterpret_cast<A3DDouble *>(single_attrib.m_pcData);
                        m->AddProperty(&metallic_factor, 1, "$mat.MetallicFactor");
                    } else if (title == kA3DPBRAttributeNameNormalTextureFactor) {
                        auto const normal_texture_factor = *reinterpret_cast<A3DDouble *>(single_attrib.m_pcData);
                        m->AddProperty(&normal_texture_factor, 1, "$mat.NormalTextureFactor");
                    } else if (title == kA3DPBRAttributeNameOcclusionTextureFactor) {
                        auto const occlusion_texture_factor = *reinterpret_cast<A3DDouble *>(single_attrib.m_pcData);
                        m->AddProperty(&occlusion_texture_factor, 1, "$mat.OcclusionTextureFactor");
                    } else if (title == kA3DPBRAttributeNameRoughnessFactor) {
                        auto const roughness_factor = *reinterpret_cast<A3DDouble *>(single_attrib.m_pcData);
                        m->AddProperty(&roughness_factor, 1, "$mat.RoughnessFactor");
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    if (A3D_DEFAULT_MATERIAL_INDEX != texture_app_data.m_uiNextTextureApplicationIndex) {
        A3DGraphTextureApplicationData next_texture_app_data;
        A3D_INITIALIZE_DATA(A3DGraphTextureApplicationData, next_texture_app_data);
        if (!CheckResult(A3DGlobalGetGraphTextureApplicationData(texture_app_data.m_uiNextTextureApplicationIndex, &next_texture_app_data))) {
            return;
        }
        populateAssimpMaterial(m, next_texture_app_data, texture_n + 1);
        A3DGlobalGetGraphTextureApplicationData(A3D_DEFAULT_MATERIAL_INDEX, &next_texture_app_data);
    }
}

aiMaterial *computeAssimpMaterial(A3DGraphStyleData const &style_data) {
    aiMaterial *m = new aiMaterial();
    if (style_data.m_bMaterial) {
        A3DBool is_texture = false;
        if (style_data.m_bMaterial) {
            A3DGlobalIsMaterialTexture(style_data.m_uiRgbColorIndex, &is_texture);
        }
        if (is_texture) {
            A3DGraphTextureApplicationData texture_app_data;
            A3D_INITIALIZE_DATA(A3DGraphTextureApplicationData, texture_app_data);
            if (!CheckResult(A3DGlobalGetGraphTextureApplicationData(style_data.m_uiRgbColorIndex, &texture_app_data))) {
                delete m;
                return nullptr;
            }
            populateAssimpMaterial(m, texture_app_data, 0);
            A3DGlobalGetGraphTextureApplicationData(A3D_DEFAULT_MATERIAL_INDEX, &texture_app_data);
        } else {
            A3DGraphMaterialData material_data;
            A3D_INITIALIZE_DATA(A3DGraphMaterialData, material_data);
            if (!CheckResult(A3DGlobalGetGraphMaterialData(style_data.m_uiRgbColorIndex, &material_data))) {
                delete m;
                return nullptr;
            }
            populateAssimpMaterial(m, material_data);
            A3DGlobalGetGraphMaterialData(A3D_DEFAULT_MATERIAL_INDEX, &material_data);
        }
    } else {
        aiColor3D rgb;
        A3DGraphRgbColorData color_data;
        A3D_INITIALIZE_DATA(A3DGraphRgbColorData, color_data);
        if (A3D_DEFAULT_COLOR_INDEX == style_data.m_uiRgbColorIndex) {
            // default color
            rgb = Assimp::ExchangeMaterialLoader::DEFAULT_COLOR;
        } else if (CheckResult(A3DGlobalGetGraphRgbColorData(style_data.m_uiRgbColorIndex, &color_data))) {
            rgb.r = static_cast<float>(color_data.m_dRed);
            rgb.b = static_cast<float>(color_data.m_dBlue);
            rgb.g = static_cast<float>(color_data.m_dGreen);
        }
        m->AddProperty(&rgb, 1, AI_MATKEY_COLOR_DIFFUSE);
    }

    if (style_data.m_bVPicture) {
        A3DGraphVPicturePatternData picture_pattern_data;
        A3D_INITIALIZE_DATA(A3DGraphVPicturePatternData, picture_pattern_data);
        if (CheckResult(A3DGlobalGetGraphVPicturePatternData(style_data.m_uiLinePatternIndex, &picture_pattern_data))) {
        }
    } else {
        A3DGraphLinePatternData line_pattern_data;
        A3D_INITIALIZE_DATA(A3DGraphLinePatternData, line_pattern_data);
        if (CheckResult(A3DGlobalGetGraphLinePatternData(style_data.m_uiLinePatternIndex, &line_pattern_data))) {
        }
    }

    if (!style_data.m_bBackCulling && !style_data.m_bFrontCulling) {
        bool double_sided = true;
        m->AddProperty(&double_sided, 1, AI_MATKEY_TWOSIDED);
    }

    if (style_data.m_bIsTransparencyDefined) {
        auto const opacity = static_cast<float>(style_data.m_ucTransparency)/255.0f;
        m->AddProperty(&opacity, 1, AI_MATKEY_OPACITY);
    }

    //j = json{
    //    { "no_light", static_cast<bool>(style_data.m_bNoLight) },
    //    { "special_culling", static_cast<bool>(style_data.m_bSpecialCulling) },
    //    { "v_picture", static_cast<bool>(style_data.m_bVPicture) },
    //    { "width", style_data.m_dWidth },
    //    { "render_mode", style_data.m_eRenderingMode },
    //};
    return m;
}
} // namespace

Assimp::ExchangeMaterialLoader::ExchangeMaterialLoader(aiScene* pScene) : mScene(pScene) {

}

namespace {
unsigned int findOrCreateMaterial(A3DGraphStyleData const &style_data, Assimp::ExchangeMaterialLoader::ExchangeIndexToAssimpIndexMap &map, aiScene *pScene) {
    auto it = map.find(style_data.m_uiRgbColorIndex);
    if (std::end(map) != it) {
        return it->second;
    }

    auto new_material = computeAssimpMaterial(style_data);
    auto old_materials = pScene->mMaterials;
    pScene->mMaterials = new aiMaterial *[pScene->mNumMaterials + 1];
    if (old_materials) {
        memcpy(pScene->mMaterials, old_materials, sizeof(aiMaterial *) * pScene->mNumMaterials);
        delete[] old_materials;
        old_materials = nullptr;
    }
    pScene->mMaterials[pScene->mNumMaterials] = new_material;
    map[style_data.m_uiRgbColorIndex] = pScene->mNumMaterials;
    return pScene->mNumMaterials++;
}
}

//bool Assimp::ExchangeMaterialLoader::isDefaultMaterial(A3DGraphStyleData const& style_data) {
//    if (style_data.m_bMaterial) {
//        return false;
//    }
//
//    return A3D_DEFAULT_COLOR_INDEX == style_data.m_uiRgbColorIndex;
//}


int Assimp::ExchangeMaterialLoader::getMaterialIndex(A3DGraphStyleData const& style_data) {
    if (style_data.m_bMaterial) {
        A3DBool is_texture = false;
        if (style_data.m_bMaterial) {
            A3DGlobalIsMaterialTexture(style_data.m_uiRgbColorIndex, &is_texture);
        }

        if (is_texture) {
            // interpret style_data.m_uiRgbColorIndex as texture material lookup
            return findOrCreateMaterial(style_data, mTextureMaterialMap, mScene);
        } else {
            // interpret style_data.m_uiRgbColorIndex as plain material lookup
            return findOrCreateMaterial(style_data, mPlainMaterialMap, mScene);
        }
    }

    // interpret style_data.m_uiRgbColorIndex as rgb material lookup
    return findOrCreateMaterial(style_data, mRgbMaterialMap, mScene);
}
