//
// Created by Jonathon LeFaive on 1/3/16.
//


#include <algorithm>
#include <system_error>

#include "http_v1_header_block.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    v1_header_block::v1_header_block()
    {

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_header_block::~v1_header_block()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_header_block::header(const std::string& name, const std::string& value)
    {
      std::string n(name);
      std::string v(value);
      this->header(std::move(n), std::move(v));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_header_block::header(std::string&& name, std::string&& value)
    {
      // trim
      const std::string whitespace(" \t\f\v\r\n");
      name.erase(0, name.find_first_not_of(":" + whitespace));
      name.erase(name.find_last_not_of(whitespace)+1);
      value.erase(0, value.find_first_not_of(whitespace));
      value.erase(value.find_last_not_of(whitespace)+1);

      // make name lowercase
      std::for_each(name.begin(), name.end(), ::tolower);

      for (auto it = this->headers_.begin(); it != this->headers_.end();)
      {
        if (it->first == name)
          it = this->headers_.erase(it);
        else
          ++it;
      }
      this->headers_.push_back(std::pair<std::string,std::string>(std::move(name), std::move(value)));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_header_block::multi_header(const std::string& name, const std::list<std::string>& values)
    {
      std::string n(name);
      std::list<std::string> v(values);
      this->multi_header(std::move(n), std::move(v));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_header_block::multi_header(std::string&& name, std::list<std::string>&& values)
    {
      // trim
      const std::string whitespace(" \t\f\v\r\n");
      name.erase(0, name.find_first_not_of(":" + whitespace));
      name.erase(name.find_last_not_of(whitespace)+1);

      // make name lowercase
      std::for_each(name.begin(), name.end(), ::tolower);

      for (auto it = this->headers_.begin(); it != this->headers_.end();)
      {
        if (it->first == name)
          it = this->headers_.erase(it);
        else
          ++it;
      }

      std::for_each(values.begin(), values.end(), [this, &whitespace, &name](std::string& value)
      {
        value.erase(0, value.find_first_not_of(whitespace));
        value.erase(value.find_last_not_of(whitespace)+1);

        this->headers_.push_back(std::pair<std::string,std::string>(std::move(name), std::move(value)));
      });

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string v1_header_block::header(const std::string& name) const
    {
      std::string ret;
      std::string nameToLower(name);
      std::for_each(nameToLower.begin(), nameToLower.end(), ::tolower);

      for (auto it = this->headers_.rbegin(); ret.empty() && it != this->headers_.rend(); ++it)
      {
        if (it->first == nameToLower)
          ret = it->second;
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::list<std::string> v1_header_block::multi_header(const std::string& name) const
    {
      std::list<std::string> ret;
      std::string nameToLower(name);
      std::for_each(nameToLower.begin(), nameToLower.end(), ::tolower);

      for (auto it = this->headers_.begin(); it != this->headers_.end(); ++it)
      {
        if (it->first == nameToLower)
          ret.push_back(it->second);
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::list<std::pair<std::string,std::string>> v1_header_block::raw_headers() const
    {
      return this->headers_;
    }
    //----------------------------------------------------------------//

//    //----------------------------------------------------------------//
//    const std::string& v1_header_block::http_version() const
//    {
//      return this->version_;
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void  v1_header_block::http_version(const std::string& version)
//    {
//      this->version_ = version;
//    }
//    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_header_block::serialize(const v1_header_block& source, std::ostream& destination)
    {
      destination << source.start_line_ << "\r\n";

      for (auto it = source.headers_.begin(); it != source.headers_.end(); ++it)
      {
        destination << it->first << ": " << it->second << "\r\n";
      }

      destination << "\r\n";
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool v1_header_block::deserialize(std::istream& source, v1_header_block& destination)
    {
      std::getline(source, destination.start_line_);
      destination.start_line_.erase(destination.start_line_.find_last_not_of(" \r")+1);

      while (source.good())
      {
        std::string header_line;
        std::getline(source, header_line);
        header_line.erase(header_line.find_last_not_of("\r\n") + 1);

        if (header_line.empty())
          break;

        std::size_t colon_it = header_line.find(":");
        if (colon_it == std::string::npos)
          break;

        std::string name = header_line.substr(0, colon_it);
        name.erase(0, name.find_first_not_of(" "));
        name.erase(name.find_last_not_of(" ") + 1);

        std::string value = header_line.substr(++colon_it);
        value.erase(0, value.find_first_not_of(" "));
        value.erase(value.find_last_not_of(" ") + 1);
        destination.header(std::move(name), std::move(value));
      }

      return true;
    }
    //----------------------------------------------------------------//
  }
}