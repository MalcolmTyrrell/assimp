
#ifndef AI_EXCHANGELOADER_H_INCLUDED
#define AI_EXCHANGELOADER_H_INCLUDED

#include <assimp/BaseImporter.h>
#include <assimp/types.h>

#include "A3DSDKIncludes.h"
// Forward declarations
struct aiNode;

namespace Assimp {

// ---------------------------------------------------------------------------
/**
 * @brief   Importer class for the formats supported by HOOPS Exchange.
 */
class ExchangeImporter : public BaseImporter {
public:
    enum class InitializeStatus {
        OK,
        FAILURE_NOT_INITIALIZED,
        FAILURE_LOAD_LIBRARIES,
        FAILURE_LICENSE_INVALID,
        FAILURE_LIBRARY_VERSION_MISMATCH
    };

    static InitializeStatus Initialize(std::string const &exchange_library_path, std::string const &hoops_license);

    static std::string const METADATA_KEY_MATERIAL; // value- aiMetadata
    static std::string const METADATA_KEY_MATERIAL_DENSITY; // value - double
    static std::string const METADATA_KEY_MATERIAL_NAME; // value - string

    static std::string const METADATA_KEY_UNIT_FACTOR;
    static std::string const METADATA_KEY_FORMAT; // value - one of A3DEModellerType

protected:
    static InitializeStatus mInitializeStatus;

public:
    /**
     * @brief ExchangeImporter, the class default constructor.
     */
    ExchangeImporter();

    /**
     * @brief   The class destructor.
     */
    ~ExchangeImporter();

    /**
     * @brief   Returns whether the class can handle the format of the given file.
     *  See BaseImporter::CanRead() for details.
     */
    bool CanRead(const std::string &pFile, IOSystem *pIOHandler, bool checkSig) const;

protected:
    /**
     * @brief   Return importer meta information.
     *  See #BaseImporter::GetInfo for the details
     */
    const aiImporterDesc *GetInfo() const;

    /**
     * @brief   Imports the given file into the given scene structure.
    * See BaseImporter::InternReadFile() for details
    */
    void InternReadFile(const std::string &pFile, aiScene *pScene,
            IOSystem *pIOHandler);


private:
    A3DRWParamsLoadData mLoadParameters;
};

} // end of namespace Assimp

#endif // AI_EXCHANGELOADER_H_INCLUDED
