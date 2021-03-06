// Copyright 2017, OpenCensus Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "opencensus/trace/sampler.h"

#include <cmath>
#include <cstdint>

#include "absl/base/attributes.h"

namespace opencensus {
namespace trace {

namespace {
// Converts a probability in [0, 1] to a threshold in [0, UINT64_MAX].
uint64_t CalculateThreshold(double probability) {
  if (probability <= 0.0) return 0;
  if (probability >= 1.0) return UINT64_MAX;
  // We can't directly return probability * UINT64_MAX.
  //
  // UINT64_MAX is (2^64)-1, but as a double rounds up to 2^64.
  // For probabilities >= 1-(2^-54), the product wraps to zero!
  // Instead, calculate the high and low 32 bits separately.
  const double product = UINT32_MAX * probability;
  double hi_bits, lo_bits = ldexp(modf(product, &hi_bits), 32) + product;
  return (static_cast<uint64_t>(hi_bits) << 32) +
         static_cast<uint64_t>(lo_bits);
}

uint64_t CalculateThresholdFromBuffer(const TraceId& trace_id) {
  const uint8_t* buf = reinterpret_cast<const uint8_t*>(trace_id.Value());
  uint64_t res = 0;
  // We only use the first 8 bytes of TraceId.
  static_assert(TraceId::kSize >= 8, "TraceID must be at least 8 bytes long.");
  for (int i = 0; i < 8; ++i) {
    res |= (static_cast<uint64_t>(buf[i]) << (i * 8));
  }
  return res;
}
}  // namespace

ProbabilitySampler::ProbabilitySampler(double probability)
    : threshold_(CalculateThreshold(probability)) {}

bool ProbabilitySampler::ShouldSample(
    const SpanContext* parent_context ABSL_ATTRIBUTE_UNUSED,
    bool has_remote_parent ABSL_ATTRIBUTE_UNUSED, const TraceId& trace_id,
    const SpanId& span_id ABSL_ATTRIBUTE_UNUSED,
    absl::string_view name ABSL_ATTRIBUTE_UNUSED,
    const std::vector<Span*>& parent_links ABSL_ATTRIBUTE_UNUSED) const {
  if (threshold_ == 0) return false;
  // All Spans within the same Trace will get the same sampling decision, so
  // full trees of Spans will be sampled.
  return CalculateThresholdFromBuffer(trace_id) <= threshold_;
}

}  // namespace trace
}  // namespace opencensus
