/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_SIM_DATA_INTERNAL_H_
#define SC_API_SIM_DATA_INTERNAL_H_
#include "sc-api/core/sim_data.h"
#include "shm_bson_data_provider.h"

namespace sc_api::core::internal {

class SimDataProvider : public BsonShmDataProvider {
public:
    std::shared_ptr<sim_data::SimData> parseSimData();

private:
    std::shared_ptr<sim_data::SimData> parsed_data_;
};

}  // namespace sc_api::core::internal

#endif  // SC_API_SIM_DATA_INTERNAL_H_
