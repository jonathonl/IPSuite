#pragma once

#ifndef MANIFOLD_HTTP_MESSAGE_HPP
#define MANIFOLD_HTTP_MESSAGE_HPP

#include "http_connection.hpp"
#include "http_message_head.hpp"
#include "hpack.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    class message
    {
    public:
      //----------------------------------------------------------------//
//      enum class error_code
//      { NoError = 0, SocketError, HeadCorrupt, HeadTooLarge };
      //----------------------------------------------------------------//
    protected:
      //----------------------------------------------------------------//
      header_block& head_;
      std::shared_ptr<http::connection> connection_;
      std::uint32_t stream_id_;
      bool ended_ = false;
      //----------------------------------------------------------------//
    public:
      //----------------------------------------------------------------//
      message(header_block& head, const std::shared_ptr<http::connection>& conn, std::uint32_t stream_id);
      virtual ~message();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      std::uint32_t stream_id() const;
      void on_stream_reset(const std::function<void(const std::error_code& ec)>& cb);
      //----------------------------------------------------------------//
    };
    //================================================================//
  }
}

#endif //MANIFOLD_HTTP_MESSAGE_HPP
