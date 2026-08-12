/**
 * @file
 * @brief Version of the API implementation.
 *
 * These macros give the version of the API implementation itself. The API sends this
 * version to Simucube Tuner when it registers a session. Tuner uses it to find
 * incompatibilities between the API and the backend.
 *
 * This version is not the version of the sc-api package. The package version comes from
 * the CMake project and Doxygen shows it as the project number.
 */

#ifndef SC_API_VERSION_H_
#define SC_API_VERSION_H_

#define SC_API_CORE_VERSION_MAJOR 0
#define SC_API_CORE_VERSION_MINOR 1
#define SC_API_CORE_VERSION_PATCH 1

#endif  // SC_API_VERSION_H_
