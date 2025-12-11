/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PowerStatsProvider.h"
#include <android/hardware/power/stats/IPowerStats.h>
#include <android-base/logging.h>
#include <mediautils/ServiceSingleton.h>
#include <psh_utils/AudioPowerManager.h>
#include <unordered_map>

using ::android::hardware::power::stats::IPowerStats;

namespace android::media::psh_utils {

static sp<IPowerStats> getPowerStatsService() {
    if (!AudioPowerManager::enabled()) {
        LOG(ERROR) << __func__ << ": should not be called if not enabled";
        return {};
    }
    return mediautils::getService<IPowerStats>();
}

status_t RailEnergyDataProvider::fill(PowerStats *stat) const {
    if (!AudioPowerManager::enabled()) return NO_INIT;

    if (stat == nullptr) return BAD_VALUE;
    auto powerStatsService = getPowerStatsService();
    if (powerStatsService == nullptr) {
        return NO_INIT;
    }

    std::unordered_map<int32_t, ::android::hardware::power::stats::Channel> channelMap;
    {
        std::vector<::android::hardware::power::stats::Channel> channels;
        if (!powerStatsService->getEnergyMeterInfo(&channels).isOk()) {
            LOG(ERROR) << "unable to get energy meter info";
            return INVALID_OPERATION;
        }
        for (auto& channel : channels) {
          channelMap.emplace(channel.id, std::move(channel));
        }
    }

    std::vector<::android::hardware::power::stats::EnergyMeasurement> measurements;
    if (!powerStatsService->readEnergyMeter({}, &measurements).isOk()) {
        LOG(ERROR) << "unable to get energy measurements";
        return INVALID_OPERATION;
    }

    for (const auto& measurement : measurements) {
        stat->rail_energy.emplace_back(
            channelMap.at(measurement.id).subsystem,
            channelMap.at(measurement.id).name,
            measurement.energyUWs);
    }

    // Sort entries first by subsystem_name, then by rail_name.
    // Sorting is needed to make interval processing efficient.
    std::sort(stat->rail_energy.begin(), stat->rail_energy.end(),
              [](const auto& a, const auto& b) {
                  if (a.subsystem_name != b.subsystem_name) {
                      return a.subsystem_name < b.subsystem_name;
                  }
                  return a.rail_name < b.rail_name;
              });

    return NO_ERROR;
}

status_t PowerEntityResidencyDataProvider::fill(PowerStats* stat) const {
    if (!AudioPowerManager::enabled()) return NO_INIT;

    if (stat == nullptr) return BAD_VALUE;
    auto powerStatsService = getPowerStatsService();
    if (powerStatsService == nullptr) {
        return NO_INIT;
    }

    // these are based on entityId
    std::unordered_map<int32_t, std::string> entityNames;
    std::unordered_map<int32_t, std::unordered_map<int32_t, std::string>> stateNames;
    std::vector<int32_t> powerEntityIds; // ids to use

    {
        std::vector<::android::hardware::power::stats::PowerEntity> entities;
        if (!powerStatsService->getPowerEntityInfo(&entities).isOk()) {
            LOG(ERROR) << __func__ << ": unable to get entity info";
            return INVALID_OPERATION;
        }

        std::vector<std::string> powerEntityNames;
        for (const auto& entity : entities) {
            std::unordered_map<int32_t, std::string> states;
            for (const auto& state : entity.states) {
                states.emplace(state.id, state.name);
            }

            if (std::find(powerEntityNames.begin(), powerEntityNames.end(), entity.name) !=
                powerEntityNames.end()) {
                powerEntityIds.emplace_back(entity.id);
            }
            entityNames.emplace(entity.id, std::move(entity.name));
            stateNames.emplace(entity.id, std::move(states));
        }
    }

    std::vector<::android::hardware::power::stats::StateResidencyResult> results;
    if (!powerStatsService->getStateResidency(powerEntityIds, &results).isOk()) {
        LOG(ERROR) << __func__ << ": Unable to get state residency";
        return INVALID_OPERATION;
    }

    for (const auto& result : results) {
        for (const auto& curStateResidency : result.stateResidencyData) {
          stat->power_entity_state_residency.emplace_back(
              entityNames.at(result.id),
              stateNames.at(result.id).at(curStateResidency.id),
              static_cast<uint64_t>(curStateResidency.totalTimeInStateMs),
              static_cast<uint64_t>(curStateResidency.totalStateEntryCount));
        }
    }

    // Sort entries first by entity_name, then by state_name.
    // Sorting is needed to make interval processing efficient.
    std::sort(stat->power_entity_state_residency.begin(),
              stat->power_entity_state_residency.end(),
              [](const auto& a, const auto& b) {
                  if (a.entity_name != b.entity_name) {
                      return a.entity_name < b.entity_name;
                  }
                  return a.state_name < b.state_name;
              });
    return NO_ERROR;
}

} // namespace android::media::psh_utils
