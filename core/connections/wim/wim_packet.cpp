#include "stdafx.h"

#include "wim_packet.h"

#include "../../http_request.h"
#include "../../tools/hmac_sha_base64.h"
#include "../../log/log.h"
#include "../../utils.h"

#include "../../../libomicron/include/omicron/omicron.h"

using namespace core;
using namespace wim;

wim_packet::wim_packet(wim_packet_params params)
    :
    hosts_scheme_changed_(false),
    status_code_(std::numeric_limits<uint32_t>::max()),
    status_detail_code_(std::numeric_limits<uint32_t>::max()),
    http_code_(std::numeric_limits<uint32_t>::max()),
    repeat_count_(0),
    can_change_hosts_scheme_(false),
    params_(std::move(params))
{

}

wim_packet::~wim_packet()
{

}

bool wim_packet::support_async_execution() const
{
    return false;
}

int32_t wim_packet::execute()
{
    auto request = std::make_shared<core::http_request_simple>(params_.proxy_, utils::get_user_agent(params_.aimid_), params_.stop_handler_);

    ++repeat_count_;

    int32_t err = init_request(request);
    if (err != 0)
        return err;

    err = execute_request(request);
    if (err != 0)
        return err;

    auto response = std::static_pointer_cast<tools::binary_stream>(request->get_response());
    assert(response);
    err = parse_response(response);
    if (err != 0 && g_core->is_im_stats_enabled())
    {
        const auto is_one_domain = omicronlib::_o("one_domain_feature", feature::default_one_domain_feature());

        core::stats::event_props_type props;
        props.emplace_back("endpoint", request->get_normalized_url());
        props.emplace_back("error", std::to_string(err));
        props.emplace_back("status", std::to_string(status_code_));

        if (err == wpie_http_parse_response)
        {
            auto event_name = is_one_domain ? stats::im_stat_event_names::u_network_parsing_error_event : stats::im_stat_event_names::network_parsing_error_event;
            g_core->insert_im_stats_event(event_name, std::move(props));
        }
        else if (status_code_ != 200 && status_code_ != 20000)
        {
            auto event_name = is_one_domain ? stats::im_stat_event_names::u_network_api_error_event : stats::im_stat_event_names::network_api_error_event;
            g_core->insert_im_stats_event(event_name, std::move(props));
        }
    }

    return err;
}

void wim_packet::execute_async(handler_t _handler)
{
    assert(support_async_execution());

    auto request = std::make_shared<core::http_request_simple>(params_.proxy_, utils::get_user_agent(params_.aimid_), params_.stop_handler_);

    int32_t err = init_request(request);
    if (err != 0)
    {
        _handler(err);
        return;
    }

    auto wr_this = weak_from_this();

    execute_request_async(request, [wr_this, request, _handler](int32_t _err)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_err != 0)
        {
            _handler(_err);
            return;
        }

        auto response = std::static_pointer_cast<tools::binary_stream>(request->get_response());
        assert(response);
        const auto err = ptr_this->parse_response(response);
        if (err != 0 && g_core->is_im_stats_enabled())
        {
            const auto is_one_domain = omicronlib::_o("one_domain_feature", feature::default_one_domain_feature());

            core::stats::event_props_type props;
            props.emplace_back("endpoint", request->get_normalized_url());
            props.emplace_back("error", std::to_string(err));
            props.emplace_back("status", std::to_string(ptr_this->get_status_code()));

            if (err == wpie_http_parse_response || err == wpie_http_empty_response)
            {
                auto event_name = is_one_domain ? stats::im_stat_event_names::u_network_parsing_error_event : stats::im_stat_event_names::network_parsing_error_event;
                g_core->insert_im_stats_event(event_name, std::move(props));
            }
            else if (ptr_this->get_status_code() != 200 && ptr_this->get_status_code() != 20000)
            {
                auto event_name = is_one_domain ? stats::im_stat_event_names::u_network_api_error_event : stats::im_stat_event_names::network_api_error_event;
                g_core->insert_im_stats_event(event_name, std::move(props));
            }
        }

        _handler(err);
    });
}

bool wim_packet::is_stopped() const
{
    return params_.stop_handler_();
}

bool wim_packet::needs_to_repeat_failed(const int32_t _error) noexcept
{
    return is_network_error_or_canceled(_error) || is_timeout_error(_error);
}

int32_t wim_packet::init_request(std::shared_ptr<core::http_request_simple> request)
{
    return 0;
}

int32_t wim_packet::execute_request(std::shared_ptr<core::http_request_simple> request)
{
    if (!request->get())
        return wpie_network_error;

    http_code_ = (uint32_t)request->get_response_code();

    if (request->get_header()->available())
    {
        auto header = request->get_header();
        uint32_t size = header->available();
        auto buf = (const char *)header->read(size);

        if (buf && size)
        {
            header_str_.assign(buf, size);
        }

        header->reset_out();
    }

    if (http_code_ != 200)
    {
        if (http_code_ > 400 && http_code_ < 500)
            return on_http_client_error();
        else if (http_code_ >= 500 && http_code_ < 600)
            return wpie_error_resend;

        return wpie_http_error;
    }

    return 0;
}

void wim_packet::execute_request_async(std::shared_ptr<core::http_request_simple> _request, handler_t _handler)
{
    auto wr_this = weak_from_this();

    _request->get_async([_request, _handler, wr_this](curl_easy::completion_code _completion_code)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_completion_code != curl_easy::completion_code::success)
        {
            if (_handler)
                _handler(wpie_network_error);
            return;
        }

        ptr_this->http_code_ = (uint32_t) _request->get_response_code();

        if (auto header = _request->get_header(); header->available())
        {
            const uint32_t size = header->available();
            auto buf = (const char *)header->read(size);

            if (buf && size)
            {
                ptr_this->header_str_.assign(buf, size);
            }

            header->reset_out();
        }

        if (!_handler)
            return;

        if (ptr_this->http_code_ == 200)
        {
            _handler(0);
        }
        else if (ptr_this->http_code_ > 400 && ptr_this->http_code_ < 500)
        {
            _handler(ptr_this->on_http_client_error());
        }
        else if (ptr_this->http_code_ >= 500 && ptr_this->http_code_ < 600)
        {
            _handler(wpie_error_resend);
        }
        else
        {
            _handler(wpie_http_error);
        }
    });
}

void wim_packet::load_response_str(const char* buf, unsigned size)
{
    assert(buf && size);
    if (buf && size)
    {
        response_str_.assign(buf, size);
    }
    else
    {
        response_str_.clear();
    }
}

const std::string& wim_packet::response_str() const
{
    return response_str_;
}

const std::string& wim_packet::header_str() const
{
    return header_str_;
}

int32_t wim_packet::parse_response(std::shared_ptr<core::tools::binary_stream> response)
{
    if (!response->available())
        return wpie_http_empty_response;

    int32_t err = 0;

    response->write((char) 0);

    uint32_t size = response->available();

    load_response_str((const char*) response->read(size), size);

    response->reset_out();

    try
    {
        const auto json_str = response->read(response->available());

#ifdef DEBUG__OUTPUT_NET_PACKETS
        puts(json_str);
#endif // DEBUG__OUTPUT_NET_PACKETS

#ifdef DEBUG
        const std::string json_str_dbg(json_str);
#endif

        rapidjson::Document doc;
        if (doc.ParseInsitu(json_str).HasParseError())
            return wpie_error_parse_response;

        auto iter_response = doc.FindMember("response");
        if (iter_response == doc.MemberEnd())
            return wpie_http_parse_response;

        auto iter_status = iter_response->value.FindMember("statusCode");
        if (iter_status == iter_response->value.MemberEnd() || !iter_status->value.IsUint())
            return wpie_http_parse_response;

        status_code_ = iter_status->value.GetUint();

        auto iter_status_text = iter_response->value.FindMember("statusText");
        if (iter_status_text != iter_response->value.MemberEnd() && iter_status_text->value.IsString())
            status_text_ = rapidjson_get_string(iter_status_text->value);

        auto iter_status_detail = iter_response->value.FindMember("statusDetailCode");
        if (iter_status_detail != iter_response->value.MemberEnd() && iter_status_detail->value.IsUint())
            status_detail_code_ = (uint32_t) iter_status_detail->value.GetUint();

        if (status_code_ == 200)
        {
            auto iter_data = iter_response->value.FindMember("data");
            if (iter_data == iter_response->value.MemberEnd())
            {
                err = on_empty_data();
                if (err != 0)
                    return err;
            }
            else
            {
                return parse_response_data(iter_data->value);
            }
        }
        else
        {
            auto iter_data = iter_response->value.FindMember("data");
            if (iter_data != iter_response->value.MemberEnd())
            {
                parse_response_data_on_error(iter_data->value);
            }

            return on_response_error_code();
        }
    }
    catch (...)
    {
        return 0;
    }

    return 0;
}

int32_t wim_packet::on_response_error_code()
{
    switch (status_code_)
    {
        case INVALID_REQUEST:
            return wpie_error_invalid_request;

        case AUTHN_REQUIRED:
        case MISSING_REQUIRED_PARAMETER:
            return wpie_error_need_relogin;

        case SEND_IM_RATE_LIMIT:
        case TARGET_RATE_LIMIT_REACHED:
            return wpie_error_too_fast_sending;

        default:
            return wpie_error_message_unknown;
    }
}

int32_t wim_packet::on_http_client_error()
{
    if (http_code_ >= 500 && http_code_ < 600)
        return wpie_error_resend;
    return wpie_client_http_error;
}


int32_t wim_packet::on_empty_data()
{
    return wpie_http_parse_response;
}

std::string wim_packet::escape_symbols(std::string_view _data)
{
    std::stringstream ss_out;

    std::array<char, 100> buffer;

    for (auto sym : _data)
    {
        if (core::tools::is_latin(sym) || core::tools::is_digit(sym) || strchr("-._~", sym))
        {
            ss_out << sym;
        }
        else
        {
#ifdef _WIN32
            sprintf_s(buffer.data(), buffer.size(), "%%%.2X", (unsigned char) sym);
#else
            sprintf(buffer.data(), "%%%.2X", (unsigned char) sym);
#endif
            ss_out << buffer.data();
        }
    }

    return ss_out.str();
}

std::string wim_packet::escape_symbols_data(std::string_view _data)
{
    std::stringstream ss_out;

    std::array<char, 100> buffer;

    for (auto sym : _data)
    {
        if (core::tools::is_latin(sym) || core::tools::is_digit(sym))
        {
            ss_out << sym;
        }
        else
        {
#ifdef _WIN32
            sprintf_s(buffer.data(), buffer.size(), "%%%.2X", (unsigned char)sym);
#else
            sprintf(buffer.data(), "%%%.2X", (unsigned char)sym);
#endif
            ss_out << buffer.data();
        }
    }

    return ss_out.str();
}

std::string wim_packet::create_query_from_map(const str_2_str_map& _params)
{
    std::string query;

    for (const auto& iter : _params)
    {
        query += iter.first;
        query += '=';
        query += iter.second;
        query += '&';
    }

    if (!query.empty())
        query.pop_back();

    return query;
}

std::string wim_packet::detect_digest(const std::string& hashed_data, const std::string& session_key)
{
    if (hashed_data.empty() || session_key.empty())
    {
        assert(false);
        return std::string();
    }

    std::vector<uint8_t> hash_data_vector(hashed_data.size());
    memcpy(&hash_data_vector[0], hashed_data.c_str(), hashed_data.size());

    std::vector<uint8_t> session_key_vector(session_key.size());
    memcpy(&session_key_vector[0], session_key.c_str(), session_key.size());

    return core::tools::base64::hmac_base64(hash_data_vector, session_key_vector);
}


std::string wim_packet::get_url_sign(std::string_view _host, const str_2_str_map& _params, const wim_packet_params& _wim_params,  bool _post_method, bool make_escape_symbols/* = true*/)
{
    const std::string_view http_method = _post_method ? "POST" : "GET";
    std::string query_string = create_query_from_map(_params);

    std::string hash_data;
    hash_data += http_method;
    hash_data += '&';
    if (make_escape_symbols)
    {
        hash_data += escape_symbols(_host);
        hash_data += '&';
        hash_data += escape_symbols(query_string);
    }
    else
    {
        hash_data += _host;
        hash_data += '&';
        hash_data += query_string;
    }

    return detect_digest(hash_data, _wim_params.session_key_);
}

std::string wim_packet::format_get_params(const std::map<std::string, std::string>& _params)
{
    std::string result;

    for (const auto& [key, value] : _params)
    {
        result += key;
        result += '=';
        result += value;
        result += '&';
    }
    if (!result.empty())
        result.pop_back();

    return result;
}

int32_t wim_packet::parse_response_data(const rapidjson::Value& _data)
{
    return 0;
}

void wim_packet::parse_response_data_on_error(const rapidjson::Value& _data)
{
}

const wim_packet_params& core::wim::wim_packet::get_params() const
{
    return params_;
}

uint32_t core::wim::wim_packet::get_repeat_count() const
{
    return repeat_count_;
}

void core::wim::wim_packet::set_repeat_count(const uint32_t _count)
{
    repeat_count_ = _count;
}

void core::wim::wim_packet::replace_log_messages(tools::binary_stream& _bs)
{
    uint32_t sz = _bs.available();
    char* logdata = _bs.get_data();
    char* end = logdata + sz;

    if (!logdata || !sz)
        return;

    auto replace_marker = [logdata, end](const std::string_view _marker)
    {
        char* cursor = logdata;

        while (cursor < end)
        {
            cursor = std::search(cursor, end, _marker.data(), _marker.data() + _marker.length());
            cursor += _marker.length();
            if (cursor >= end)
                return;

            while (cursor < end && *cursor++ != '\"') {}


            while (cursor < end)
            {
                if (*cursor == '\"')
                    break;

                if (*cursor == '\\')
                {
                    *cursor = '*';

                    ++cursor;
                    if (cursor >= end)
                        return;
                }

                *cursor = '*';

                ++cursor;
            }
        }
    };

    for (const std::string_view _marker : { "\"text\":", "\"message\":" })
        replace_marker(_marker);
}

std::string wim_packet::extract_etag() const
{
    auto header = header_str();
    if (!header.empty())
    {
        std::transform(header.begin(), header.end(), header.begin(), ::tolower);

        for (const std::string_view etag_prefix : { "etag: \"", "etag: w/\"" })
        {
            if (const auto etag_pos_begin = header.find(etag_prefix); etag_pos_begin != std::string::npos)
            {
                constexpr auto etag_postfix = std::string_view("\"");

                const auto etag_pos_end = header.find(etag_postfix, etag_pos_begin + etag_prefix.length());

                if (etag_pos_end != std::string::npos)
                    return std::string(header.begin() + etag_pos_begin + etag_prefix.length(), header.begin() + etag_pos_end);
            }
        }
    }

    return std::string();
}

bool wim_packet::is_network_error_or_canceled(const int32_t _error) noexcept
{
    return wim_protocol_internal_error::wpie_network_error == _error
        || wim_protocol_internal_error::wpie_error_resend == _error
        || wim_protocol_internal_error::wpie_error_task_canceled == _error;
}

bool wim_packet::is_timeout_error(const int32_t _error) noexcept
{
    return _error == wim_protocol_internal_error::wpie_robusto_timeout;
}

void log_replace_functor::add_marker(std::string_view _marker, range_evaluator _re)
{
    markers_.push_back(std::make_pair(std::string(_marker) + '=', std::move(_re)));
}

void log_replace_functor::add_url_marker(const char* _marker, range_evaluator _re)
{
    markers_.push_back(std::make_pair(_marker, std::move(_re)));
}

void log_replace_functor::add_json_marker(const char* _marker, range_evaluator _re)
{
    const static std::string json_delimeter = "\":\"";
    const static std::string json_delimeter_with_space = "\": \"";
    const static std::string json_delimeter_encoded = wim::wim_packet::escape_symbols("\":\"");

    markers_json_.push_back(std::make_pair('"' + std::string(_marker) + json_delimeter, _re));
    markers_json_.push_back(std::make_pair('"' + std::string(_marker) + json_delimeter_with_space, _re));
    markers_json_encoded_.push_back(std::make_pair(wim::wim_packet::escape_symbols("\"") + _marker + json_delimeter_encoded, std::move(_re)));
}

void log_replace_functor::add_json_array_marker(const char* _marker, range_evaluator _re)
{
    const static std::string json_delimeter = "\":[";
    const static std::string json_delimeter_with_space = "\": [";
    markers_json_.push_back(std::make_pair(_marker + json_delimeter, _re));
    markers_json_.push_back(std::make_pair(_marker + json_delimeter_with_space, _re));
}

void log_replace_functor::operator()(tools::binary_stream& _bs)
{
    auto sz = _bs.available();
    auto data = _bs.get_data();

    std::string json_value_end;
    std::string json_value_end_excluded;

    decltype(sz) i = 0;
    while (i < sz - 1) // -1 because there is no need to check last character
    {
        bool found = false;
        bool found_json = false;

        range_evaluator range_eval = nullptr;

        for (const auto& [m, re] : markers_)
        {
            if (sz - i > m.size() && strncmp(data + i, m.c_str(), m.size()) == 0)
            {
                i += m.size();
                found = true;
                range_eval = re;
                break;
            }
        }
        if (!found)
        {
            for (const auto& [m, re] : markers_json_) // we have to check both escaped and not escaped json markers, because everything is possible
            {
                if (sz - i > m.size() && strncmp(data + i, m.c_str(), m.size()) == 0)
                {
                    i += m.size();
                    found_json = true;
                    range_eval = re;
                    if (m.back() == '[') // found array marker "...: ["
                    {
                        static const auto json_array_end_not_encoded = std::string("]");
                        json_value_end = json_array_end_not_encoded;
                    }
                    else
                    {
                        static const auto json_value_end_not_encoded = std::string("\"");
                        static const auto json_value_end_not_encoded_excluded = std::string("\\\"");
                        json_value_end = json_value_end_not_encoded;
                        json_value_end_excluded = json_value_end_not_encoded_excluded;
                    }
                    break;
                }
            }
            if (!found_json)
            {
                for (const auto& [m, re] : markers_json_encoded_)
                {
                    if (sz - i > m.size() && strncmp(data + i, m.c_str(), m.size()) == 0)
                    {
                        i += m.size();
                        found_json = true;
                        range_eval = re;
                        static const auto json_value_end_encoded = wim::wim_packet::escape_symbols("\"");
                        static const auto json_value_end_encoded_excluded = wim::wim_packet::escape_symbols("\\\"");
                        json_value_end = json_value_end_encoded;
                        json_value_end_excluded = json_value_end_encoded_excluded;
                        break;
                    }
                }
            }
        }
        if (found || found_json)
        {
            if (range_eval)
            {
                auto [start, end] = range_eval(std::string_view(data + i, sz - i));

                end += i;
                i += start;

                while (i < sz && i <= end)
                {
                    replace(data + i);
                    ++i;
                }
            }
            else if (!found_json)
            {
                while (i < sz && *(data + i) != '&')
                {
                    replace(data + i);
                    ++i;
                }
            }
            else
            {
                bool skip_value_end = false;
                int count_to_skip = 0;

                while (i <= (sz - json_value_end.size()) && !(!skip_value_end && strncmp(data + i, json_value_end.c_str(), json_value_end.size()) == 0))
                {
                    // check if there is excluded value end, for example we need to skip \" characters and don't treat " character as value end
                    if (count_to_skip > 0)
                    {
                        skip_value_end = --count_to_skip > 0;
                    }
                    else if (i <= (sz - json_value_end_excluded.size()) && strncmp(data + i, json_value_end_excluded.c_str(), json_value_end_excluded.size()) == 0)
                    {
                        count_to_skip = json_value_end_excluded.size() - 1;
                        skip_value_end = true;
                    }

                    replace(data + i);
                    ++i;
                }
            }
        }
        else
        {
            ++i;
        }
    }
}

void log_replace_functor::replace(char *c)
{
    if (*c != ' ') // do not replace space character
        *c = '*';
}

std::pair<size_t, size_t> aimsid_range_evaluator::operator()(std::string_view s) const
{
    // 025.0643767406.2836549238:123456 -> 025.0643767406.**********:123456

    auto point_count = 0;
    auto start = 0u;
    auto end = 0u;

    for (auto i = 0u; i < s.size(); i++)
    {
        if (s[i] == '.')
        {
            point_count++;
            if (point_count == 2)
                start = i + 1;
        }
        else if (point_count == 2 && (s[i] == ':' || s[i] == '%'))
        {
            end = i - 1;
            break;
        }
    }

    return std::make_pair(start, end);
}

tail_from_last_range_evaluator::tail_from_last_range_evaluator(const char _chr)
    : chr_(_chr)
{
}

std::pair<size_t, size_t> tail_from_last_range_evaluator::operator()(std::string_view _str) const
{
    auto start = 0u;
    auto end = _str.size() - 1;

    for (auto i = 0u; i < _str.size(); i++)
    {
        if (_str[i] == chr_)
        {
            start = i + 1;
        }
        else if (std::isspace(static_cast<int>(_str[i])) || _str[i] == '"')
        {
            end = i - 1;
            break;
        }
    }

    return std::make_pair(start, end);
}

bool core::wim::is_new_avatar_rapi()
{
    return build::is_dit() || omicronlib::_o("new_avatar_rapi", feature::default_new_avatar_rapi());
}
