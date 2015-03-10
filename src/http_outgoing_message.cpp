
#include "http_outgoing_message.hpp"

namespace IPSuite
{
  namespace HTTP
  {
    //----------------------------------------------------------------//
    OutgoingMessage::OutgoingMessage(MessageHead& head, Socket&& sock)
      : head_(head), transferEncoding_(TransferEncoding::Unknown)
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    OutgoingMessage::~OutgoingMessage()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    ssize_t OutgoingMessage::send(const char* data, std::size_t dataSize)
    {

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    ssize_t OutgoingMessage::sendChunkedEntity(const char* data, std::size_t dataSize)
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    ssize_t OutgoingMessage::sendKnownLengthEntity(const char* data, std::size_t dataSize)
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool OutgoingMessage::sendHead()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void OutgoingMessage::end()
    {
    }
    //----------------------------------------------------------------//
  }
}