
#include <iostream>
#include <http_v2_request_head.hpp>
#include <http_v2_response_head.hpp>

#include "http_v2_connection.hpp"

#ifndef MANIFOLD_DISABLE_HTTP2

namespace manifold
{
  namespace http
  {
    template <typename SendMsg, typename RecvMsg> const std::uint32_t v2_connection<SendMsg, RecvMsg>::default_header_table_size      = 4096;
    template <typename SendMsg, typename RecvMsg> const std::uint32_t v2_connection<SendMsg, RecvMsg>::default_enable_push            = 1;
    template <typename SendMsg, typename RecvMsg> const std::uint32_t v2_connection<SendMsg, RecvMsg>::default_initial_window_size    = 65535;
    template <typename SendMsg, typename RecvMsg> const std::uint32_t v2_connection<SendMsg, RecvMsg>::default_max_frame_size         = 16384;

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::stream_dependency_tree()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::stream_dependency_tree(const std::vector<stream_dependency_tree_child_node>& children)
      : children_(children)
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    typename v2_connection<SendMsg, RecvMsg>::stream* v2_connection<SendMsg, RecvMsg>::stream_dependency_tree_child_node::stream_ptr() const
    {
      return this->stream_ptr_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    const std::vector<typename v2_connection<SendMsg, RecvMsg>::stream_dependency_tree_child_node>& v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::children() const
    {
      return this->children_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::insert_child(v2_connection<SendMsg, RecvMsg>::stream_dependency_tree_child_node&& child)
    {
      this->children_.push_back(std::move(child));
    }
    //----------------------------------------------------------------//

    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::remove(stream& stream_to_remove)
    {
      bool found = false;
      for (auto it = this->children_.begin(); it != this->children_.end() && found == false; ++it)
      {
        if (it->stream_ptr() == &stream_to_remove)
        {
          stream_dependency_tree tree_to_remove(std::move(*it));
          this->children_.erase(it);

          this->children_.reserve(this->children_.size() + tree_to_remove.children_.size());
          for (auto child_it = tree_to_remove.children_.begin(); child_it != tree_to_remove.children_.end(); ++child_it)
          {
            this->children_.push_back(std::move(*child_it));
          }
          tree_to_remove.children_.clear();
          found = true;
        }
      }

      for (auto it = this->children_.begin(); it != this->children_.end() && found == false; ++it)
        it->remove(stream_to_remove);
    }

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::clear_children()
    {
      for (auto it = this->children_.begin(); it != this->children_.end(); ++it)
        it->clear_children();
      this->children_.clear();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    const std::array<char,24> v2_connection<SendMsg, RecvMsg>::preface{{0x50, 0x52, 0x49, 0x20, 0x2a, 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x32, 0x2e, 0x30, 0x0d, 0x0a, 0x0d, 0x0a, 0x53, 0x4d, 0x0d, 0x0a, 0x0d, 0x0a}};
    template<> const std::uint32_t v2_connection<request_head, response_head>::initial_stream_id = 1;
    template<> const std::uint32_t v2_connection<response_head, request_head>::initial_stream_id = 2;
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    v2_connection<SendMsg, RecvMsg>::v2_connection(non_tls_socket&& sock)
      : socket_(new non_tls_socket(std::move(sock))),
        hpack_encoder_(4096),
        hpack_decoder_(4096),
        last_newly_accepted_stream_id_(0),
        next_stream_id_(initial_stream_id),
        outgoing_window_size_(default_initial_window_size),
        incoming_window_size_(default_initial_window_size),
        stream_dependency_tree_()
    {
      std::seed_seq seed({static_cast<std::uint32_t>((std::uint64_t)this)});
      this->rg_.seed(seed);

      // header_table_size      = 0x1, // 4096
      // enable_push            = 0x2, // 1
      // max_concurrent_streams = 0x3, // (infinite)
      // initial_window_size    = 0x4, // 65535
      // max_frame_size         = 0x5, // 16384
      // max_header_list_size   = 0x6  // (infinite)
      this->local_settings_ =
        {
          { setting_code::header_table_size,   default_header_table_size },
          { setting_code::enable_push,         default_enable_push },
          { setting_code::initial_window_size, default_initial_window_size },
          { setting_code::max_frame_size,      default_max_frame_size }
        };

      this->peer_settings_ = this->local_settings_;

      this->started_ = false;
      this->closed_ = false;
      this->send_loop_running_ = false;

    };
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    v2_connection<SendMsg, RecvMsg>::v2_connection(tls_socket&& sock)
      : socket_(new tls_socket(std::move(sock))),
        hpack_encoder_(4096),
        hpack_decoder_(4096),
        last_newly_accepted_stream_id_(0),
        next_stream_id_(initial_stream_id),
        outgoing_window_size_(default_initial_window_size),
        incoming_window_size_(default_initial_window_size),
        stream_dependency_tree_()
    {
      std::seed_seq seed({static_cast<std::uint32_t>((std::uint64_t)this)});
      this->rg_.seed(seed);

      // header_table_size      = 0x1, // 4096
      // enable_push            = 0x2, // 1
      // max_concurrent_streams = 0x3, // (infinite)
      // initial_window_size    = 0x4, // 65535
      // max_frame_size         = 0x5, // 16384
      // max_header_list_size   = 0x6  // (infinite)
      this->local_settings_ =
        {
          { setting_code::header_table_size,   default_header_table_size },
          { setting_code::enable_push,         default_enable_push },
          { setting_code::initial_window_size, default_initial_window_size },
          { setting_code::max_frame_size,      default_max_frame_size }
        };

      this->peer_settings_ = this->local_settings_;

      this->started_ = false;
      this->closed_ = false;
      this->send_loop_running_ = false;

    };
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    v2_connection<SendMsg, RecvMsg>::~v2_connection()
    {
      std::cout << "~v2_connection()" << std::endl;

      this->close(errc::cancel);

      if (this->socket_)
        delete this->socket_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::close(errc ec)
    {
      if (!this->closed_)
      {
        this->closed_ = true;

        //TODO: This needs to be fixed.

        this->send_goaway(ec);
        for (auto it = this->streams_.begin(); it != this->streams_.end(); ++it)
        {
          if (it->second.state() != stream_state::closed)
          {
            it->second.handle_outgoing_rst_stream_state_change(ec);
          }
        }

        this->on_close_ ? this->on_close_(ec) : void();
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::is_closed() const
    {
      return this->closed_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    typename v2_connection<SendMsg, RecvMsg>::stream* v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::get_next_send_stream_ptr(std::uint32_t connection_window_size, std::minstd_rand& rng)
    {
      stream* ret = nullptr;
      std::uint64_t weight_sum = 0;
      std::vector<stream_dependency_tree_child_node*> pool;

      for (auto it = this->children_.begin(); it != this->children_.end(); ++it)
      {
        if (it->check_for_outgoing_frame(connection_window_size > 0))
        {
          pool.push_back(&(*it));
          weight_sum += (it->stream_ptr()->weight + 1);
        }
      }

      if (pool.size())
      {
        std::uint64_t sum_index = (rng() % weight_sum) + 1;
        std::uint64_t current_sum = 0;
        for (auto it = pool.begin(); ret == nullptr && it != pool.end(); ++it)
        {
          stream_dependency_tree_child_node* current_pool_node = (*it);
          current_sum += (current_pool_node->stream_ptr()->weight + 1);
          if (sum_index <= current_sum)
          {
            ret = current_pool_node->get_next_send_stream_ptr(connection_window_size, rng);
          }
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    typename v2_connection<SendMsg, RecvMsg>::stream* v2_connection<SendMsg, RecvMsg>::stream_dependency_tree_child_node::get_next_send_stream_ptr(std::uint32_t connection_window_size, std::minstd_rand& rng)
    {
      // TODO: enforce a max tree depth of 10 to avoid stack overflow from recursion.
      stream* ret = nullptr;

      if (this->stream_ptr()->has_sendable_frame(connection_window_size > 0))
      {
        ret = this->stream_ptr();
      }
      else
      {
        ret = stream_dependency_tree::get_next_send_stream_ptr(connection_window_size, rng);
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream_dependency_tree_child_node::check_for_outgoing_frame(bool can_send_data)
    {
      bool ret = false;

      if (this->stream_ptr_->has_sendable_frame(can_send_data))
        ret = true;

      for (auto it = this->children_.begin(); !ret && it != this->children_.end(); ++it)
      {
        ret = it->check_for_outgoing_frame(can_send_data);
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::garbage_collect_streams()
    {
      for (auto it = this->streams_.begin(); it != this->streams_.end(); )
      {
        if (it->second.state() == stream_state::closed && !(it->second.has_sendable_frame(true)))
        {
          this->stream_dependency_tree_.remove(it->second);
          it = this->streams_.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template<> bool v2_connection<request_head, response_head>::receiving_push_promise_is_allowed()
    {
      bool ret = true;
      auto it = this->local_settings_.find(setting_code::enable_push);
      if (it != this->local_settings_.end() && it->second == 0)
      {
        ret = false;
      }
      return ret;
    }
    template<> bool v2_connection<response_head, request_head>::receiving_push_promise_is_allowed() { return false; }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template<> bool v2_connection<request_head, response_head>::sending_push_promise_is_allowed() { return false; }
    template<> bool v2_connection<response_head, request_head>::sending_push_promise_is_allowed()
    {
      bool ret = true;
      auto it = this->peer_settings_.find(setting_code::enable_push);
      if (it != this->peer_settings_.end() && it->second == 0)
      {
        ret = false;
      }
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::run_recv_loop()
    {
      if (!this->closed_)
      {
        std::shared_ptr<v2_connection> self = casted_shared_from_this();
        this->incoming_frame_ = frame(); // reset incoming frame
        frame::recv_frame(*this->socket_, this->incoming_frame_, [self](const std::error_code& ec)
        {
          if (ec)
          {
            self->close(errc::internal_error);
          }
          else if (!self->is_closed())
          {
            const frame_type incoming_frame_type = self->incoming_frame_.type();
            std::uint32_t incoming_stream_id = self->incoming_frame_.stream_id();
            if (incoming_stream_id)
            {
              auto current_stream_it = self->streams_.find(incoming_stream_id);

              if (current_stream_it == self->streams_.end())
              {
                if (incoming_stream_id > self->last_newly_accepted_stream_id_)
                {
                  self->last_newly_accepted_stream_id_ = incoming_stream_id;
                  if (!self->create_stream(0, incoming_stream_id))
                  {
                    self->close(errc::internal_error);
                  }
                  else
                  {
                    current_stream_it = self->streams_.find(incoming_stream_id);
                    self->on_new_stream_ ? self->on_new_stream_(incoming_stream_id) : void();
                  }
                  assert(current_stream_it != self->streams_.end());
                }
                else
                {
                  self->close(errc::protocol_error);
                }
              }

              if (current_stream_it == self->streams_.end())
              {
                // TODO: Handle Error.
              }
              else
              {
                if (self->incoming_header_block_fragments_.size() && (incoming_frame_type != frame_type::continuation || self->incoming_frame_.stream_id() != self->incoming_header_block_fragments_.front().stream_id()))
                {
                  // TODO: connection error PROTOCOL_ERROR
                }
                else if (incoming_frame_type == frame_type::continuation)
                {
                  if (self->incoming_header_block_fragments_.empty())
                  {
                    // TODO: connection error PROTOCOL_ERROR
                  }
                  else if (!self->incoming_frame_.continuation_frame().has_end_headers_flag())
                  {
                    self->incoming_header_block_fragments_.push(std::move(self->incoming_frame_));
                  }
                  else
                  {
                    errc handle_frame_conn_error = errc::no_error;
                    if (self->incoming_header_block_fragments_.front().type() == frame_type::headers)
                    {
                      headers_frame h_frame(std::move(self->incoming_header_block_fragments_.front().headers_frame()));
                      self->incoming_header_block_fragments_.pop();

                      std::vector<continuation_frame> cont_frames;
                      cont_frames.reserve(self->incoming_header_block_fragments_.size());
                      while (self->incoming_header_block_fragments_.size())
                      {
                        cont_frames.push_back(std::move(self->incoming_header_block_fragments_.front().continuation_frame()));
                        self->incoming_header_block_fragments_.pop();
                      }

                      current_stream_it->second.handle_incoming_frame(h_frame, cont_frames, self->hpack_decoder_, handle_frame_conn_error);
                    }
                    else
                    {
                      push_promise_frame pp_frame(std::move(self->incoming_header_block_fragments_.front().push_promise_frame()));
                      self->incoming_header_block_fragments_.pop();

                      std::vector<continuation_frame> cont_frames;
                      cont_frames.reserve(self->incoming_header_block_fragments_.size());
                      while (self->incoming_header_block_fragments_.size())
                      {
                        cont_frames.push_back(std::move(self->incoming_header_block_fragments_.front().continuation_frame()));
                        self->incoming_header_block_fragments_.pop();
                      }

                      if (pp_frame.promised_stream_id() <= self->last_newly_accepted_stream_id_)
                      {
                        self->close(errc::protocol_error);
                      }
                      else
                      {
                        self->last_newly_accepted_stream_id_ = pp_frame.promised_stream_id();
                        if (!self->create_stream(current_stream_it->second.id(), pp_frame.promised_stream_id()))
                        {
                          self->close(errc::internal_error);
                        }
                        else
                        {
                          auto promised_stream_it = self->streams_.find(pp_frame.promised_stream_id());
                          current_stream_it->second.handle_incoming_frame(pp_frame, cont_frames, self->hpack_decoder_, promised_stream_it->second, handle_frame_conn_error);
                        }
                      }
                    }
                  }
                }
                else if (incoming_frame_type == frame_type::headers || incoming_frame_type == frame_type::push_promise)
                {
                  if (!(self->receiving_push_promise_is_allowed()) && incoming_frame_type == frame_type::push_promise)
                  {
                    // TODO: Connection error.
                  }
                  else
                  {
                    bool has_end_headers_flag = (incoming_frame_type == frame_type::headers ? self->incoming_frame_.headers_frame().has_end_headers_flag() : self->incoming_frame_.push_promise_frame().has_end_headers_flag());

                    if (!has_end_headers_flag)
                      self->incoming_header_block_fragments_.push(std::move(self->incoming_frame_));
                    else if (incoming_frame_type == frame_type::headers)
                    {
                      errc handle_frame_conn_error = errc::no_error;
                      current_stream_it->second.handle_incoming_frame(self->incoming_frame_.headers_frame(), {}, self->hpack_decoder_, handle_frame_conn_error);
                      if (handle_frame_conn_error != errc::no_error)
                        self->close(handle_frame_conn_error);
                    }
                    else
                    {
                      if (self->incoming_frame_.push_promise_frame().promised_stream_id() <= self->last_newly_accepted_stream_id_)
                      {
                        self->close(errc::protocol_error);
                      }
                      else
                      {
                        self->last_newly_accepted_stream_id_ = self->incoming_frame_.push_promise_frame().promised_stream_id();
                        if (!self->create_stream(current_stream_it->second.id(), self->incoming_frame_.push_promise_frame().promised_stream_id()))
                        {
                          // TODO: Handle error.
                        }
                        else
                        {
                          errc handle_frame_conn_error = errc::no_error;
                          auto promised_stream_it = self->streams_.find(self->incoming_frame_.push_promise_frame().promised_stream_id());
                          current_stream_it->second.handle_incoming_frame(self->incoming_frame_.push_promise_frame(), {}, self->hpack_decoder_, promised_stream_it->second, handle_frame_conn_error);
                          if (handle_frame_conn_error != errc::no_error)
                            self->close(handle_frame_conn_error);
                        }
                      }
                    }
                  }
                }
                else
                {
                  errc handle_frame_conn_error = errc::no_error;
                  switch (incoming_frame_type)
                  {
                    case frame_type::data:
                      current_stream_it->second.handle_incoming_frame(self->incoming_frame_.data_frame(), handle_frame_conn_error);
                      break;
                    case frame_type::priority:
                      current_stream_it->second.handle_incoming_frame(self->incoming_frame_.priority_frame(), handle_frame_conn_error);
                      break;
                    case frame_type::rst_stream:
                      current_stream_it->second.handle_incoming_frame(self->incoming_frame_.rst_stream_frame(), handle_frame_conn_error);
                      break;
                    case frame_type::window_update:
                      current_stream_it->second.handle_incoming_frame(self->incoming_frame_.window_update_frame(), handle_frame_conn_error);
                      break;
                    default:
                    {
                      // ignore unknown frame type
                    }
                  }

                  if (handle_frame_conn_error != errc::no_error)
                    self->close(handle_frame_conn_error);
                }
              }
            }
            else
            {
              switch (incoming_frame_type)
              {
                case frame_type::settings:
                  self->handle_incoming_frame(self->incoming_frame_.settings_frame());
                  break;
                case frame_type::ping:
                  self->handle_incoming_frame(self->incoming_frame_.ping_frame());
                  break;
                case frame_type::goaway:
                  self->handle_incoming_frame(self->incoming_frame_.goaway_frame());
                  break;
                case frame_type::window_update:
                  self->handle_incoming_frame(self->incoming_frame_.window_update_frame());
                  break;
                default:
                {
                  // TODO: error stream-only frame missing stream id_
                }
              }
            }


            self->run_send_loop(); // One of the handle_incoming frames may have pushed an outgoing frame.
            self->run_recv_loop();
          }
        });
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template<> bool v2_connection<request_head, response_head>::stream::incoming_header_is_informational(const response_head& head) { return head.has_informational_status(); }
    template<> bool v2_connection<response_head, request_head>::stream::incoming_header_is_informational(const request_head& head) { return false; }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_data(const std::function<void(const char* const buf, std::size_t buf_size)>& fn)
    {
      this->on_data_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_headers(const std::function<void(RecvMsg&& headers)>& fn)
    {
      this->on_headers_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_informational_headers(const std::function<void(RecvMsg&& headers)>& fn)
    {
      this->on_informational_headers_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_trailers(const std::function<void(header_block&& headers)>& fn)
    {
      this->on_trailers_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_rst_stream(const std::function<void(std::uint32_t error_code)>& fn)
    {
      this->on_rst_stream_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_push_promise(const std::function<void(SendMsg&& headers, std::uint32_t promised_stream_id)>& fn)
    {
      this->on_push_promise_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_end(const std::function<void()>& fn)
    {
      if (this->state_ == stream_state::half_closed_remote || this->state_ == stream_state::closed || this->state_ == stream_state::reserved_local)
        fn ? fn() : void();
      else
        this->on_end_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_drain(const std::function<void()>& fn)
    {
      this->on_drain_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_close(const std::function<void(errc error_code)>& fn)
    {
      if (this->state_ == stream_state::closed)
        fn ? fn(errc::no_error) : void(); // TODO: pass error if there is one.
      else
        this->on_close_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::has_sendable_frame(bool can_send_data)
    {
      return (this->outgoing_non_data_frames.size()
        || (can_send_data && this->outgoing_data_frames.size() && this->outgoing_window_size > 0)
        || (this->outgoing_data_frames.size() && this->outgoing_data_frames.front().payload_length() == 0));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    frame v2_connection<SendMsg, RecvMsg>::stream::pop_next_outgoing_frame(std::uint32_t connection_window_size)
    {
      frame ret;
      if (this->outgoing_non_data_frames.size())
      {
        ret = std::move(this->outgoing_non_data_frames.front());
        this->outgoing_non_data_frames.pop();
      }
      else if (this->outgoing_data_frames.size() && this->outgoing_window_size > 0 && connection_window_size > 0)
      {
        if (connection_window_size > this->outgoing_window_size)
          connection_window_size = this->outgoing_window_size;
        if (this->outgoing_data_frames.front().data_frame().data_length() > connection_window_size)
          ret = frame(this->outgoing_data_frames.front().data_frame().split(connection_window_size), this->id_);
        else
          ret = std::move(this->outgoing_data_frames.front());
        this->outgoing_data_frames.pop();
        this->outgoing_window_size -= ret.data_frame().data_length();

        if (this->outgoing_data_frames.empty() && this->on_drain_)
          this->on_drain_();
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const data_frame& incoming_data_frame, errc& connection_error)
    {
      switch (this->state_)
      {
        case stream_state::open:
        {
          this->on_data_ ? this->on_data_(incoming_data_frame.data(), incoming_data_frame.data_length()) : void();

          if (incoming_data_frame.has_end_stream_flag())
          {
            this->state_ = stream_state::half_closed_remote;
            this->on_end_ ? this->on_end_() : void();
          }
          break;
        }
        case stream_state::half_closed_local:
        {
          this->on_data_ ? this->on_data_(incoming_data_frame.data(), incoming_data_frame.data_length()) : void();

          if (incoming_data_frame.has_end_stream_flag())
          {
            this->state_= stream_state::closed;
            this->on_end_ ? this->on_end_() : void();
            this->on_close_ ? this->on_close_(errc::no_error) : void();
          }
          break;
        }
        default:
        {
          // TODO: deal with frame / state_ mismatch
        }
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const headers_frame& incoming_headers_frame, const std::vector<continuation_frame>& continuation_frames, hpack::decoder& dec, errc& connection_error)
    {
      switch (this->state_)
      {
        case stream_state::idle:
        case stream_state::open:
        case stream_state::half_closed_local:
        case stream_state::reserved_remote:
        {


          std::string serialized_header_block;
          std::size_t serialized_header_block_sz = incoming_headers_frame.header_block_fragment_length();
          for (auto it = continuation_frames.begin(); it != continuation_frames.end(); ++it)
            serialized_header_block_sz += it->header_block_fragment_length();

          serialized_header_block.reserve(serialized_header_block_sz);

          serialized_header_block.append(incoming_headers_frame.header_block_fragment(), incoming_headers_frame.header_block_fragment_length());

          for (auto it = continuation_frames.begin(); it != continuation_frames.end(); ++it)
          {
            serialized_header_block.append(it->header_block_fragment(), it->header_block_fragment_length());
          }

          v2_header_block headers;
          if (!v2_header_block::deserialize(dec, serialized_header_block, headers))
            connection_error = errc::compression_error;
          else
          {
            if (this->state_ == stream_state::reserved_remote)
            {
              this->state_ = stream_state::half_closed_local;
            }
            else if (this->state_ == stream_state::idle)
            {
              this->state_ = (incoming_headers_frame.has_end_stream_flag() ? stream_state::half_closed_remote : stream_state::open );
            }
            else
            {
              if (incoming_headers_frame.has_end_stream_flag())
                this->state_ = (this->state_ == stream_state::half_closed_local ? stream_state::closed : stream_state::half_closed_remote);
            }

            RecvMsg generic_head(std::move(headers));
            if (incoming_header_is_informational(generic_head))
              this->on_informational_headers_ ? this->on_informational_headers_(std::move(generic_head)) : void();
            else if (!this->on_headers_called_)
            {
              this->on_headers_called_ = true;
              this->on_headers_ ? this->on_headers_(std::move(generic_head)) : void();
            }
            else
              this->on_trailers_ ? this->on_trailers_(std::move(generic_head)) : void();

            if (this->state_ == stream_state::closed)
            {
              this->on_end_ ? this->on_end_() : void();
              this->on_close_ ? this->on_close_(errc::no_error) : void();
            }
            else if (this->state_ == stream_state::half_closed_remote)
            {
              this->on_end_ ? this->on_end_() : void();
            }
          }
          break;
        }
        default:
        {
          // TODO: deal with frame / state_ mismatch
        }
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const priority_frame& incoming_priority_frame, errc& connection_error)
    {
      // TODO: implement.
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const rst_stream_frame& incoming_rst_stream_frame, errc& connection_error)
    {
      if (this->state_ != stream_state::closed)
      {
        this->state_= stream_state::closed;
        std::queue<frame> empty;
        std::swap(this->outgoing_data_frames, empty);
        this->on_close_ ? this->on_close_(int_to_errc(incoming_rst_stream_frame.error_code())) : void();
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::handle_incoming_frame(const settings_frame& incoming_settings_frame)
    {
      if (incoming_settings_frame.has_ack_flag())
      {
        // TODO: may need to add a pending queue for settings.
      }
      else
      {
        std::list<std::pair<std::uint16_t, std::uint32_t>> settings_list(incoming_settings_frame.settings());
        for (auto it = settings_list.begin(); it != settings_list.end(); ++it)
        {
          switch (it->first)
          {
            case (std::uint16_t)setting_code::header_table_size:
              this->peer_settings_[setting_code::header_table_size] = it->second;
              break;
            case (std::uint16_t)setting_code::enable_push:
              this->peer_settings_[setting_code::enable_push] = it->second;
              break;
            case (std::uint16_t)setting_code::max_concurrent_streams:
              this->peer_settings_[setting_code::max_concurrent_streams] = it->second;
              break;
            case (std::uint16_t)setting_code::initial_window_size:
              this->peer_settings_[setting_code::initial_window_size] = it->second;
              break;
            case (std::uint16_t)setting_code::max_frame_size:
              this->peer_settings_[setting_code::max_frame_size] = it->second;
              if (it->second < 4096)
                this->hpack_encoder_.add_table_size_update(it->second);
              else
                this->hpack_encoder_.add_table_size_update(4096);
              break;
            case (std::uint16_t)setting_code::max_header_list_size:
              this->peer_settings_[setting_code::max_header_list_size] = it->second;
              break;

          }
        }
        this->outgoing_frames_.push(http::frame(http::settings_frame(ack_flag()), 0x0));
      }
    }
    //----------------------------------------------------------------//

    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const push_promise_frame& incoming_push_promise_frame, const std::vector<continuation_frame>& continuation_frames, hpack::decoder& dec, stream& idle_promised_stream, errc& connection_error)
    {
      switch (this->state_)
      {
        case stream_state::half_closed_local:
        case stream_state::open:
        {
          std::string serialized_header_block;
          std::size_t serialized_header_block_sz = incoming_push_promise_frame.header_block_fragment_length();
          for (auto it = continuation_frames.begin(); it != continuation_frames.end(); ++it)
            serialized_header_block_sz += it->header_block_fragment_length();

          serialized_header_block.reserve(serialized_header_block_sz);

          serialized_header_block.append(incoming_push_promise_frame.header_block_fragment(), incoming_push_promise_frame.header_block_fragment_length());

          for (auto it = continuation_frames.begin(); it != continuation_frames.end(); ++it)
          {
            serialized_header_block.append(it->header_block_fragment(), it->header_block_fragment_length());
          }

          v2_header_block headers;
          if (!v2_header_block::deserialize(dec, serialized_header_block, headers))
            connection_error = errc::compression_error;
          else
          {
            idle_promised_stream.state_ = stream_state::reserved_remote;
            this->on_push_promise_ ? this->on_push_promise_(std::move(headers), incoming_push_promise_frame.promised_stream_id()) : void();
          }

          break;
        }
        case stream_state::closed:
        {
          if (true) // if stream was reset by me
          {
            if (true) //incoming_push_promise_frame.promised_stream_id() <= this->parent_connection_.last_newly_accepted_stream_id_)
            {
              //              this->parent_connection_.last_newly_accepted_stream_id_ = incoming_push_promise_frame.promised_stream_id();
              //              assert(this->parent_connection_.create_stream(this->parent_connection_.last_newly_accepted_stream_id_));
              //              auto it = this->parent_connection_.streams_.find(this->parent_connection_.last_newly_accepted_stream_id_);
              //              assert(it != this->parent_connection_.streams_.end());
              idle_promised_stream.state_ = stream_state::reserved_remote;
              idle_promised_stream.outgoing_non_data_frames.push(http::frame(http::rst_stream_frame(errc::refused_stream), idle_promised_stream.id_));
              //this->parent_connection_.send_reset_stream(this->parent_connection_.last_newly_accepted_stream_id_, errc::refused_stream);
            }
            else
            {
              // TODO: error promised stream_id is too low
            }
          }
          else
          {
            // TODO: handle error.
          }
          break;
        }
        default:
        {
          // TODO: deal with frame / state_ mismatch
        }
      }
    }

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::handle_incoming_frame(const ping_frame& incoming_ping_frame)
    {
      this->send_ping_acknowledgement(incoming_ping_frame.data());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::handle_incoming_frame(const goaway_frame& incoming_goaway_frame)
    {
      // TODO:
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::handle_incoming_frame(const window_update_frame& incoming_window_update_frame)
    {
      this->outgoing_window_size_ += incoming_window_update_frame.window_size_increment();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const window_update_frame& incoming_window_update_frame, errc& connection_error)
    {
      this->outgoing_window_size += incoming_window_update_frame.window_size_increment();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::handle_outgoing_headers_state_change()
    {
      switch (this->state_)
      {
        case stream_state::idle:
          this->state_ = stream_state::open;
          return true;
        case stream_state::reserved_local:
          this->state_ = stream_state::half_closed_remote;
          return true;
        case stream_state::reserved_remote:
        case stream_state::half_closed_local:
        case stream_state::closed:
          return false;
        case stream_state::open:
        case stream_state::half_closed_remote:
          return true;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::handle_outgoing_end_stream_state_change()
    {
      switch (this->state_)
      {
        case stream_state::open:
          this->state_ = stream_state::half_closed_local;
          return true;
        case stream_state::half_closed_remote:
          this->state_ = stream_state::closed;
          this->on_close_ ? this->on_close_(errc::no_error) : void();
          return true;
        case stream_state::reserved_remote:
        case stream_state::idle:
        case stream_state::reserved_local:
        case stream_state::half_closed_local:
        case stream_state::closed:
          return false;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::handle_outgoing_push_promise_state_change()
    {
      switch (this->state_)
      {
        case stream_state::idle:
          this->state_ = stream_state::reserved_local;
          return true;
        default:
          return false;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::handle_outgoing_rst_stream_state_change(errc ec)
    {
      switch (this->state_)
      {
        case stream_state::idle:
        case stream_state::closed:
          return false;
        default:
          this->state_ = stream_state::closed;
          std::queue<frame> empty;
          std::swap(empty, this->outgoing_data_frames);
          this->on_close_ ? this->on_close_(ec) : void();
          return true;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::run_send_loop()
    {
      if (!this->send_loop_running_)
      {
        this->send_loop_running_ = true;

        auto self = casted_shared_from_this();


        bool outgoing_frame_set = false;

        if (this->closed_)
        {
          while (this->outgoing_frames_.size())
          {
            if (!outgoing_frame_set && this->outgoing_frames_.front().type() == frame_type::goaway)
            {
              this->outgoing_frame_ = std::move(this->outgoing_frames_.front());
              outgoing_frame_set = true;
              frame::send_frame(*this->socket_, this->outgoing_frame_, [self](const std::error_code& ec)
              {
                self->socket_->close();
                self->stream_dependency_tree_.clear_children();
                self->streams_.clear();
                self->on_new_stream_ = nullptr;
                self->on_close_ = nullptr;
              });
            }
            this->outgoing_frames_.pop();
          }
        }
        else
        {
          if (this->outgoing_frames_.size())
          {
            this->outgoing_frame_ = std::move(this->outgoing_frames_.front());
            this->outgoing_frames_.pop();
            outgoing_frame_set = true;
          }
          else
          {
            stream* prioritized_stream_ptr = this->stream_dependency_tree_.get_next_send_stream_ptr(this->outgoing_window_size_, this->rg_);

            if (prioritized_stream_ptr)
            {
              this->outgoing_frame_ = std::move(prioritized_stream_ptr->pop_next_outgoing_frame(this->outgoing_window_size_));
              if (this->outgoing_frame_.type() == frame_type::data)
                this->outgoing_window_size_ -= this->outgoing_frame_.data_frame().data_length();
              outgoing_frame_set = true;
            }
          }

          if (outgoing_frame_set)
          {
            frame::send_frame(*this->socket_, this->outgoing_frame_, [self](const std::error_code& ec)
            {
              self->garbage_collect_streams();
              self->send_loop_running_ = false;
              if (ec)
              {
                self->close(errc::internal_error);
                std::cout << "ERROR " << __FILE__ << ":" << __LINE__ << " " << ec.message() << std::endl;
              }
              else
              {
                self->run_send_loop();
              }
            });
          }
          else
          {
            // All pending frames are sent so cleanup.
            //this->garbage_collect_streams();
            this->send_loop_running_ = false;
          }
        }
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::run()
    {
      if (!this->started_)
      {
        std::list<std::pair<std::uint16_t,std::uint32_t>> settings;
        for (auto it = this->local_settings_.begin(); it != this->local_settings_.end(); ++it)
          settings.emplace_back(static_cast<std::uint16_t>(it->first), it->second);
        this->send_settings(settings);
        this->run_recv_loop();

        this->started_ = true;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_new_stream(const std::function<void(std::uint32_t stream_id)>& fn)
    {
      this->on_new_stream_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_close(const std::function<void(errc ec)>& fn)
    {
      this->on_close_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_data(std::uint32_t stream_id, const std::function<void(const char* const buf, std::size_t buf_size)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_data(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_headers(std::uint32_t stream_id, const std::function<void(RecvMsg&& headers)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_headers(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_informational_headers(std::uint32_t stream_id, const std::function<void(RecvMsg&& headers)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_informational_headers(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_trailers(std::uint32_t stream_id, const std::function<void(header_block&& headers)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_trailers(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_close(std::uint32_t stream_id, const std::function<void(errc error_code)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_close(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_push_promise(std::uint32_t stream_id, const std::function<void(SendMsg&& headers, std::uint32_t promised_stream_id)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_push_promise(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_end(std::uint32_t stream_id, const std::function<void()>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_end(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_drain(std::uint32_t stream_id, const std::function<void()>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_drain(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    std::uint32_t v2_connection<SendMsg, RecvMsg>::create_stream(std::uint32_t dependency_stream_id, std::uint32_t stream_id) //TODO: allow for dependency other than root.
    {
      std::uint32_t ret = 0;

      //std::unique_ptr<v2_connection<SendMsg, RecvMsg>::stream> s(this->create_stream_object(stream_id));

      if (stream_id == 0)
        stream_id = this->get_next_stream_id();
      if (stream_id)
      {
        auto insert_res = this->streams_.emplace(stream_id, stream(stream_id, this->local_settings().at(setting_code::initial_window_size), this->peer_settings().at(setting_code::initial_window_size)));
        if (insert_res.second)
        {
          this->stream_dependency_tree_.insert_child(stream_dependency_tree_child_node(&(insert_res.first->second)));

          ret = insert_res.first->first;
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    std::uint32_t v2_connection<SendMsg, RecvMsg>::get_next_stream_id()
    {
      std::uint32_t ret = 0;
      if (this->next_stream_id_ <= max_stream_id)
      {
        ret = this->next_stream_id_;
        this->next_stream_id_ += 2;
      }
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <>
    bool v2_connection<request_head, response_head>::send_headers(std::uint32_t stream_id, const request_head& head, bool end_headers, bool end_stream)
    {
      return this->send_headers(stream_id, v2_request_head(head), end_headers, end_stream);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <>
    bool v2_connection<response_head, request_head>::send_headers(std::uint32_t stream_id, const response_head& head, bool end_headers, bool end_stream)
    {
      return this->send_headers(stream_id, v2_response_head(head), end_headers, end_stream);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_trailers(std::uint32_t stream_id, const header_block& head, bool end_headers, bool end_stream)
    {
      return this->send_headers(stream_id, v2_header_block(head), end_headers, end_stream);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_headers(std::uint32_t stream_id, const v2_header_block& head, bool end_headers, bool end_stream)
    {
      bool ret = false;

      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        std::string header_data;
        v2_header_block::serialize(this->hpack_encoder_, head, header_data);
        const std::uint8_t EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME = 0; //TODO: Set correct value
        if ((header_data.size() + EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME) > this->peer_settings_[setting_code::max_frame_size])
        {
          // TODO: Handle error
        }
        else
        {
          // TODO: check for errors below
          bool state_change_result = it->second.handle_outgoing_headers_state_change();
          if (end_stream && state_change_result)
            state_change_result = it->second.handle_outgoing_end_stream_state_change();

          if (state_change_result)
          {
            it->second.outgoing_non_data_frames.push(http::frame(http::headers_frame(header_data.data(), (std::uint32_t)header_data.size(), end_headers, end_stream), stream_id));
            this->run_send_loop();
            ret = true;
          }
          else
          {
            assert(!"Stream state_ change not allowed.");
          }
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_headers(std::uint32_t stream_id, const v2_header_block& head, priority_options priority, bool end_headers, bool end_stream)
    {
      bool ret = false;

     auto it = this->streams_.find(stream_id);

      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        std::string header_data;
        v2_header_block::serialize(this->hpack_encoder_, head, header_data);
        const std::uint8_t EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME = 0; //TODO: Set correct value
        if ((header_data.size() + EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME) > this->peer_settings_[setting_code::max_frame_size])
        {
          // TODO: Handle error
        }
        else
        {
          // TODO: check for errors below
          auto state_change_result = it->second.handle_outgoing_headers_state_change();
          if (end_stream && state_change_result)
            state_change_result = it->second.handle_outgoing_end_stream_state_change();

          if (state_change_result)
          {
            it->second.outgoing_non_data_frames.push(http::frame(http::headers_frame(header_data.data(), (std::uint32_t)header_data.size(), end_headers, end_stream, priority), stream_id));
            this->run_send_loop();
            ret = true;
          }
          else
          {
            assert(!"Stream state_ change not allowed.");
          }
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_priority(std::uint32_t stream_id, priority_options options)
    {
      bool ret = false;

      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        it->second.outgoing_non_data_frames.push(http::frame(http::priority_frame(options), stream_id));
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_reset_stream(std::uint32_t stream_id, http::errc error_code)
    {
      bool ret = false;

      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        if (it->second.handle_outgoing_rst_stream_state_change(error_code))
        {
          it->second.outgoing_non_data_frames.push(http::frame(http::rst_stream_frame(error_code), stream_id));
          this->run_send_loop();
          ret = true;
        }
        else
        {
          assert(!"Stream state_ change not allowed.");
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::send_settings(const std::list<std::pair<std::uint16_t,std::uint32_t>>& settings)
    {
      this->outgoing_frames_.push(http::frame(http::settings_frame(settings.begin(), settings.end()), 0x0));
      this->run_send_loop();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_countinuation(std::uint32_t stream_id, const v2_header_block& head, bool end_headers)
    {
      bool ret = false;

      auto it = this->streams_.find(stream_id);

      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        std::string header_data;
        v2_header_block::serialize(this->hpack_encoder_, head, header_data);
        if (header_data.size() > this->peer_settings_[setting_code::max_frame_size])
        {
          // TODO: Handle error
        }
        else
        {
          it->second.outgoing_non_data_frames.push(http::frame(http::continuation_frame(header_data.data(), (std::uint32_t)header_data.size(), end_headers), stream_id));
          this->run_send_loop();
          ret = true;
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_data(std::uint32_t stream_id, const char *const data, std::uint32_t data_sz, bool end_stream)
    {
      bool ret = false;

      auto it = this->streams_.find(stream_id);

      if (it != this->streams_.end())
      {
        bool state_change_result = true;
        if (end_stream)
          state_change_result = it->second.handle_outgoing_end_stream_state_change();

        if (state_change_result)
        {
          it->second.outgoing_data_frames.push(http::frame(http::data_frame(data, data_sz, end_stream), stream_id));
          this->run_send_loop();
          ret = true;
        }
        else
        {
          assert(!"Stream state change not allowed.");
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    std::uint32_t v2_connection<SendMsg, RecvMsg>::send_push_promise(std::uint32_t stream_id, const RecvMsg& head)
    {
      std::uint32_t promised_stream_id = 0;

      if (this->sending_push_promise_is_allowed())
      {
        auto it = this->streams_.find(stream_id);

        if (it == this->streams_.end())
        {
          // TODO: Handle error
        }
        else
        {
          v2_header_block v2_head(head);
          std::string header_data;
          v2_header_block::serialize(this->hpack_encoder_, v2_head, header_data);
          const std::uint8_t EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME = 0; //TODO: Set correct value
          if ((header_data.size() + EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME) > this->peer_settings_[setting_code::max_frame_size])
          {
            // TODO: Handle error
          }
          else
          {
            promised_stream_id = this->create_stream(stream_id, 0);

            if (!promised_stream_id)
            {
              // TODO: stream_id's exhausted.
            }
            else
            {
              this->streams_.at(promised_stream_id).handle_outgoing_push_promise_state_change();

              it->second.outgoing_non_data_frames.push(http::frame(http::push_promise_frame(header_data.data(), (std::uint32_t) header_data.size(), promised_stream_id, true), stream_id));
              this->run_send_loop();
            }
          }
        }
      }

      return promised_stream_id;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::send_ping(std::uint64_t opaque_data)
    {
      this->outgoing_frames_.push(http::frame(http::ping_frame(opaque_data), 0x0));
      this->run_send_loop();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::send_ping_acknowledgement(std::uint64_t opaque_data)
    {
      this->outgoing_frames_.push(http::frame(http::ping_frame(opaque_data, true), 0x0));
      this->run_send_loop();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::send_goaway(http::errc error_code, const char *const data, std::uint32_t data_sz)
    {
      this->outgoing_frames_.push(http::frame(http::goaway_frame(this->last_newly_accepted_stream_id_, error_code, data, data_sz), 0x0));
      this->run_send_loop();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_window_update(std::uint32_t stream_id, std::uint32_t amount)
    {
      bool ret = false;

      auto it = this->streams_.find(stream_id);
      if (it != this->streams_.end())
      {
        it->second.outgoing_non_data_frames.push(http::frame(http::window_update_frame(amount), stream_id));
        this->run_send_loop();
        ret = true;
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::send_connection_level_window_update(std::uint32_t amount)
    {
      this->outgoing_frames_.push(http::frame(http::window_update_frame(amount), 0x0));
      this->run_send_loop();
    }
    //----------------------------------------------------------------//

    template class v2_connection<request_head, response_head>;
    template class v2_connection<response_head, request_head>;

//    //----------------------------------------------------------------//
//    void connection::send(char* buf, std::size_t buf_size, const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& handler)
//    {
//      this->socket_.async_send(asio::buffer(buf, buf_size), handler);
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void connection::recv(const char* buf, std::size_t buf_size, const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& handler)
//    {
//      this->socket_.async_send(asio::buffer(buf, buf_size), handler);
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void connection::recvMessageHead()
//    {
//      std::shared_ptr<connection> self = this->shared_from_this();
//
//
//      TCP::recvline(this->socket_, this->incomingHeadBuffer_.data(), this->incomingHeadBuffer_.size(), [self](const std::error_code& ec, std::size_t bytes_transferred)
//      {
//        if (ec)
//        {
//          std::cout << ec.message() << ":" __FILE__ << "/" << __LINE__ << std::endl;
//        }
//        else
//        {
//
//          http::request_head requestHead;
//          v2_header_block::deserialize(std::string(self->incomingHeadBuffer_.data(), bytes_transferred), requestHead);
//
//          std::cout << requestHead.url() << ":" __FILE__ << "/" << __LINE__ << std::endl;
//
//          this->requests_.emplace(std::make_shared<http::server::request>(requestHead, this->socket_));
//        }
//      }, "\r\n\r\n");
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void connection::close()
//    {
//      this->socket_.close();
//      this->server_.httpSessions_.erase(std::shared_ptr<Session>(this));
//    }
//    //----------------------------------------------------------------//
  }
}

#endif //MANIFOLD_DISABLE_HTTP2