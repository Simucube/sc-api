/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_API_H_
#define SC_API_API_H_
#include <sc-api/core/api.h>

#include "./session.h"

namespace sc_api {

/** API documentation here */
typedef ::sc_api::core::Api Api;

using ::sc_api::core::ApiUserInformation;
using ::sc_api::core::NoAuthControlEnabler;
using ::sc_api::core::SecureControlEnabler;

}  // namespace sc_api

#endif  // SC_API_INTERNAL_API_H_
