#pragma once
#include <nanobind/nanobind.h>
#include <sc-api/sim_data_builder.h>

void populate_sim_data_builder(sc_api::sim_data::SimDataUpdateBuilder& builder, const nanobind::dict& data);
