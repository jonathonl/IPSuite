
#include <sstream>
#include <algorithm>

#include "http_v2_request_head.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    v2_request_head::v2_request_head(v2_header_block&& hb)
      : v2_header_block(std::move(hb))
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_request_head::v2_request_head(const std::string& path, const std::string& method, std::list<hpack::header_field>&& headers)
    {
      this->headers_ = std::move(headers);
      this->path(path);
      this->method(method);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_request_head::v2_request_head(const std::string& path, http::method method, std::list<hpack::header_field>&& headers)
    {
      this->headers_ = std::move(headers);
      this->path(path);
      this->method(method);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_request_head::~v2_request_head()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string v2_request_head::method() const
    {
      return this->header(":method");
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_request_head::method(const std::string& value)
    {
      std::string tmp(value);
      std::for_each(tmp.begin(), tmp.end(), ::toupper);
      this->pseudo_header(":method", std::move(tmp));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_request_head::method(http::method value)
    {
      this->pseudo_header(":method", std::move(method_enum_to_string(value)));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string v2_request_head::path() const
    {
      return this->header(":path");
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_request_head::path(const std::string& value)
    {
      this->pseudo_header(":path", value);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string v2_request_head::scheme() const
    {
      return this->header(":scheme");
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_request_head::scheme(const std::string& value)
    {
      this->pseudo_header(":scheme", value);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string v2_request_head::authority() const
    {
      return this->header(":authority");
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_request_head::authority(const std::string& value)
    {
      this->pseudo_header(":authority", value);
    }
    //----------------------------------------------------------------//
  }
}