#pragma once

#if UPDATE_VERSION_TYPE_UINT64 == 1
typedef uint64_t version_t;
#elif UPDATE_VERSION_TYPE_STRING == 1
typedef std::string version_t;
#else
#error "No valid version type defined"
#endif
