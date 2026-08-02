#pragma once

#include <string>

namespace VectorSmartFilament {

template<class T> struct Result
{
    T           value;
    std::string error;

    bool ok() const { return error.empty(); }
};

template<> struct Result<void>
{
    std::string error;

    bool ok() const { return error.empty(); }
};

} // namespace VectorSmartFilament
