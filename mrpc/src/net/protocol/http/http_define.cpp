#include "net/protocol/http/http_define.h"

//
// Created by baitianyu on 2/8/25.
//
#include <sstream>
#include "net/protocol/http/http_define.h"
#include "common/log.h"
#include "common/string_util.h"

namespace mrpc {

    const char *HTTPCodeToString(const int code) {
        switch (code) {
            case HTTP_OK:
                return "OK";
            case HTTP_BAD_REQUEST:
                return "Bad Request";
            case HTTP_FORBIDDEN:
                return "Forbidden";
            case HTTP_NOTFOUND:
                return "Not Found";
            case HTTP_INTERNAL_SERVER_ERROR:
                return "Internal Server Error";
            default:
                return "UnKnown code";
        }
    }

    HTTPCode stringToHTTPCode(std::string &code) {
        if (code == "200") {
            return HTTP_OK;
        } else if (code == "400") {
            return HTTP_BAD_REQUEST;
        } else if (code == "403") {
            return HTTP_FORBIDDEN;
        } else if (code == "404") {
            return HTTP_NOTFOUND;
        } else if (code == "500") {
            return HTTP_INTERNAL_SERVER_ERROR;
        } else {
            return HTTP_UNKNOWN_ERROR;
        }
    }

    const char *HTTPMethodToString(HTTPMethod method) {
        switch (method) {
            case HTTPMethod::GET:
                return "GET";
            case HTTPMethod::POST:
                return "POST";
            default:
                return "Unknown HTTP Method";
        }
    }

    void HTTPHeaderProp::setKeyValue(const std::string &key, const std::string &value) {
        m_map_properties[key] = value;
    }

    std::string HTTPHeaderProp::getValue(const std::string &key) {
        return m_map_properties.at(key);
    }

    std::string HTTPHeaderProp::toHTTPString() {
        std::stringstream ss;
        for (const auto &item: m_map_properties) {
            ss << item.first << ":" << item.second << g_CRLF;
        }
        return ss.str();
    }

    std::string HTTPRequest::toString() {
        std::stringstream ss;
        ss << HTTPMethodToString(m_request_method) << " "
           << m_request_path << " "
           << m_request_version << g_CRLF
           << m_request_properties.toHTTPString() << g_CRLF
           << m_request_body;
        DEBUGLOG("HTTP request encode success");
        return ss.str();
    }

    std::string HTTPResponse::toString() {
        std::stringstream ss;
        ss << m_response_version << " "
           << m_response_code << " "
           << m_response_info << g_CRLF
           << m_response_properties.toHTTPString() << g_CRLF
           << m_response_body;
        DEBUGLOG("HTTP response encode success");
        return ss.str();
    }

    void HTTPManager::createRequest(HTTPRequest::ptr request, MSGType type, body_type &body) {
        switch (type) {
            case MSGType::RPC_METHOD_REQUEST:
                createMethodRequest(request, body);
                return;
            case MSGType::RPC_REGISTER_HEART_SERVER_REQUEST:
                createHeartRequest(request, body);
                return;
            case MSGType::RPC_SERVER_REGISTER_REQUEST:
                createRegisterRequest(request, body);
                return;
            case MSGType::RPC_CLIENT_REGISTER_DISCOVERY_REQUEST:
                createDiscoveryRequest(request, body);
                return;
            case MSGType::RPC_CLIENT_REGISTER_SUBSCRIBE_REQUEST:
                createSubscribeRequest(request, body);
                return;
            case MSGType::RPC_REGISTER_CLIENT_PUBLISH_REQUEST:
                createPublishRequest(request, body);
                return;
        }
    }

    void
    HTTPManager::createResponse(HTTPResponse::ptr response, MSGType type, body_type &body) {
        switch (type) {
            case MSGType::RPC_METHOD_RESPONSE:
                createMethodResponse(response, body);
                return;
            case MSGType::RPC_REGISTER_HEART_SERVER_RESPONSE:
                createHeartResponse(response, body);
                return;
            case MSGType::RPC_SERVER_REGISTER_RESPONSE:
                createRegisterResponse(response, body);
                return;
            case MSGType::RPC_CLIENT_REGISTER_DISCOVERY_RESPONSE:
                createDiscoveryResponse(response, body);
                return;
            case MSGType::RPC_CLIENT_REGISTER_SUBSCRIBE_RESPONSE:
                createSubscribeResponse(response, body);
                return;
            case MSGType::RPC_REGISTER_CLIENT_PUBLISH_RESPONSE:
                createPublishResponse(response, body);
                return;
        }
    }

    void HTTPManager::createDefaultResponse(HTTPResponse::ptr response) {
        std::string body_str = default_html_template;
        response->m_response_body = body_str;
        response->m_response_version = "HTTP/1.1";
        response->m_response_code = HTTPCode::HTTP_OK;
        response->m_response_info = HTTPCodeToString(HTTPCode::HTTP_OK);
        response->m_response_properties.m_map_properties["Content-Type"] = content_type_text;
        response->m_response_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
    }

    void HTTPManager::createMethodRequest(HTTPRequest::ptr request, body_type &body) {
        request->m_msg_id = MSGIDUtil::GenerateMSGID();
        std::string body_str = "method_full_name:" + body["method_full_name"] + g_CRLF
                               + "pb_data:" + body["pb_data"] + g_CRLF
                               + "msg_id:" + request->m_msg_id;
        request->m_request_body = body_str;
        request->m_request_method = HTTPMethod::POST;
        request->m_request_version = "HTTP/1.1";
        request->m_request_path = RPC_METHOD_PATH;
        request->m_request_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        request->m_request_properties.m_map_properties["Content-Type"] = content_type_text;
    }

    void HTTPManager::createHeartRequest(HTTPRequest::ptr request, body_type &body) {
        request->m_msg_id = MSGIDUtil::GenerateMSGID();
        std::string body_str = "msg_id:" + request->m_msg_id + g_CRLF
                               + "server_ip:" + body["server_ip"] + g_CRLF
                               + "server_port:" + body["server_port"];
        request->m_request_body = body_str;
        request->m_request_method = HTTPMethod::POST;
        request->m_request_version = "HTTP/1.1";
        request->m_request_path = RPC_REGISTER_HEART_SERVER_PATH;
        request->m_request_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        request->m_request_properties.m_map_properties["Content-Type"] = content_type_text;
    }

    void HTTPManager::createRegisterRequest(HTTPRequest::ptr request, body_type &body) {
        request->m_msg_id = MSGIDUtil::GenerateMSGID();
        std::string body_str = "server_ip:" + body["server_ip"] + g_CRLF
                               + "server_port:" + body["server_port"] + g_CRLF
                               + "all_services_names:" + body["all_services_names"] + g_CRLF
                               + "msg_id:" + request->m_msg_id;
        request->m_request_body = body_str;
        request->m_request_method = HTTPMethod::POST;
        request->m_request_version = "HTTP/1.1";
        request->m_request_path = RPC_SERVER_REGISTER_PATH;
        request->m_request_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        request->m_request_properties.m_map_properties["Content-Type"] = content_type_text;
    }

    void HTTPManager::createDiscoveryRequest(HTTPRequest::ptr request, body_type &body) {
        request->m_msg_id = MSGIDUtil::GenerateMSGID();
        std::string body_str = "msg_id:" + request->m_msg_id + g_CRLF
                               + "service_name:" + body["service_name"];
        request->m_request_body = body_str;
        request->m_request_method = HTTPMethod::POST;
        request->m_request_version = "HTTP/1.1";
        request->m_request_path = RPC_CLIENT_REGISTER_DISCOVERY_PATH;
        request->m_request_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        request->m_request_properties.m_map_properties["Content-Type"] = content_type_text;
    }

    void HTTPManager::createSubscribeRequest(HTTPRequest::ptr request, body_type &body) {
        request->m_msg_id = MSGIDUtil::GenerateMSGID();
        std::string body_str = "msg_id:" + request->m_msg_id + g_CRLF
                               + "service_name:" + body["service_name"];
        request->m_request_body = body_str;
        request->m_request_method = HTTPMethod::POST;
        request->m_request_version = "HTTP/1.1";
        request->m_request_path = RPC_REGISTER_SUBSCRIBE_PATH;
        request->m_request_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        request->m_request_properties.m_map_properties["Content-Type"] = content_type_text;
    }

    void HTTPManager::createPublishRequest(HTTPRequest::ptr request, body_type &body) {
        // 注册中心告诉客户端哪个service name有变化，证明已经有服务器掉线，然后客户端去重新拉，推拉结合
        // 后期可以订阅其他东西，实现自定义推送内容
        request->m_msg_id = MSGIDUtil::GenerateMSGID();
        std::string body_str = "msg_id:" + request->m_msg_id + g_CRLF
                               + "service_name:" + body["service_name"];
        request->m_request_body = body_str;
        request->m_request_method = HTTPMethod::POST;
        request->m_request_version = "HTTP/1.1";
        request->m_request_path = RPC_REGISTER_PUBLISH_PATH;
        request->m_request_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        request->m_request_properties.m_map_properties["Content-Type"] = content_type_text;
    }

    void HTTPManager::createMethodResponse(HTTPResponse::ptr response, body_type &body) {
        std::string body_str = "method_full_name:" + body["method_full_name"] + g_CRLF
                               + "pb_data:" + body["pb_data"] + g_CRLF
                               + "msg_id:" + body["msg_id"];
        response->m_response_body = body_str;
        response->m_response_version = "HTTP/1.1";
        response->m_response_code = HTTPCode::HTTP_OK;
        response->m_response_info = HTTPCodeToString(HTTPCode::HTTP_OK);
        response->m_response_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        response->m_response_properties.m_map_properties["Content-Type"] = content_type_text;
    }

    void HTTPManager::createHeartResponse(HTTPResponse::ptr response, body_type &body) {
        std::string body_str = "msg_id:" + body["msg_id"];
        response->m_response_body = body_str;
        response->m_response_version = "HTTP/1.1";
        response->m_response_code = HTTPCode::HTTP_OK;
        response->m_response_info = HTTPCodeToString(HTTPCode::HTTP_OK);
        response->m_response_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        response->m_response_properties.m_map_properties["Content-Type"] = content_type_text;
    }

    void HTTPManager::createRegisterResponse(HTTPResponse::ptr response, body_type &body) {
        std::string body_str = "add_service_count:" + body["add_service_count"] + g_CRLF
                               + "msg_id:" + body["msg_id"];
        response->m_response_body = body_str;
        response->m_response_version = "HTTP/1.1";
        response->m_response_code = HTTPCode::HTTP_OK;
        response->m_response_info = HTTPCodeToString(HTTPCode::HTTP_OK);
        response->m_response_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        response->m_response_properties.m_map_properties["Content-Type"] = content_type_text;
    }

    void HTTPManager::createDiscoveryResponse(HTTPResponse::ptr response, body_type &body) {
        std::string body_str = "server_list:" + body["server_list"] + g_CRLF
                               + "msg_id:" + body["msg_id"];
        response->m_response_body = body_str;
        response->m_response_version = "HTTP/1.1";
        response->m_response_code = HTTPCode::HTTP_OK;
        response->m_response_info = HTTPCodeToString(HTTPCode::HTTP_OK);
        response->m_response_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        response->m_response_properties.m_map_properties["Content-Type"] = content_type_text;
    }

    void HTTPManager::createSubscribeResponse(HTTPResponse::ptr response, body_type &body) {
        std::string body_str = "msg_id:" + body["msg_id"] + g_CRLF
                               + "service_name:" + body["service_name"] + g_CRLF
                               + "subscribe_success:" + body["subscribe_success"];
        response->m_response_body = body_str;
        response->m_response_version = "HTTP/1.1";
        response->m_response_code = HTTPCode::HTTP_OK;
        response->m_response_info = HTTPCodeToString(HTTPCode::HTTP_OK);
        response->m_response_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        response->m_response_properties.m_map_properties["Content-Type"] = content_type_text;
    }

    void HTTPManager::createPublishResponse(HTTPResponse::ptr response, body_type &body) {
        // 客户端告诉注册中心已经收到
        std::string body_str = "msg_id:" + body["msg_id"];
        response->m_response_body = body_str;
        response->m_response_version = "HTTP/1.1";
        response->m_response_code = HTTPCode::HTTP_OK;
        response->m_response_info = HTTPCodeToString(HTTPCode::HTTP_OK);
        response->m_response_properties.m_map_properties["Content-Length"] = std::to_string(body_str.length());
        response->m_response_properties.m_map_properties["Content-Type"] = content_type_text;
    }


}



