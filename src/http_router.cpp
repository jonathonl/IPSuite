
#include "http_router.hpp"


namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    void router::register_handler(const std::regex& expression, const std::function<void(server::request&& req, server::response&& res, const std::smatch& matches)>& handler)
    {
      if (handler)
        this->routes_.emplace_back(expression, handler);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void router::register_handler(const std::regex& expression, const std::string& method, const std::function<void(server::request&& req, server::response&& res, const std::smatch& matches)>& handler)
    {
      if (handler)
        this->routes_.emplace_back(expression, handler, method);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void router::route(server::request&& req, server::response&& res)
    {
      bool both_matched = false;
      bool path_matched = false;

      std::string request_path = req.head().url(); // TODO: make url class and get path without query string
      std::string request_method = req.head().method();
      std::smatch sm;

      for (auto rt : this->routes_)
      {
        if (std::regex_match(request_path, sm, rt.expression))
        {
          if (rt.method.empty() || rt.method == request_method)
          {
            rt.handler(std::move(req), std::move(res), sm);
            both_matched = true;
          }
          path_matched = true;
        }
      }

      if (!both_matched)
      {
        if (path_matched)
        {
          // TODO: method not allowed.
        }
        else
        {
          // TODO: not found.
        }
      }
    }
    //----------------------------------------------------------------//
  }
}