#pragma once

#include "envoy/extensions/filters/http/s3_auth/v3/s3_auth.pb.h"
#include "envoy/http/filter.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace S3AuthFilter {

/**
 * All stats for the S3 auth filter. @see stats_macros.h
 */
// clang-format off
#define ALL_S3_AUTH_FILTER_STATS(COUNTER)                                                           \
  COUNTER(auth_header_added)
// clang-format on

/**
 * Wrapper struct filter stats. @see stats_macros.h
 */
struct FilterStats {
  ALL_S3_AUTH_FILTER_STATS(GENERATE_COUNTER_STRUCT)
};

/**
 * HTTP S3 auth filter.
 */
class Filter : public Http::StreamFilter {
public:
  Filter(const std::string& stat_prefix) : stat_prefix_(stat_prefix) {}

  // Http::StreamFilterBase
  void onDestroy() override {}

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::HeaderMap& headers, bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;
  Http::FilterTrailersStatus decodeTrailers(Http::HeaderMap& trailers) override;
  void setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) override {
    decoder_callbacks_ = &callbacks;
  }

  // Http::StreamEncoderFilter
  Http::FilterHeadersStatus encode100ContinueHeaders(Http::HeaderMap&) override {
    return Http::FilterHeadersStatus::Continue;
  }
  Http::FilterHeadersStatus encodeHeaders(Http::HeaderMap& headers, bool end_stream) override;
  Http::FilterDataStatus encodeData(Buffer::Instance& data, bool end_stream) override;
  Http::FilterTrailersStatus encodeTrailers(Http::HeaderMap& trailers) override;
  Http::FilterMetadataStatus encodeMetadata(Http::MetadataMap&) override {
    return Http::FilterMetadataStatus::Continue;
  }
  void setEncoderFilterCallbacks(Http::StreamEncoderFilterCallbacks&) override {}

private:
  const std::string stat_prefix_;
  Http::StreamDecoderFilterCallbacks* decoder_callbacks_{};
};

} // namespace S3AuthFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
