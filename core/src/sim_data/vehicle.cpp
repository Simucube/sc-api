#include "sc-api/core/sim_data/vehicle.h"

namespace sc_api::core::sim_data::vehicle {

const VehiclePropertyRef<double> engine_idle_rpm{"engine_idle_rpm"};
const VehiclePropertyRef<double> engine_redline_rpm{"engine_redline_rpm"};
const VehiclePropertyRef<double> shift_light_first_rpm{"shift_light_first_rpm"};
const VehiclePropertyRef<double> shift_light_last_rpm{"shift_light_last_rpm"};
const VehiclePropertyRef<double> shift_light_blink_rpm{"shift_light_blink_rpm"};
const VehiclePropertyRef<double> steering_wheel_rotation_deg{"steering_wheel_rotation_deg"};
const VehiclePropertyRef<int32_t> gearbox_forward_gears{"gearbox_forward_gears"};
const VehiclePropertyRef<int32_t> gearbox_backward_gears{"gearbox_backward_gears"};
const VehiclePropertyRef<bool> non_unique{"non_unique"};
const VehiclePropertyRef<bool> has_drs{"has_drs"};
const VehiclePropertyRef<bool> has_abs{"has_abs"};
const VehiclePropertyRef<bool> has_tc{"has_tc"};
const VehiclePropertyRef<std::string_view> name{"name"};
const VehiclePropertyRef<std::string_view> short_name{"short_name"};
const VehiclePropertyRef<std::string_view> model{"model"};
const VehiclePropertyRef<std::string_view> brand{"brand"};
const VehiclePropertyRef<std::string_view> class_name{"class_name"};


}  // namespace sc_api::core::sim_data::vehicle
