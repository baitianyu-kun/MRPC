//
// Created by baitianyu on 2/18/25.
//
#include "net/protocol/servlet.h"

namespace mrpc {
    void CallBacksServlet::handle(Protocol::ptr request, Protocol::ptr response, Session::ptr session) {
        m_callback(request, response, session);
    }

    DispatchServlet::DispatchServlet() : Servlet("DispatchServlet") {
        m_default.reset(new NotFoundServlet("404 not found"));
    }

    void DispatchServlet::handle(Protocol::ptr request, Protocol::ptr response, Session::ptr session) {
        auto servlet = getServlet(msgTypeToPath(request->m_type));
        if (servlet) {
            servlet->handle(request, response, session);
        }
    }

    // 主线程已经创建好了Servlet，并且已经addServlet完成(只有主线程，没有子线程)，所以子线程getServlet的时候，不会有其他子线程去add或者del
    // 所以暂时不需要加写锁
    void DispatchServlet::addServlet(const std::string &uri, Servlet::ptr slt) {
        // RWMutex::WriteLock lock(m_mutex);
        m_all_servlets[uri] = slt;
    }

    void DispatchServlet::addServlet(const std::string &uri, CallBacksServlet::callback cb) {
        // RWMutex::WriteLock lock(m_mutex);
        m_all_servlets[uri] = std::make_shared<CallBacksServlet>(cb);
    }

    void DispatchServlet::delServlet(const std::string &uri) {
        // RWMutex::WriteLock lock(m_mutex);
        m_all_servlets.erase(uri);
    }

    Servlet::ptr DispatchServlet::getServlet(const std::string &uri) {
        // RWMutex::WriteLock lock(m_mutex);
        auto it = m_all_servlets.find(uri);
        if (it == m_all_servlets.end()) {
            return nullptr;
        }
        return it->second;
    }

    NotFoundServlet::NotFoundServlet(const std::string &name) : Servlet("NotFoundServlet"), m_name(name) {
        m_content = "<html>"
                    "<head><title>404 Not Found</title></head>"
                    "<body>"
                    "<center><h1>404 Not Found</h1></center>"
                    "<hr><center>" + m_name + "</center>"
                                              "</body>"
                                              "</html>";
    }

    void NotFoundServlet::handle(Protocol::ptr request, Protocol::ptr response, Session::ptr session) {

    }
}