#ifndef AI_EXCHANGELOADERCONFIG_H_INCLUDED
#define AI_EXCHANGELOADERCONFIG_H_INCLUDED

#include <string>
#include "A3DSDKIncludes.h"


namespace Assimp {

// Defaults are set for all fields in this class *except* for the
// ExchangeLibraryFolder. This must be set explicitly. If left unset
// the OS will use the default dynamic library search path (pwd, PATH)
class ExchangeLoaderConfig {
public:
    static ExchangeLoaderConfig &instance(void);

    enum class InitializeStatus {
        OK,
        FAILURE_NOT_INITIALIZED,
        FAILURE_LOAD_LIBRARIES,
        FAILURE_LICENSE_INVALID,
        FAILURE_LIBRARY_VERSION_MISMATCH
    };

    // This can be called explicitly prior to any use of the ExchangeLoader
    // or will be called implicitly when it is first called upon to load
    // a support file type
    InitializeStatus Initialize(std::string const &hoops_license = HOOPS_LICENSE);
    InitializeStatus GetInitializationStatus(void) const;

    // Returns a reasonable set of default values for loading parameters
    static A3DRWParamsLoadData GetDefaultLoadParams(void);

    // Gets the current load parameters. Initially these are set to the defaults (above)
    A3DRWParamsLoadData const &GetLoadParams(void) const;

    // Sets the load paramters. Use this method if you want to change loading behavior
    void SetLoadParams(A3DRWParamsLoadData const &load_params);

    // No default value provided - this must be set to the folder containing the Exchange dynamic libraries
    std::string const &GetExchangeLibraryFolder(void) const;
    void SetExchangeLibraryFolder(std::string const &folder);

    // Defaults to temp folder obtained from environment variable
    std::string const &GetTextureFolder(void) const;
    // Set the texture folder. This is where extracted texture files will be written
    void SetTextureFolder(std::string const &folder);


private:
    ExchangeLoaderConfig(void);
    InitializeStatus mInitializeStatus;
    std::string mExchangeLibraryFolder;
    std::string mTextureFolder;
    A3DRWParamsLoadData mLoadParameters;
};

} // end of namespace Assimp

#endif // AI_EXCHANGELOADERCONFIG_H_INCLUDED
