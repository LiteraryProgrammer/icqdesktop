#include "stdafx.h"

#include "request_avatar.h"

#include "../../../http_request.h"
#include "../../../tools/system.h"
#include "../../urls_cache.h"

using namespace core;
using namespace wim;

request_avatar::request_avatar(wim_packet_params params, const std::string& _contact, const std::string& _avatar_type, time_t _write_time)
    : wim_packet(std::move(params))
    , avatar_type_(_avatar_type)
    , contact_(_contact)
    , write_time_(_write_time)
{
}


request_avatar::~request_avatar()
{
}

int32_t request_avatar::init_request(std::shared_ptr<core::http_request_simple> _request)
{
    std::stringstream ss_url;

    if (is_new_avatar_rapi())
    {
        ss_url << urls::get_url(urls::url_type::rapi_host) << "avatar/get?"
            << "aimsid=" << escape_symbols(get_params().aimsid_)
            << "&targetSn=" << escape_symbols(contact_)
            << "&size=" << avatar_type_;

        _request->set_normalized_url("avatarGet");
    }
    else
    {
        ss_url << "https://api.icq.net/expressions/get?"
            << "t=" << escape_symbols(contact_)
            << "&f=native"
            << "&r=" << core::tools::system::generate_guid()
            << "&type=" << avatar_type_;

        _request->set_normalized_url("avatarExpressionsGet");
    }

    if (write_time_ != 0)
        _request->set_modified_time_condition(write_time_ - params_.time_offset_);

    _request->set_need_log(params_.full_log_);
    _request->set_need_log_original_url(false);
    _request->set_url(ss_url.str());
    _request->set_keep_alive();
    _request->set_priority(highest_priority() + increase_priority());

    if (!params_.full_log_)
    {
        log_replace_functor f;
        f.add_marker("aimsid", aimsid_range_evaluator());
        _request->set_replace_log_function(f);
    }

    return 0;
}

int32_t core::wim::request_avatar::execute_request(std::shared_ptr<core::http_request_simple> _request)
{
    // request for an empty contact in the old method returned code 404, but for now the new method returns 200 and the stub file (later replaced with code 400)
    // in any case, an empty contact request is an bad request, so immediately returns an error code without a request
    return contact_.empty() ? wpie_client_http_error : wim_packet::execute_request(_request);
}

int32_t core::wim::request_avatar::parse_response(std::shared_ptr<core::tools::binary_stream> _response)
{
    data_ = _response;

    return 0;
}

std::shared_ptr<core::tools::binary_stream> core::wim::request_avatar::get_data() const
{
    return data_;
}
