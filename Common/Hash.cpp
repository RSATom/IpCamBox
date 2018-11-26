#include "Hash.h"

#include <vector>

#include <openssl/sha.h>


bool CheckHash(
    HashType hashType,
    const std::string& string,
    const std::string& salt,
    const std::string& hash)
{
    switch(hashType) {
        case HashType::SHA1: {
            const std::string stringToHash = salt + string;
            std::string calculatedHash(SHA_DIGEST_LENGTH, 0);

            SHA1(
                reinterpret_cast<const unsigned char*>(stringToHash.c_str()),
                stringToHash.size(),
                reinterpret_cast<unsigned char*>(&calculatedHash[0]));

            return hash == calculatedHash;
        }
        case HashType::SHA256: {
            const std::string stringToHash = salt + string;
            std::string calculatedHash(SHA256_DIGEST_LENGTH, 0);

            SHA256(
                reinterpret_cast<const unsigned char*>(stringToHash.c_str()),
                stringToHash.size(),
                reinterpret_cast<unsigned char*>(&calculatedHash[0]));

            return hash == calculatedHash;
        }
    }

    return false;
}

std::string MakeHash(
    HashType hashType,
    const std::string& string,
    const std::string& salt)
{
    switch(hashType) {
        case HashType::SHA1: {
            const std::string stringToHash = salt + string;
            std::string calculatedHash(SHA_DIGEST_LENGTH, 0);

            SHA1(
                reinterpret_cast<const unsigned char*>(stringToHash.c_str()),
                stringToHash.size(),
                reinterpret_cast<unsigned char*>(&calculatedHash[0]));

            return calculatedHash;
        }
        case HashType::SHA256: {
            const std::string stringToHash = salt + string;
            std::string calculatedHash(SHA256_DIGEST_LENGTH, 0);

            SHA256(
                reinterpret_cast<const unsigned char*>(stringToHash.c_str()),
                stringToHash.size(),
                reinterpret_cast<unsigned char*>(&calculatedHash[0]));

            return calculatedHash;
        }
    }

    return std::string();
}
