
#include <memory>
#include <array>
#include <iostream>

#include "asio.hpp"
#include "tcp.hpp"
#include "http_server.hpp"
#include "http_connection.hpp"

namespace manifold
{
  namespace http
  {
    int alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
      unsigned char *out_len, const unsigned char *in,
      unsigned int in_len, void *arg)
    {
      static const char*const h2_proto_string = "\x02h2\x08http/1.1";
      std::size_t h2_proto_string_len = ::strlen(h2_proto_string);

      int ret = SSL_select_next_proto((unsigned char **)out, out_len, (unsigned char*)h2_proto_string, h2_proto_string_len, in, in_len) == OPENSSL_NPN_NEGOTIATED ? SSL_TLSEXT_ERR_OK : SSL_TLSEXT_ERR_ALERT_FATAL;
      auto select_proto = *out;
      int e = SSL_get_error(ssl, ret);
      return  ret;
//      const unsigned char* client_proto = in;
//      const unsigned char* client_proto_end = in + in_len;
//      for ( ; client_proto + h2_proto_string_len <= client_proto_end; client_proto += *client_proto + 1)
//      {
//        std::size_t client_proto_len = (*client_proto + 1);
//        if (::memcmp(h2_proto_string, client_proto, h2_proto_string_len <  client_proto_len ? h2_proto_string_len : client_proto_len) == 0)
//        {
//          *out = client_proto + 1;
//          *out_len = (unsigned char)(client_proto_len - 1);
//          return SSL_TLSEXT_ERR_OK;
//        }
//      }
      return SSL_TLSEXT_ERR_NOACK;
    }

    //----------------------------------------------------------------//
    server::request::request(request_head&& head, const std::shared_ptr<http::connection>& conn, std::int32_t stream_id)
      : incoming_message(conn, stream_id)
    {
      this->head_ = std::move(head);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    server::request::~request()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const request_head& server::request::head() const
    {
      return this->head_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    server::response::response(response_head&& head, const std::shared_ptr<http::connection>& conn, std::int32_t stream_id)
      : outgoing_message(conn, stream_id)
    {
      this->head_ = std::move(head);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    server::response::~response()
    {

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    response_head& server::response::head()
    {
      return this->head_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool server::response::send_headers(bool end_stream)
    {
      if (this->head().header("date").empty())
        this->head().header("date", server::date_string());
      return outgoing_message::send_headers(end_stream);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    server::response server::response::make_push_response(request_head&& push_promise_headers)
    {
      std::uint32_t stream_id = this->connection_->create_stream(this->stream_id_, 0);

      if (stream_id)
        this->connection_->send_push_promise(this->stream_id_, std::move(push_promise_headers), stream_id, true);

      response ret(http::response_head(200, {{"server", this->head().header("server")}}), this->connection_, stream_id);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    server::response server::response::make_push_response(const request_head& push_promise_headers)
    {
      return this->make_push_response(request_head(push_promise_headers));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    server::server(asio::io_service& ioService, unsigned short port, const std::string& host)
      : io_service_(ioService),
        acceptor_(io_service_)
    {
      this->port_ = port;
      this->host_ = host;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    server::server(asio::io_service& ioService, ssl_options options, unsigned short port, const std::string& host)
      : io_service_(ioService),
      acceptor_(io_service_),
      ssl_context_(new asio::ssl::context(options.method))
    {
//      this->ssl_context_->set_options(
//        asio::ssl::context::default_workarounds
//          | asio::ssl::context::no_sslv2
//          | asio::ssl::context::single_dh_use);
      if (true) //options.pfx.size())
      {
        //this->ssl_context_->use_certificate_chain_file("/Users/jonathonl/Developer/certs2/server-cert.pem");
        //this->ssl_context_->use_private_key_file("/Users/jonathonl/Developer/certs2/server-key.pem", asio::ssl::context::pem);
        //this->ssl_context_->use_tmp_dh_file("/Users/jonathonl/Developer/certs/dh512.pem");

        char cwd[FILENAME_MAX];
        getcwd(cwd, FILENAME_MAX);

        //this->ssl_context_->use_certificate_chain_file("tests/certs/server.crt");
        //this->ssl_context_->use_private_key_file("tests/certs/server.key", asio::ssl::context::pem);
        //this->ssl_context_->use_tmp_dh_file("tests/certs/dh2048.pem");

        this->ssl_context_->use_certificate_chain(asio::buffer(options.chain.data(), options.chain.size()));
        this->ssl_context_->use_private_key(asio::buffer(options.key.data(), options.key.size()), asio::ssl::context::pem);
        this->ssl_context_->use_tmp_dh(asio::buffer(options.dhparam.data(), options.dhparam.size()));
      }
      else
      {
        std::error_code ec;
        if (options.cert.size())
          this->ssl_context_->use_certificate_chain(asio::const_buffer(options.cert.data(), options.cert.size()));
        if (options.key.size())
          this->ssl_context_->use_private_key(asio::const_buffer(options.key.data(), options.key.size()), asio::ssl::context::file_format::pem);
        if (options.ca.size())
          this->ssl_context_->use_tmp_dh_file("/Users/jonathonl/Developer/certs/dh512.pem");
      }


      auto ssl_opts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
        SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |
        SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
        SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_TICKET |
        SSL_OP_CIPHER_SERVER_PREFERENCE;

      SSL_CTX_set_options(ssl_context_->native_handle(), SSL_CTX_get_options(ssl_context_->native_handle()) | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
      //SSL_CTX_set_options(ssl_context_->impl(), ssl_opts);
      //SSL_CTX_set_mode(ssl_context_->impl(), SSL_MODE_AUTO_RETRY);
      //SSL_CTX_set_mode(ssl_context_->impl(), SSL_MODE_RELEASE_BUFFERS);

      static const char *const DEFAULT_CIPHER_LIST =
        //"HIGH:!AES256-SHA:!AES128-GCM-SHA256:!AES128-SHA:!DES-CBC3-SHA";
        //"DHE:EDH:kDHE:kEDH:DH:kEECDH:kECDHE:ECDHE:EECDH:ECDH";
        "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-"
        "AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:"
        "DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-"
        "AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-"
        "AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-"
        "AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:"
        "DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:"
        "!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";



      //SSL_CTX_set_cipher_list(ssl_context_->impl(), DEFAULT_CIPHER_LIST);


      SSL_CTX_set_alpn_select_cb(this->ssl_context_->impl(), alpn_select_proto_cb, nullptr);
      this->port_ = port;
      this->host_ = host;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    server::~server()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void server::listen(const std::function<void(server::request&& req, server::response&& res)>& handler)
    {
      this->request_handler_ = handler;
      // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
      //asio::ip::tcp::resolver resolver(io_service_);
      //asio::ip::tcp::endpoint endpoint = *(resolver.resolve({host, std::to_string(port)}));
      auto ep = asio::ip::tcp::endpoint(asio::ip::address::from_string(this->host_), this->port_);
      acceptor_.open(ep.protocol());
      acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
      acceptor_.bind(ep);
      acceptor_.listen();

      if (this->ssl_context_)
        this->accept(*this->ssl_context_);
      else
        this->accept();
    };
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void server::accept()
    {
      auto sock = std::make_shared<non_tls_socket>(this->io_service_);
      acceptor_.async_accept((asio::ip::tcp::socket&)*sock, [this, sock](std::error_code ec)
      {

        if (!acceptor_.is_open())
        {
          std::cout << "acceptor not open" << ":" __FILE__ << "/" << __LINE__ << std::endl;
          return;
        }

        if (!ec)
        {
          auto* preface_buf = new std::array<char,connection::preface.size()>();
          sock->recv(preface_buf->data(), connection::preface.size(), [this, sock, preface_buf](const std::error_code& ec, std::size_t bytes_read)
          {
            if (ec)
            {
              std::cout << ec.message() << ":" __FILE__ << "/" << __LINE__ << std::endl;
            }
            else
            {
              if (*preface_buf != connection::preface)
              {
                std::cout << "Invalid Connection Preface" << ":" __FILE__ << "/" << __LINE__ << std::endl;
              }
              else
              {
                auto it = this->connections_.emplace(std::make_shared<connection>(std::move(*sock)));
                if (it.second)
                {
                  this->manage_connection(*it.first);
                  (*it.first)->run();
                }
              }
            }
            delete preface_buf;
          });
        }
        else
        {
          std::cout << ec.message() << ":" __FILE__ << "/" << __LINE__ << std::endl;
        }

        if (!this->io_service_.stopped())
          this->accept();
      });
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void server::accept(asio::ssl::context& ctx)
    {
      auto sock = std::make_shared<tls_socket>(this->io_service_, ctx);
      acceptor_.async_accept(((asio::ssl::stream<asio::ip::tcp::socket>&)*sock).lowest_layer(), [this, sock, &ctx](std::error_code ec)
      {

        if (!acceptor_.is_open())
        {
          std::cout << "acceptor not open" << ":" __FILE__ << "/" << __LINE__ << std::endl;
          return;
        }

        if (ec)
        {

        }
        else
        {
          ((asio::ssl::stream<asio::ip::tcp::socket>&)*sock).async_handshake(asio::ssl::stream_base::server, [this, sock] (const std::error_code& ec)
          {
            std::cout << "Cipher: " << SSL_CIPHER_get_name(SSL_get_current_cipher(((asio::ssl::stream<asio::ip::tcp::socket>&)*sock).native_handle())) << std::endl;
            const unsigned char* selected_alpn = nullptr;
            unsigned int selected_alpn_sz = 0;
            SSL_get0_alpn_selected(((asio::ssl::stream<asio::ip::tcp::socket>&)*sock).native_handle(), &selected_alpn, &selected_alpn_sz);
            std::cout << "Server ALPN: " << std::string((char*)selected_alpn, selected_alpn_sz) << std::endl;
            if (ec)
            {
              std::cout << ec.message() << ":" __FILE__ << "/" << __LINE__ << std::endl;
            }
            else
            {
              auto* preface_buf = new std::array<char,connection::preface.size()>();
              sock->recv(preface_buf->data(), connection::preface.size(), [this, sock, preface_buf](const std::error_code& ec, std::size_t bytes_read)
              {
                if (ec)
                {
                  std::cout << ec.message() << ":" __FILE__ << "/" << __LINE__ << std::endl;
                  std::string err = ec.message();
                  if (ec.category() == asio::error::get_ssl_category())
                  {
                    err = std::string(" (");
                    //ERR_PACK /* crypto/err/err.h */
                    char buf[128];
                    ::ERR_error_string_n(ec.value(), buf, sizeof(buf));
                    err += buf;
                  }
                }
                else
                {
                  const char* t = preface_buf->data();
                  if (*preface_buf != connection::preface)
                  {
                    std::cout << "Invalid Connection Preface" << ":" __FILE__ << "/" << __LINE__ << std::endl;
                  }
                  else
                  {
                    auto it = this->connections_.emplace(std::make_shared<connection>(std::move(*sock)));
                    if (it.second)
                    {
                      this->manage_connection(*it.first);
                      (*it.first)->run();
                    }
                  }
                }
                delete preface_buf;
              });
            }
          });
        }

        if (!this->io_service_.stopped())
          this->accept(ctx);
      });
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void server::manage_connection(const std::shared_ptr<http::connection>& conn)
    {
      conn->on_new_stream([this, conn](std::int32_t stream_id)
      {
        conn->on_headers(stream_id, [conn, stream_id, this](http::header_block&& headers)
        {
          this->request_handler_ ? this->request_handler_(server::request(std::move(headers), conn, stream_id), server::response(http::response_head(200, {{"server", this->default_server_header_}}), conn, stream_id)) : void();
        });

        conn->on_push_promise(stream_id, [stream_id, conn](http::request_head&& head, std::uint32_t promised_stream_id)
        {
          conn->send_goaway(errc::protocol_error, "Clients Cannot Push!");
        });
      });

      conn->on_close([conn, this](std::uint32_t ec)
      {
        this->connections_.erase(conn);
      });
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void server::set_default_server_header(const std::string& value)
    {
      this->default_server_header_ = value;
    }
    //----------------------------------------------------------------//
  }
}
