#include "sc-api/core/sim_data/sim.h"

namespace sc_api::core::sim_data::sim {

const SimPropertyRef<bool> process_detection{"process_detection"};
const SimPropertyRef<bool> max_rpm_available{"max_rpm_available"};
const SimPropertyRef<bool> full_shift_light_data_available{"full_shift_light_data_available"};
const SimPropertyRef<bool> vehicle_detection_support{"vehicle_detection_support"};
const SimPropertyRef<std::string_view> name{"name"};


}  // namespace sc_api::core::sim_data::sim
