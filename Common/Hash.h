#pragma once

#include <string>


enum class HashType
{
    SHA1,
    SHA256,
};

bool CheckHash(
    HashType,
    const std::string& string,
    const std::string& salt,
    const std::string& hash);

std::string MakeHash(
    HashType,
    const std::string& string,
    const std::string& salt);
