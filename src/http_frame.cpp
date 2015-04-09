
#include <new>

#include "http_frame.hpp"

namespace manifold
{
  namespace http
  {
    //****************************************************************//
    // frame_payload_base
    //----------------------------------------------------------------//
    void frame_payload_base::recv_frame_payload(asio::ip::tcp::socket& sock, frame_payload_base& destination, std::uint32_t payload_size, const std::function<void(const std::error_code& ec)>& cb)
    {
      destination.buf_.resize(payload_size);
      asio::async_read(sock, asio::buffer(destination.buf_.data(), payload_size), cb);
    }
    //----------------------------------------------------------------//
    //****************************************************************//



    //****************************************************************//
    // data_frame
    //----------------------------------------------------------------//
    const char*const data_frame::data() const
    {
      return this->buf_.data() + 1;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t data_frame::data_length() const
    {
      return (std::uint32_t)(this->buf_.size() - (this->pad_length() + 1));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const data_frame::padding() const
    {
      return this->buf_.data() + 1 + this->data_length();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t data_frame::pad_length() const
    {
      std::uint8_t ret;
      memcpy(&ret, this->buf_.data(), 1);
      return ret;
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // headers_frame
    //----------------------------------------------------------------//
    const char*const headers_frame::header_block_fragment() const
    {
      return this->buf_.data() + 6;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t headers_frame::header_block_fragment_length() const
    {
      return (std::uint32_t)(this->buf_.size() - (this->pad_length() + 6));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const headers_frame::padding() const
    {
      return this->buf_.data() + 6 + this->header_block_fragment_length();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t headers_frame::pad_length() const
    {
      std::uint8_t ret;
      memcpy(&ret, this->buf_.data(), 1);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t headers_frame::weight() const
    {
      std::uint8_t ret;
      memcpy(&ret, this->buf_.data() + 5, 1);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t headers_frame::stream_dependency_id() const
    {
      std::uint32_t ret;
      memcpy(&ret, this->buf_.data() + 1, 4);
      return (0x7FFFFFFF & ret);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool headers_frame::exclusive_stream_dependency() const
    {
      std::uint8_t tmp;
      memcpy(&tmp, this->buf_.data() + 1, 1);

      return (0x80 & tmp) != 0;
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // priority_frame
    //----------------------------------------------------------------//
    std::uint8_t priority_frame::weight() const
    {
      std::uint8_t ret;
      memcpy(&ret, this->buf_.data() + 4, 1);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t priority_frame::stream_dependency_id() const
    {
      std::uint32_t ret;
      memcpy(&ret, this->buf_.data(), 4);
      return (0x7FFFFFFF & ret);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool priority_frame::exclusive_stream_dependency() const
    {
      std::uint8_t tmp;
      memcpy(&tmp, this->buf_.data(), 1);

      return (0x80 & tmp) != 0;
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // rst_stream_frame
    //----------------------------------------------------------------//
    http::errc rst_stream_frame::error_code() const
    {
      http::errc ret;
      memcpy(&ret, this->buf_.data(), 4);
      return ret;
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // settings_frame
    //----------------------------------------------------------------//
    void settings_frame::deserialize_settings()
    {
      std::size_t bytesToParse = this->buf_.size();
      std::size_t pos = 0;
      while (bytesToParse > 6)
      {
        std::uint16_t key;
        std::uint32_t value;
        memcpy(&key, &this->buf_[pos], 2);
        memcpy(&key, &this->buf_[pos + 2], 4);
        this->settings_.push_back(std::pair<std::uint16_t,std::uint32_t>(key, value));
        pos = pos + 6;
        bytesToParse = bytesToParse - 6;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::vector<std::pair<std::uint16_t,std::uint32_t>>::const_iterator settings_frame::begin()
    {
      if (this->settings_.empty())
        this->deserialize_settings();
      return this->settings_.begin();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::vector<std::pair<std::uint16_t,std::uint32_t>>::const_iterator settings_frame::end()
    {
      if (this->settings_.empty())
        this->deserialize_settings();
      return this->settings_.end();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::size_t settings_frame::count()
    {
      if (this->settings_.empty())
        this->deserialize_settings();
      return this->settings_.size();
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // push_promise_frame
    //----------------------------------------------------------------//
    const char*const push_promise_frame::header_block_fragment() const
    {
      return (this->buf_.data() + 5);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t push_promise_frame::header_block_fragment_length() const
    {
      return (std::uint32_t)(this->buf_.size() - (this->pad_length() + 5));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const push_promise_frame::padding() const
    {
      return (this->buf_.data() + 5 + this->header_block_fragment_length());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t push_promise_frame::pad_length() const
    {
      std::uint8_t ret;
      memcpy(&ret, this->buf_.data(), 1);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t push_promise_frame::promised_stream_id() const
    {
      std::uint32_t ret;
      memcpy(&ret, this->buf_.data() + 1, 4);
      return (0x7FFFFFFF & ret);
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // goaway_frame
    //----------------------------------------------------------------//
    std::uint32_t goaway_frame::last_stream_id() const
    {
      std::uint32_t ret;
      memcpy(&ret, this->buf_.data(), 4);
      return (0x7FFFFFFF & ret);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    http::errc goaway_frame::error_code() const
    {
      http::errc ret;
      memcpy(&ret, this->buf_.data() + 4, 4);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const goaway_frame::additional_debug_data() const
    {
      return (this->buf_.data() + 8);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t goaway_frame::additional_debug_data_length() const
    {
      return (std::uint32_t)(this->buf_.size() - 8);
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // window_update_frame
    //----------------------------------------------------------------//
    std::uint32_t window_update_frame::window_size_increment() const
    {
      std::uint32_t ret;
      memcpy(&ret, this->buf_.data(), 4);
      return (0x7FFFFFFF & ret);
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // continuation_frame
    //----------------------------------------------------------------//
    const char*const continuation_frame::header_block_fragment() const
    {
      return this->buf_.data();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t continuation_frame::header_block_fragment_length() const
    {
      return (std::uint32_t)this->buf_.size();
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // frame
    //----------------------------------------------------------------//
    void frame::recv_frame(asio::ip::tcp::socket& sock, frame& destination, const std::function<void(const std::error_code& ec)>& cb)
    {
      asio::async_read(sock, asio::buffer(destination.metadata_.data(), 9), [&sock, &destination, cb](const std::error_code& ec, std::size_t bytes_read)
      {
        if (ec)
        {
          cb ? cb(ec) : void();
        }
        else
        {
          if (destination.is<http::data_frame>())
          {
            new (&destination.payload_.data_frame_) http::data_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.data_frame_, destination.payload_length(), cb);
          }
          else if (destination.is<http::headers_frame>())
          {
            new (&destination.payload_.headers_frame_) http::headers_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.headers_frame_, destination.payload_length(), cb);
          }
          else if (destination.is<http::priority_frame>())
          {
            new (&destination.payload_.priority_frame_) http::priority_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.priority_frame_, destination.payload_length(), cb);
          }
          else if (destination.is<http::rst_stream_frame>())
          {
            new (&destination.payload_.rst_stream_frame_) http::rst_stream_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.rst_stream_frame_, destination.payload_length(), cb);
          }
          else if (destination.is<http::settings_frame>())
          {
            new (&destination.payload_.settings_frame_) http::settings_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.settings_frame_, destination.payload_length(), cb);
          }
          else if (destination.is<http::push_promise_frame>())
          {
            new (&destination.payload_.push_promise_frame_) http::push_promise_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.push_promise_frame_, destination.payload_length(), cb);
          }
          else if (destination.is<http::ping_frame>())
          {
            new (&destination.payload_.ping_frame_) http::ping_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.ping_frame_, destination.payload_length(), cb);
          }
          else if (destination.is<http::goaway_frame>())
          {
            new (&destination.payload_.goaway_frame_) http::goaway_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.goaway_frame_, destination.payload_length(), cb);
          }
          else if (destination.is<http::window_update_frame>())
          {
            new (&destination.payload_.window_update_frame_) http::window_update_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.window_update_frame_, destination.payload_length(), cb);
          }
          else if (destination.is<http::continuation_frame>())
          {
            new (&destination.payload_.continuation_frame_) http::continuation_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.continuation_frame_, destination.payload_length(), cb);
          }
          else
          {
            // TODO: Invalid Frame Error;
            cb ? cb(std::make_error_code(std::errc::bad_message)) : void();
          }
        }
      });
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void frame::send_frame(asio::ip::tcp::socket& sock, const frame& source, const std::function<void(const std::error_code& ec)>& cb)
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t frame::payload_length()
    {
      std::uint32_t ret = 0;
      memcpy(&ret, this->metadata_.data(), 3);
      return ((ret >> 8) & 0x00FFFFFF);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame_type frame::type() const
    {
      frame_type ret;
      memcpy(&ret, this->metadata_.data() + 3, 1);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t frame::flags() const
    {
      std::uint8_t ret;
      memcpy(&ret, this->metadata_.data() + 4, 1);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t frame::stream_id() const
    {
      std::uint32_t ret;
      memcpy(&ret, this->metadata_.data() + 5, 4);
      return (0x7FFFFFFF & ret);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template<> bool frame::is<http::data_frame>()           const { return this->type() == frame_type::data           ; }
    template<> bool frame::is<http::headers_frame>()        const { return this->type() == frame_type::headers        ; }
    template<> bool frame::is<http::priority_frame>()       const { return this->type() == frame_type::priority       ; }
    template<> bool frame::is<http::rst_stream_frame>()     const { return this->type() == frame_type::rst_stream     ; }
    template<> bool frame::is<http::settings_frame>()       const { return this->type() == frame_type::settings       ; }
    template<> bool frame::is<http::push_promise_frame>()   const { return this->type() == frame_type::push_promise   ; }
    template<> bool frame::is<http::ping_frame>()           const { return this->type() == frame_type::ping           ; }
    template<> bool frame::is<http::goaway_frame>()        const { return this->type() == frame_type::goaway        ; }
    template<> bool frame::is<http::window_update_frame>()  const { return this->type() == frame_type::window_update  ; }
    template<> bool frame::is<http::continuation_frame>()   const { return this->type() == frame_type::continuation   ; }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const http::data_frame&           frame::data_frame()          const { return this->type() == frame_type::data           ? this->payload_.data_frame_           : frame::default_data_frame_          ; }
    const http::headers_frame&        frame::headers_frame()       const { return this->type() == frame_type::headers        ? this->payload_.headers_frame_        : frame::default_headers_frame_       ; }
    const http::priority_frame&       frame::priority_frame()      const { return this->type() == frame_type::priority       ? this->payload_.priority_frame_       : frame::default_priority_frame_      ; }
    const http::rst_stream_frame&     frame::rst_stream_frame()    const { return this->type() == frame_type::rst_stream     ? this->payload_.rst_stream_frame_     : frame::default_rst_stream_frame_    ; }
    const http::settings_frame&       frame::settings_frame()      const { return this->type() == frame_type::settings       ? this->payload_.settings_frame_       : frame::default_settings_frame_      ; }
    const http::push_promise_frame&   frame::push_promise_frame()  const { return this->type() == frame_type::push_promise   ? this->payload_.push_promise_frame_   : frame::default_push_promise_frame_  ; }
    const http::ping_frame&           frame::ping_frame()          const { return this->type() == frame_type::ping           ? this->payload_.ping_frame_           : frame::default_ping_frame_          ; }
    const http::goaway_frame&        frame::goaway_frame()       const { return this->type() == frame_type::goaway        ? this->payload_.goaway_frame_        : frame::default_goaway_frame_       ; }
    const http::window_update_frame&  frame::window_update_frame() const { return this->type() == frame_type::window_update  ? this->payload_.window_update_frame_  : frame::default_window_update_frame_ ; }
    const http::continuation_frame&   frame::continuation_frame()  const { return this->type() == frame_type::continuation   ? this->payload_.continuation_frame_   : frame::default_continuation_frame_  ; }
    //----------------------------------------------------------------//
    //****************************************************************//
  }
}