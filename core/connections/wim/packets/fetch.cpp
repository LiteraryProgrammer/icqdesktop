#include "stdafx.h"
#include "fetch.h"

#include <sstream>

#include "../../../http_request.h"
#include "../events/fetch_event.h"

#include "../events/fetch_event_buddy_list.h"
#include "../events/fetch_event_presence.h"
#include "../events/fetch_event_dlg_state.h"
#include "../events/fetch_event_hidden_chat.h"
#include "../events/fetch_event_diff.h"
#include "../events/fetch_event_my_info.h"
#include "../events/fetch_event_user_added_to_buddy_list.h"
#include "../events/fetch_event_typing.h"
#include "../events/fetch_event_permit.h"
#include "../events/fetch_event_imstate.h"
#include "../events/fetch_event_notification.h"
#include "../events/fetch_event_appsdata.h"
#include "../events/fetch_event_mention_me.h"
#include "../events/fetch_event_chat_heads.h"
#include "../events/fetch_event_gallery_notify.h"
#include "../events/fetch_event_mchat.h"

#include "../events/webrtc.h"

#include "../../../log/log.h"

#include "../../../tools/json_helper.h"

#include <time.h>


using namespace core;
using namespace wim;

static constexpr auto default_fetch_timeout = std::chrono::milliseconds(500);
static constexpr auto default_next_fetch_timeout = std::chrono::seconds(60);

fetch::fetch(
    wim_packet_params _params,
    const std::string& _fetch_url,
    std::chrono::milliseconds _timeout,
    const timepoint _fetch_time,
    const bool _hidden,
    std::function<bool(std::chrono::milliseconds)> _wait_function)
    :
    wim_packet(std::move(_params)),
    fetch_url_(_fetch_url),
    timeout_(_timeout),
    wait_function_(std::move(_wait_function)),
    fetch_time_(_fetch_time),
    relogin_(relogin::none),
    next_fetch_time_(std::chrono::system_clock::now()),
    ts_(0),
    time_offset_(0),
    execute_time_(0),
    now_(0),
    request_time_(0),
    timezone_offset_(0),
    hidden_(_hidden),
    events_count_(0),
    my_aimid_(_params.aimid_),
    next_fetch_timeout_(default_next_fetch_timeout)
{
}


fetch::~fetch()
{
}

int32_t fetch::init_request(std::shared_ptr<core::http_request_simple> _request)
{
    std::stringstream ss_url;

    if (auto trimmed = core::tools::trim_right(std::string_view(fetch_url_), '/'); trimmed.rfind('?') == std::string_view::npos)
        ss_url << trimmed << '?';
    else
        ss_url << trimmed << '&';

    static int32_t request_id = 0;
    ss_url << "f=json" << "&r=" << ++request_id << "&timeout=" << timeout_.count() << "&peek=" << '0';
    if (hidden_)
        ss_url << "&hidden=1";

    _request->set_url(ss_url.str());
    _request->set_normalized_url("fetchEvents");
    _request->set_timeout(timeout_ + std::chrono::seconds(5));
    _request->set_keep_alive();
    _request->set_priority(priority_fetch());

    if (!params_.full_log_)
    {
        log_replace_functor f;
        f.add_marker("aimsid", aimsid_range_evaluator());
        f.add_json_marker("text");
        f.add_json_marker("message");
        f.add_json_marker("url");
        f.add_json_marker("caption");
        _request->set_replace_log_function(f);
    }

    return 0;
}

int32_t fetch::execute_request(std::shared_ptr<core::http_request_simple> _request)
{
    auto current_time = std::chrono::system_clock::now();

    if (fetch_time_ > current_time)
    {
        const auto time_out = std::chrono::duration_cast<std::chrono::milliseconds>(fetch_time_ - current_time);
        if (wait_function_(time_out))
            return wpie_error_request_canceled;
    }

    next_fetch_time_ = std::chrono::system_clock::now() + default_fetch_timeout;

    execute_time_ = time(nullptr);

    if (!_request->get(request_time_))
        return wpie_network_error;

    http_code_ = (uint32_t)_request->get_response_code();

    if (http_code_ != 200)
    {
        if (http_code_ >= 500 && http_code_ < 600)
            return wpie_error_resend;

        return wpie_http_error;
    }

    return 0;
}

void fetch::on_session_ended(const rapidjson::Value &_data)
{
    relogin_ = relogin::relogin_with_error;

    int end_code = 0;
    tools::unserialize_value(_data, "endCode", end_code);
    switch (end_code)
    {
        case 26:            // "endCode":26, "offReason":"User Initiated Bump"
        case 142:           // "endCode":142, "offReason":"Killed Sessions"
            relogin_ = relogin::relogin_without_error;
            break;
    }
}

relogin fetch::need_relogin() const
{
    return relogin_;
}

int32_t fetch::get_events_count() const
{
    return events_count_;
}

std::shared_ptr<core::wim::fetch_event> fetch::push_event(std::shared_ptr<core::wim::fetch_event> _event)
{
    events_.push_back(_event);

    return _event;
}

std::shared_ptr<core::wim::fetch_event> fetch::pop_event()
{
    if (events_.empty())
    {
        return nullptr;
    }


    auto evt = events_.front();

    events_.pop_front();

    return evt;
}


int32_t fetch::parse_response_data(const rapidjson::Value& _data)
{
    try
    {
        bool have_webrtc_event = false;

        events_count_ = 0;

        if (const auto iter_events = _data.FindMember("events"); iter_events != _data.MemberEnd() && iter_events->value.IsArray())
        {
            for (const auto& event : iter_events->value.GetArray())
            {
                ++events_count_;
                const auto iter_event_data = event.FindMember("eventData");
                if (std::string_view event_type; tools::unserialize_value(event, "type", event_type) && iter_event_data != event.MemberEnd())
                {
                    if (event_type == "buddylist")
                        push_event(std::make_shared<fetch_event_buddy_list>())->parse(iter_event_data->value);
                    else if (event_type == "presence")
                        push_event(std::make_shared<fetch_event_presence>())->parse(iter_event_data->value);
                    else if (event_type == "histDlgState")
                        push_event(std::make_shared<fetch_event_dlg_state>())->parse(iter_event_data->value);
                    else if (event_type == "webrtcMsg")
                        have_webrtc_event = true;
                    else if (event_type == "hiddenChat")
                        push_event(std::make_shared<fetch_event_hidden_chat>())->parse(iter_event_data->value);
                    else if (event_type == "diff")
                        push_event(std::make_shared<fetch_event_diff>())->parse(iter_event_data->value);
                    else if (event_type == "myInfo")
                        push_event(std::make_shared<fetch_event_my_info>())->parse(iter_event_data->value);
                    else if (event_type == "userAddedToBuddyList")
                        push_event(std::make_shared<fetch_event_user_added_to_buddy_list>())->parse(iter_event_data->value);
                    else if (event_type == "typing")
                        push_event(std::make_shared<fetch_event_typing>())->parse(iter_event_data->value);
                    else if (event_type == "sessionEnded")
                        on_session_ended(iter_event_data->value);
                    else if (event_type == "permitDeny")
                        push_event(std::make_shared<fetch_event_permit>())->parse(iter_event_data->value);
                    else if (event_type == "imState")
                        push_event(std::make_shared<fetch_event_imstate>())->parse(iter_event_data->value);
                    else if (event_type == "notification")
                        push_event(std::make_shared<fetch_event_notification>())->parse(iter_event_data->value);
                    else if (event_type == "apps")
                        push_event(std::make_shared<fetch_event_appsdata>())->parse(iter_event_data->value);
                    else if (event_type == "mentionMeMessage")
                        push_event(std::make_shared<fetch_event_mention_me>())->parse(iter_event_data->value);
                    else if (event_type == "chatHeadsUpdate")
                        push_event(std::make_shared<fetch_event_chat_heads>())->parse(iter_event_data->value);
                    else if (event_type == "galleryNotify")
                        push_event(std::make_shared<fetch_event_gallery_notify>(my_aimid_))->parse(iter_event_data->value);
                    else if (event_type == "mchat")
                        push_event(std::make_shared<fetch_event_mchat>())->parse(iter_event_data->value);
                }
            }
        }

        if (relogin_ == relogin::none)
        {
            if (!tools::unserialize_value(_data, "fetchBaseURL", next_fetch_url_))
                return wpie_http_parse_response;

            next_fetch_time_ = std::chrono::system_clock::now();

            if (int64_t fetch_timeout = 0; tools::unserialize_value(_data, "timeToNextFetch", fetch_timeout))
                next_fetch_time_ += std::chrono::milliseconds(fetch_timeout);

            if (int64_t next_fetch_timeout = 0; tools::unserialize_value(_data, "fetchTimeout", next_fetch_timeout))
                next_fetch_timeout_ = std::chrono::seconds(next_fetch_timeout);

            if (int64_t ts = 0; tools::unserialize_value(_data, "ts", ts))
                ts_ = ts;
            else
                return wpie_http_parse_response;

            now_ = time(nullptr);

            timezone_offset_ = mktime(localtime(&now_)) - mktime(gmtime(&now_));

            const auto diff = now_ - execute_time_ - std::round(request_time_);

            const auto now_local = now_ + timezone_offset_;

            time_offset_ = now_ - ts_ - diff;
            time_offset_local_ = now_local - ts_ - diff;
        }

        if (have_webrtc_event) {
            auto we = std::make_shared<webrtc_event>();
            if (!!we) {
                // sorry... the simplest way
                we->parse(response_str());
                push_event(we);
            } else {
                assert(false);
            }
        }
    }
    catch (const std::exception&)
    {
        return wpie_http_parse_response;
    }

    return 0;
}

int32_t fetch::on_response_error_code()
{
    auto code = get_status_code();

    if (code >= 500 && code < 600)
        return wpie_error_resend;

    switch (code)
    {
    case 401:
    case 460:
    default:
        return wpie_error_need_relogin;
    }
}
