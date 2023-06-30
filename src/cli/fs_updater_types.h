#pragma once

#ifdef UPDATE_VERSION_TYPE_UINT64
typedef uint64_t version_t;
#elif defined(UPDATE_VERSION_TYPE_STRING)
typedef std::string version_t;
#endif