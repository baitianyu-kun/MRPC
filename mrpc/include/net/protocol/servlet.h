//
// Created by baitianyu on 2/18/25.
//

#ifndef RPCFRAME_SERVLET_H
#define RPCFRAME_SERVLET_H

#include <memory>
#include <functional>
#include <map>
#include "net/protocol/http/http_define.h"
#include "net/protocol/protocol.h"
#include "common/mutex.h"

namespace mrpc {
    // 应该使用单例模式，整个系统里面应该只有一个
    class Servlet {
    public:
        using ptr = std::shared_ptr<Servlet>;

        explicit Servlet(const std::string &name) : m_name(name) {}

        virtual ~Servlet() {};

        virtual void handle(Protocol::ptr request, Protocol::ptr response, Session::ptr session) = 0;

        const std::string &getName() const { return m_name; }

    private:
        std::string m_name;
    };

    class CallBacksServlet : public Servlet {
    public:
        using ptr = std::shared_ptr<CallBacksServlet>;
        using callback = std::function<void(Protocol::ptr request, Protocol::ptr response, Session::ptr session)>;

        explicit CallBacksServlet(callback cb) : Servlet("CallBacksServlet"), m_callback(cb) {};

        void handle(Protocol::ptr request, Protocol::ptr response, Session::ptr session) override;

    private:
        callback m_callback;
    };

    // 负责分发给CallBacksServlet
    class DispatchServlet : public Servlet {
    public:
        using ptr = std::shared_ptr<DispatchServlet>;

    public:

        DispatchServlet();

        void handle(Protocol::ptr request, Protocol::ptr response, Session::ptr session) override;

        void addServlet(const std::string &uri, Servlet::ptr slt);

        void addServlet(const std::string &uri, CallBacksServlet::callback cb);

        void delServlet(const std::string &uri);

        Servlet::ptr getServlet(const std::string &uri);

        Servlet::ptr getDefault() const { return m_default; }

        void setDefault(Servlet::ptr def) { m_default = def; }

    private:
        std::map<std::string, Servlet::ptr> m_all_servlets;
        Servlet::ptr m_default;
    };

    class NotFoundServlet : public Servlet {
    public:
        using ptr = std::shared_ptr<NotFoundServlet>;

        explicit NotFoundServlet(const std::string &name);

        void handle(Protocol::ptr request, Protocol::ptr response, Session::ptr session) override;

    private:
        std::string m_name;
        std::string m_content;
    };
}

#endif //RPCFRAME_SERVLET_H
