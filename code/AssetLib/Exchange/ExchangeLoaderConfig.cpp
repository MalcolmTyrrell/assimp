#define INITIALIZE_A3D_API
#include "ExchangeLoaderConfig.hpp"

// static
Assimp::ExchangeLoaderConfig &Assimp::ExchangeLoaderConfig::instance(void) {
    static ExchangeLoaderConfig _instance;
    return _instance;
}

Assimp::ExchangeLoaderConfig::ExchangeLoaderConfig(void) :
        mInitializeStatus(InitializeStatus::FAILURE_NOT_INITIALIZED),
        mLoadParameters(GetDefaultLoadParams()),
        mFileExtensions(GetDefaultFileExtensionSet()) {
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
A3DRWParamsLoadData const &Assimp::ExchangeLoaderConfig::GetDefaultLoadParams(void) {
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

A3DRWParamsLoadData const &Assimp::ExchangeLoaderConfig::GetLoadParams(void) const {
    return mLoadParameters;
}

void Assimp::ExchangeLoaderConfig::SetLoadParams(A3DRWParamsLoadData const &load_params) {
    mLoadParameters = load_params;
}

std::string const &Assimp::ExchangeLoaderConfig::GetExchangeLibraryFolder(void) const {
    return mExchangeLibraryFolder;
}

void Assimp::ExchangeLoaderConfig::SetExchangeLibraryFolder(std::string const &folder) {
    mExchangeLibraryFolder = folder;
}

std::string const &Assimp::ExchangeLoaderConfig::GetTextureFolder(void) const {
    return mTextureFolder;
}

void Assimp::ExchangeLoaderConfig::SetTextureFolder(std::string const &folder) {
    mTextureFolder = folder;
}

// static
Assimp::ExchangeLoaderConfig::StringSet const &Assimp::ExchangeLoaderConfig::GetDefaultFileExtensionSet(void) {
    static StringSet const extensions = {
        "3dm", // Rhino
        // "3mf", // 3MF
        "3dxml", // catia v6
        "asm", "neu", "prt", "xas", "xpr", // creo
        "catdrawing", "catpart", "catproduct", "catshape", "exp", // catia v5
        "dae", // collada
        "dwf", "dwfx", // AutoDesk DWF
        "dwg", "dwf", // AutoCAD 2D, 3D
        "ifc", "ifczip", // IFC
        "iam", "ipt", // inventor
        "igs", "iges", // IGES
        "mf1", "arc", "unv", "pkg", // I-deas
        "model", "session", "dlv", "exp", // catia v4
        "jt", // JT
        "asm", "par", "pwd", "psm", // SolidEdge
        "prc", // exchange std
        "prt" , // nx
        "rvt", "rfa", // Revit 2
        "sat", // acis text
        "sab", // acis binary
        "sldprt", "sldasm", // solidworks 
        "step", "stp", "stpz", // step part or assembly
        "stpx", "stpxz", // STEP/XML
        // "stl", // STL
        "u3d",
        "vda",
        "wrl", "vrml", // VRML
        // "obj",
        "x_b", "x_t", "xmt", "xmt_txt", // Parasolid
        "pdf", // 3D-PDF
    };
    return extensions;
}

Assimp::ExchangeLoaderConfig::StringSet const &Assimp::ExchangeLoaderConfig::GetFileExtensionSet(void) const {
    return mFileExtensions;
}

void Assimp::ExchangeLoaderConfig::SetFileExtensionSet(StringSet const &extensions) {
    mFileExtensions = extensions;
}
