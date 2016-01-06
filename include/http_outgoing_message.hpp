#pragma once

#ifndef MANIFOLD_HTTP_OUTGOING_MESSAGE_HPP
#define MANIFOLD_HTTP_OUTGOING_MESSAGE_HPP

#include "http_message.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    class outgoing_message : public message
    {
    private:
      //----------------------------------------------------------------//
      std::uint64_t bytes_sent_;
      bool headers_sent_;
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      //bool sendChunkedEntity(const char* data, std::size_t dataSize);
      //bool sendKnownLengthEntity(const char* data, std::size_t dataSize);
      //----------------------------------------------------------------//
    protected:
      //----------------------------------------------------------------//
      virtual v2_header_block& message_head() = 0;
      //----------------------------------------------------------------//
    public:
      //----------------------------------------------------------------//
      outgoing_message(const std::shared_ptr<http::connection>& conn, std::int32_t stream_id);
      virtual ~outgoing_message();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      virtual bool send_headers(bool end_stream = false); // Must be virtual since client::request and server::response override while outgoing_message::end/send call this method.
      bool send(const char*const data, std::size_t data_sz);
      bool send(const char* cstr) { return this->send(std::string(cstr)); }
      template <typename BufferT>
      bool send(const BufferT& dataBuffer)
      {
        return this->send(dataBuffer.data(), dataBuffer.size());
      }
      void on_drain(const std::function<void()>& fn);
      bool end(const char*const data, std::size_t data_sz, const v2_header_block& trailers = {});
      bool end(const char* cstr, const v2_header_block& trailers = {})
      {
        return this->end(std::string(cstr), trailers);
      }
      template <typename BufferT>
      bool end(const BufferT& dataBuffer, const v2_header_block& trailers = {})
      {
        return this->end(dataBuffer.data(), dataBuffer.size(), trailers);
      }
      bool end(const v2_header_block& trailers = {});
      //----------------------------------------------------------------//
    };
    //================================================================//
  }
}

#endif //MANIFOLD_HTTP_OUTGOING_MESSAGE_HPP
