/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "../../../common/Csv.h"
#include "../../OutputMetricsData.h"
#include "LiftCalculator.h"

namespace private_lift {
std::unordered_map<std::string, int32_t> LiftCalculator::mapColToIndex(
    const std::vector<std::string>& headerPublisher,
    const std::vector<std::string>& headerPartner) const {
  std::unordered_map<std::string, int32_t> colNameToIndex;
  int32_t index;
  for (auto it = headerPublisher.begin(); it != headerPublisher.end(); ++it) {
    index = std::distance(headerPublisher.begin(), it);
    colNameToIndex[*it] = index;
  }
  for (auto it = headerPartner.begin(); it != headerPartner.end(); ++it) {
    index = std::distance(headerPartner.begin(), it);
    colNameToIndex[*it] = index;
  }
  return colNameToIndex;
}

// Parse input string with format [111,222,333,...]
std::vector<uint64_t> LiftCalculator::parseArray(std::string array) const {
  auto innerString = array.substr(1, array.size() - 1);
  std::vector<uint64_t> out;
  auto values = csv::splitByComma(innerString, false);
  for (auto i = 0; i < values.size(); ++i) {
    int64_t parsed = 0;
    std::istringstream iss{values[i]};
    iss >> parsed;
    if (iss.fail()) {
      LOG(FATAL) << "Failed to parse '" << iss.str() << "' to int64_t";
    }
    out.push_back(parsed);
  }
  return out;
}

OutputMetricsData LiftCalculator::compute(
    std::ifstream& inFilePublisher,
    std::ifstream& inFilePartner,
    std::unordered_map<std::string, int32_t>& colNameToIndex,
    int32_t tsOffset) const {
  OutputMetricsData out;
  uint64_t opportunity = 0;
  uint64_t testFlag = 0;
  uint64_t opportunityTimestamp = 0;
  std::vector<uint64_t> eventTimestamps;
  std::vector<uint64_t> values;

  std::string linePublisher;
  std::string linePartner;

  // Initialize field values from OutputMetricsData
  out.testPopulation = 0;
  out.controlPopulation = 0;
  out.testEvents = 0;
  out.controlEvents = 0;
  out.testConverters = 0;
  out.controlConverters = 0;
  out.testValue = 0;
  out.controlValue = 0;
  out.testSquared = 0;
  out.controlSquared = 0;
  out.testMatchCount = 0;
  out.controlMatchCount = 0;
  out.testLogValue = 0;
  out.controlLogValue = 0;

  // Read line by line, at the same time compute metrics
  while (getline(inFilePublisher, linePublisher) &&
         getline(inFilePartner, linePartner)) {
    auto partsPublisher = csv::splitByComma(linePublisher, true);
    auto partsPartner = csv::splitByComma(linePartner, true);

    if (partsPublisher.empty()) {
      LOG(FATAL) << "Empty publisher line";
    }

    // Opportunity is actually an optional column
    if (colNameToIndex.find("opportunity") != colNameToIndex.end()) {
      std::istringstream issOpportunity{
          partsPublisher.at(colNameToIndex.at("opportunity"))};
      issOpportunity >> opportunity;
      if (issOpportunity.fail()) {
        LOG(FATAL) << "Failed to parse '" << issOpportunity.str()
                   << "' to uint64_t";
      }
    } else {
      // If it isn't present, assume all lines had opportunities
      opportunity = 1;
    }

    std::istringstream issTestFlag{
        partsPublisher.at(colNameToIndex.at("test_flag"))};
    issTestFlag >> testFlag;
    if (issTestFlag.fail()) {
      LOG(FATAL) << "Failed to parse '" << issTestFlag.str() << "' to uint64_t";
    }

    std::istringstream issOpportunityTimestamp{
        partsPublisher.at(colNameToIndex.at("opportunity_timestamp"))};
    issOpportunityTimestamp >> opportunityTimestamp;
    if (issOpportunityTimestamp.fail()) {
      LOG(FATAL) << "Failed to parse '" << issOpportunityTimestamp.str()
                 << "' to uint64_t";
    }

    if (partsPartner.empty()) {
      LOG(FATAL) << "Empty partner line";
    }
    eventTimestamps =
        parseArray(partsPartner.at(colNameToIndex.at("event_timestamps")));

    auto valuesIdx = colNameToIndex.find("values") != colNameToIndex.end()
        ? colNameToIndex.at("values")
        : -1;
    if (valuesIdx != -1) {
      values = parseArray(partsPartner.at(valuesIdx));

      if (eventTimestamps.size() != values.size()) {
        LOG(FATAL) << "Size of event_timestamps (" << eventTimestamps.size()
                   << ") and values (" << values.size() << ") are inconsistent";
      }
    }
    if (opportunity && opportunityTimestamp > 0) {
      uint64_t value_subsum = 0;
      bool converted = false;
      bool countedMatchAlready = false;
      if (testFlag) {
        ++out.testPopulation;

        for (auto i = 0; i < eventTimestamps.size(); ++i) {
          if (opportunityTimestamp > 0 && eventTimestamps.at(i) > 0 &&
              !countedMatchAlready) {
            ++out.testMatchCount;
            countedMatchAlready = true;
          }
          if (opportunityTimestamp < eventTimestamps.at(i) + tsOffset) {
            // Only record the first time the user has a valid conversion
            if (!converted) {
              ++out.testConverters;
            }
            ++out.testEvents;
            converted = true;
            if (valuesIdx != -1) {
              // Only add values if the values column exists
              // (support valueless objectives)
              value_subsum += values.at(i);
            }
          }
        }
        out.testValue += value_subsum;
        out.testLogValue += value_subsum > 0 ? log(value_subsum) : 0;
        out.testSquared += value_subsum * value_subsum;
      } else {
        ++out.controlPopulation;
        for (auto i = 0; i < eventTimestamps.size(); ++i) {
          if (opportunityTimestamp > 0 && eventTimestamps.at(i) > 0 &&
              !countedMatchAlready) {
            ++out.controlMatchCount;
            countedMatchAlready = true;
          }
          if (opportunityTimestamp < eventTimestamps.at(i) + tsOffset) {
            // Only record the first time the user has a valid conversion
            if (!converted) {
              ++out.controlConverters;
            }
            ++out.controlEvents;
            converted = true;
            if (valuesIdx != -1) {
              // Only add values if the values column exists
              // (support valueless objectives)
              value_subsum += values.at(i);
            }
          }
        }
        out.controlValue += value_subsum;
        out.controlLogValue += value_subsum > 0 ? log(value_subsum) : 0;
        out.controlSquared += value_subsum * value_subsum;
      }
    }
  }

  return out;
}
} // namespace private_lift
