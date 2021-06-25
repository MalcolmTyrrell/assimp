
#ifndef AI_EXCHANGELOADER_H_INCLUDED
#define AI_EXCHANGELOADER_H_INCLUDED

#include <assimp/BaseImporter.h>
#include <assimp/types.h>
#include <string>
#include "A3DSDKIncludes.h"
// Forward declarations
struct aiNode;

namespace Assimp {

// ---------------------------------------------------------------------------
/**
 * @brief   Importer class for the formats supported by HOOPS Exchange.
 */
class ExchangeLoader : public BaseImporter {
public:

    static std::string const METADATA_KEY_MATERIAL; // value- aiMetadata
    static std::string const METADATA_KEY_MATERIAL_DENSITY; // value - double
    static std::string const METADATA_KEY_MATERIAL_NAME; // value - string

    static std::string const METADATA_KEY_UNIT_FACTOR;
    static std::string const METADATA_KEY_FORMAT; // value - one of A3DEModellerType

                                                  /**
     * @brief ExchangeLoader, the class default constructor.
     */
    ExchangeLoader();

    /**
     * @brief   The class destructor.
     */
    ~ExchangeLoader();

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
};

} // end of namespace Assimp

#endif // AI_EXCHANGELOADER_H_INCLUDED
