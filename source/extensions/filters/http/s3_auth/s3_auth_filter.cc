#include "extensions/filters/http/s3_auth/s3_auth_filter.h"

#include "envoy/extensions/filters/http/s3_auth/v3/s3_auth.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace S3AuthFilter {

Http::FilterHeadersStatus Filter::decodeHeaders(Http::HeaderMap&, bool) {
  // add auth headers.
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance&, bool) {
  return Http::FilterDataStatus::Continue;
}

Http::FilterTrailersStatus Filter::decodeTrailers(Http::HeaderMap&) {
  return Http::FilterTrailersStatus::Continue;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::HeaderMap&, bool) {
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance&, bool) {
  return Http::FilterDataStatus::Continue;
}

Http::FilterTrailersStatus Filter::encodeTrailers(Http::HeaderMap&) {
  return Http::FilterTrailersStatus::Continue;
}

} // namespace S3AuthFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
