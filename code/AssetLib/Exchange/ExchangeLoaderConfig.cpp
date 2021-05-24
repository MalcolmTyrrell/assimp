#define INITIALIZE_A3D_API
#include "ExchangeLoaderConfig.hpp"

// static
Assimp::ExchangeLoaderConfig& Assimp::ExchangeLoaderConfig::instance(void) {
    static ExchangeLoaderConfig _instance;
    return _instance;
}

Assimp::ExchangeLoaderConfig::ExchangeLoaderConfig(void)
 : mInitializeStatus(InitializeStatus::FAILURE_NOT_INITIALIZED),
    mLoadParameters(GetDefaultLoadParams()) {
    mTextureFolder = getenv("TMP");
    if (mTextureFolder.empty()) {
        mTextureFolder = getenv("TEMP");
    }
    if (mTextureFolder.empty()) {
        mTextureFolder = getenv("TEMPDIR");
    }
    if (mTextureFolder.empty()) {
        mTextureFolder = getenv("TMPDIR");
    }
}


// static
Assimp::ExchangeLoaderConfig::InitializeStatus Assimp::ExchangeLoaderConfig::Initialize(std::string const &hoops_license) {
    auto const loaded = A3DSDKLoadLibraryA(mExchangeLibraryFolder.c_str());
    if (!loaded) {
        return mInitializeStatus = InitializeStatus::FAILURE_LOAD_LIBRARIES;
    }

    auto const lic_status = A3DLicPutUnifiedLicense(hoops_license.c_str());
    if (A3D_SUCCESS != lic_status) {
        A3DSDKUnloadLibrary();
        return mInitializeStatus = InitializeStatus::FAILURE_LICENSE_INVALID;
    }

    auto const version_status = A3DDllInitialize(A3D_DLL_MAJORVERSION, A3D_DLL_MINORVERSION);
    if (A3D_SUCCESS != version_status) {
        A3DSDKUnloadLibrary();
        return mInitializeStatus = InitializeStatus::FAILURE_LIBRARY_VERSION_MISMATCH;
    }

    return mInitializeStatus = InitializeStatus::OK;
}

Assimp::ExchangeLoaderConfig::InitializeStatus Assimp::ExchangeLoaderConfig::GetInitializationStatus(void) const {
    return mInitializeStatus;
}

// static
A3DRWParamsLoadData Assimp::ExchangeLoaderConfig::GetDefaultLoadParams(void) {
    static A3DRWParamsLoadData default_load_params;
    static auto initialized = false;
    if (!initialized) {
        initialized = true;
        A3D_INITIALIZE_DATA(A3DRWParamsLoadData, default_load_params);
        default_load_params.m_sGeneral.m_bReadSolids = true;
        default_load_params.m_sGeneral.m_bReadSurfaces = true;
        default_load_params.m_sGeneral.m_bReadAttributes = true;
        default_load_params.m_sGeneral.m_bReadActiveFilter = true;
        default_load_params.m_sGeneral.m_eReadingMode2D3D = kA3DRead_3D;
        default_load_params.m_sGeneral.m_eReadGeomTessMode = kA3DReadGeomAndTess;
        default_load_params.m_sGeneral.m_eDefaultUnit = kA3DUnitUnknown;
        default_load_params.m_sTessellation.m_eTessellationLevelOfDetail = kA3DTessLODMedium;
        default_load_params.m_sAssembly.m_bUseRootDirectory = true;
        default_load_params.m_sMultiEntries.m_bLoadDefault = true;
    }
    return default_load_params;
}

A3DRWParamsLoadData const& Assimp::ExchangeLoaderConfig::GetLoadParams(void) const {
    return mLoadParameters;
}

void Assimp::ExchangeLoaderConfig::SetLoadParams(A3DRWParamsLoadData const& load_params) {
    mLoadParameters = load_params;
}

std::string const& Assimp::ExchangeLoaderConfig::GetExchangeLibraryFolder(void) const {
    return mExchangeLibraryFolder;
}

void Assimp::ExchangeLoaderConfig::SetExchangeLibraryFolder(std::string const &folder) {
    mExchangeLibraryFolder = folder;
}

std::string const& Assimp::ExchangeLoaderConfig::GetTextureFolder(void) const {
    return mTextureFolder;
}

void Assimp::ExchangeLoaderConfig::SetTextureFolder(std::string const& folder) {
    mTextureFolder = folder;
}
