#pragma once
#include <nanobind/nanobind.h>
#include <sc-api/core/sim_data_builder.h>

void populate_sim_data_builder(sc_api::core::sim_data::SimDataUpdateBuilder& builder,
                               const nanobind::dict& data);
