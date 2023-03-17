/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fbpcf::mpc_std_lib::oram {
/*
 * An ORAM encoder is responsible for taking tuples of aggregation indexes
 * and mapping them to a unique single ID that can be consumed by an
 * ORAM implementation
 */
class IOramEncoder {
 public:
  class OramMappingConfig {
   public:
    OramMappingConfig() {}

    std::string toString() {
      return "";
    }

    static OramMappingConfig fromString() {
      return OramMappingConfig();
    }

   private:
  };

  virtual ~IOramEncoder() = default;

  /*
   * Given the list of all breakdown column values, assign a unique ORAM index
   * to each permutation and return the mapping information that can be used to
   * retrieve the results. Can be called multiple times in batch mode and
   * preserve the ordering. A value of 0 indicates the tuple is filtered out.
   */
  virtual std::vector<uint32_t> generateORAMIndexes(
      const std::vector<std::vector<uint32_t>>& tuples) = 0;

  /*
   * Retrieve the current mapping for all permutations of the group by columns.
   * Should only be called once after finishing calling generateORAMIndexes()
   */
  virtual std::unique_ptr<OramMappingConfig> exportMappingConfig() const = 0;

 private:
};

} // namespace fbpcf::mpc_std_lib::oram