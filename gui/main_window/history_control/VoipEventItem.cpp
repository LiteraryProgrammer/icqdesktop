#include "stdafx.h"

#include "../../core_dispatcher.h"
#include "../../cache/avatars/AvatarStorage.h"
#include "../../utils/InterConnector.h"
#include "../../utils/PainterPath.h"
#include "../../utils/utils.h"

#include "../../controls/ContextMenu.h"
#include "../../styles/ThemeParameters.h"
#include "../../app_config.h"
#include "../../gui_settings.h"
#include "../../my_info.h"
#include "../../fonts.h"
#include "../mediatype.h"
#include "../friendly/FriendlyContainer.h"
#include "../contact_list/ContactListModel.h"
#include "../contact_list/RecentsModel.h"

#include "MessageStatusWidget.h"
#include "MessageStyle.h"
#include "VoipEventInfo.h"
#include "VoipEventItem.h"

namespace
{
    QSize getIconSize()
    {
        return Utils::scale_value(QSize(40, 40));
    }

    int32_t confMemberSize()
    {
        return Utils::scale_value(14);
    }

    int32_t confMemberOverlap()
    {
        return Utils::scale_value(2);
    }

    int32_t confMemberTopMargin()
    {
        return Utils::scale_value(20);
    }

    int32_t confMemberCutWidth()
    {
        return Utils::scale_value(1);
    }

    int32_t buttonHeight()
    {
        return Utils::scale_value(32);
    }

    int32_t buttonBottomMargin()
    {
        return Utils::scale_value(24);
    }

    QColor missedColor()
    {
        return Styling::getParameters().getColor(Styling::StyleVariable::SECONDARY_ATTENTION);
    }

    QPixmap getIcon(const bool _isVideoCall, const bool _isMissed, const bool _isSelected, const bool _isOutgoing)
    {
        const auto circle = [](const QColor& _color)
        {
            QPixmap pm(Utils::scale_bitmap(getIconSize()));
            pm.fill(Qt::transparent);
            Utils::check_pixel_ratio(pm);

            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(_color);
            p.drawEllipse(QRect(QPoint(), getIconSize()));
            return pm;
        };

        const auto makeIcon = [circle](const QString& _path, const QColor& _iconColor, const QColor& _bgColor)
        {
            const auto icon = Utils::renderSvg(_path, getIconSize() / 2, _iconColor);
            assert(!icon.isNull());

            auto bg = circle(_bgColor);
            QPainter p(&bg);
            const auto ratio = Utils::scale_bitmap_ratio();
            p.drawPixmap(icon.width() / ratio / 2, icon.height() / ratio / 2, icon);
            return bg;
        };

        const auto make = [makeIcon](const QString& _path) ->std::array<std::array<QPixmap, 3>, 2>
        {
            const auto whiteOut = Styling::getParameters().getColor(Styling::StyleVariable::CHAT_SECONDARY);
            const auto whiteIn = Styling::getParameters().getColor(Styling::StyleVariable::CHAT_PRIMARY);
            const auto green = Styling::getParameters().getColor(Styling::StyleVariable::PRIMARY_INVERSE);
            const auto red = missedColor();

            return
            {
                {
                makeIcon(_path, whiteIn, green),  // normal
                makeIcon(_path, whiteIn, red),    // miss
                makeIcon(_path, green, whiteIn),  // selected
                // Outgoing
                makeIcon(_path, whiteOut, green),  // normal
                makeIcon(_path, whiteOut, red),    // miss
                makeIcon(_path, green, whiteIn),  // selected
                }
            };
        };

        static const auto audio = make(qsl(":/voip_events/call"));
        static const auto video = make(qsl(":/voip_events/videocall"));

        const auto& set = _isVideoCall ? video : audio;
        const auto _idxOut = _isOutgoing ? 1 : 0;
        return _isSelected ? set[_idxOut][2] : (_isMissed ? set[_idxOut][1] : set[_idxOut][0]);
    }

    QPixmap getAvatar(const QString& _aimId, const QString& _friendly, const int _size)
    {
        auto def = false;
        return *Logic::GetAvatarStorage()->GetRounded(
            _aimId,
            _friendly,
            _size,
            QString(),
            def,
            false,
            false
        );
    }

    int32_t getTextBaselineY()
    {
        return Utils::scale_value(21);
    }

    int getTimeLeftSpacing()
    {
        return Utils::scale_value(8);
    }

    QColor callButtonDefTextColor()
    {
        return Styling::getParameters().getColor(Styling::StyleVariable::PRIMARY_INVERSE);
    }

    QColor blendColors(const QColor& _color1, const QColor& _color2, qreal _ratio)
    {
        const int r = _color1.red()   * (1 - _ratio) + _color2.red()  * _ratio;
        const int g = _color1.green() * (1 - _ratio) + _color2.green()* _ratio;
        const int b = _color1.blue()  * (1 - _ratio) + _color2.blue() * _ratio;
        return QColor(r, g, b, 255);
    };

    QFont getButtonFont()
    {
        return Fonts::adjustedAppFont(14, Fonts::FontWeight::SemiBold);
    }

    QFont getTextFont()
    {
        return Fonts::adjustedAppFont(15);
    }

    QFont getDurationFont()
    {
        return Fonts::adjustedAppFont(13);
    }
}

namespace Ui
{
    CallButton::CallButton(QWidget* _parent, const QString& _aimId, const bool _isOutgoing)
        : ClickableTextWidget(_parent, getButtonFont(), callButtonDefTextColor(), TextRendering::HorAligment::CENTER)
        , isSelected_(false)
        , isPressed_(false)
        , isOutgoing_(_isOutgoing)
        , aimId_(_aimId)
    {
        setFixedHeight(buttonHeight());
        setText(QT_TRANSLATE_NOOP("chat_event", "CALL BACK"));
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }

    void CallButton::setSelected(const bool _isSelected)
    {
        if (isSelected_ != _isSelected)
        {
            isSelected_ = _isSelected;
            setColor(isSelected_ ? Styling::getParameters().getColor(Styling::StyleVariable::TEXT_SOLID_PERMANENT) : callButtonDefTextColor());
            update();
        }
    }

    bool CallButton::isPressed() const
    {
        return isPressed_;
    }

    void CallButton::paintEvent(QPaintEvent* _e)
    {
        const auto isOutgoing = isOutgoing_;
        const auto normal = isOutgoing ? Styling::StyleVariable::PRIMARY_BRIGHT : Styling::StyleVariable::BASE_BRIGHT;
        const auto hover = isOutgoing ? Styling::StyleVariable::PRIMARY_BRIGHT_HOVER : Styling::StyleVariable::BASE_BRIGHT_HOVER;
        const auto active = isOutgoing ? Styling::StyleVariable::PRIMARY_BRIGHT_ACTIVE : Styling::StyleVariable::BASE_BRIGHT_ACTIVE;
        const auto select = Styling::StyleVariable::GHOST_SECONDARY;

        const auto bgVariable = isSelected_ ? select : (isHovered() ? (isPressed_ ? active : hover) : normal);
        const auto bgColor = Styling::getParameters(aimId_).getColor(bgVariable);
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(bgColor);
            p.drawRoundedRect(rect(), Utils::scale_value(8), Utils::scale_value(8));
        }

        ClickableTextWidget::paintEvent(_e);
    }

    void CallButton::mouseMoveEvent(QMouseEvent * _e)
    {
        ClickableTextWidget::mouseMoveEvent(_e);
        update();
    }

    void CallButton::mousePressEvent(QMouseEvent * _e)
    {
        ClickableTextWidget::mousePressEvent(_e);
        isPressed_ = true;
        update();
    }

    void CallButton::mouseReleaseEvent(QMouseEvent * _e)
    {
        ClickableTextWidget::mouseReleaseEvent(_e);
        isPressed_ = false;
        update();
    }

    VoipEventItem::VoipEventItem(QWidget *parent, const HistoryControl::VoipEventInfoSptr& eventInfo)
        : MessageItemBase(parent)
        , EventInfo_(eventInfo)
        , isAvatarHovered_(false)
        , id_(-1)
        , friendlyName_(Logic::GetFriendlyContainer()->getFriendly(eventInfo->getContactAimid()))
        , Direction_(SelectDirection::NONE)
        , startSelectY_(0)
        , isSelection_(false)
        , callButton_(new CallButton(this, eventInfo->getContactAimid(), isOutgoing()))
        , timeWidget_(new MessageTimeWidget(this))
    {
        assert(EventInfo_);
        assert(!friendlyName_.isEmpty());

        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        if (!EventInfo_->isVisible())
        {
            assert(!"invisible voip events are not allowed in history control");
            setFixedHeight(0);
            return;
        }

        setAttribute(Qt::WA_TranslucentBackground);

        if (EventInfo_->isClickable())
            setMouseTracking(true);

        init();

        connect(Logic::GetAvatarStorage(), &Logic::AvatarStorage::avatarChanged, this, &VoipEventItem::onAvatarChanged);
        connect(Logic::GetFriendlyContainer(), &Logic::FriendlyContainer::friendlyChanged, this, &VoipEventItem::onContactNameChanged);
        connect(callButton_, &CallButton::clicked, this, &VoipEventItem::onCallButtonClicked);
        connect(this, &VoipEventItem::selectionChanged, this, &VoipEventItem::updateColors);
    }

    VoipEventItem::~VoipEventItem()
    {
        if (callButton_->isPressed() || !pressPoint_.isNull())
            emit pressedDestroyed();
    }

    QString VoipEventItem::formatRecentsText() const
    {
        return EventInfo_->formatRecentsText();
    }

    QString VoipEventItem::formatCopyText() const
    {
        const auto header = qsl("%1 (%2):\n").arg(friendlyName_, QDateTime::fromTime_t(getTime()).toString(qsl("dd.MM.yyyy hh:mm")));
        return header % formatRecentsText();
    }

    MediaType VoipEventItem::getMediaType() const
    {
        return MediaType::mediaTypeVoip;
    }

    Data::Quote VoipEventItem::getQuote() const
    {
        Data::Quote quote;
        quote.text_ = formatRecentsText();
        quote.type_ = Data::Quote::Type::text;
        quote.senderId_ = getContact();
        quote.chatId_ = getContact();
        quote.time_ = getTime();
        quote.msgId_ = getId();
        quote.senderFriendly_ = friendlyName_;
        return quote;
    }

    void VoipEventItem::updateHeight(const UpdateHeightOption _option)
    {
        const auto textWidth = getTextWidth();
        if (_option == UpdateHeightOption::forceRecalcTextHeight || (textWidth > 0 && textWidth != text_->cachedSize().width()))
        {
            text_->getHeight(textWidth);

            if (duration_)
                duration_->getHeight(textWidth);
        }

        const auto textTop = getTextTopMargin();
        text_->setOffsets(getIconSize().width() + MessageStyle::getBubbleHorPadding() * 2, textTop);

        if (duration_)
            duration_->setOffsets(getIconSize().width() + MessageStyle::getBubbleHorPadding() * 2, textTop + text_->cachedSize().height() + Utils::scale_value(2));

        const int bubbleHeight =
            textTop +
            text_->cachedSize().height() +
            (duration_ ? Utils::scale_value(2) + Utils::scale_value(14) : 0) +
            getTextBottomMargin() +
            buttonHeight() +
            buttonBottomMargin();

        if (BubbleRect_.height() != bubbleHeight)
        {
            BubbleRect_.setHeight(bubbleHeight);
            resetBubblePath();
        }

        updateCallButton();

        const auto height = MessageStyle::getTopMargin(hasTopMargin()) + bubbleHeight + bottomOffset();
        setFixedHeight(height);
    }

    void VoipEventItem::trackMenu(const QPoint& _pos)
    {
        auto menu = new ContextMenu(this);

        const auto aimId = getContact();
        const auto isDisabled = Logic::getContactListModel()->isReadonly(aimId);
        if (!isDisabled)
            menu->addActionWithIcon(qsl(":/context_menu/reply"), QT_TRANSLATE_NOOP("context_menu", "Reply"), makeData(qsl("quote")));

        menu->addActionWithIcon(qsl(":/context_menu/copy"), QT_TRANSLATE_NOOP("context_menu", "Copy"), makeData(qsl("copy")));
        menu->addActionWithIcon(qsl(":/context_menu/forward"), QT_TRANSLATE_NOOP("context_menu", "Forward"), makeData(qsl("forward")));

        menu->addSeparator();
        menu->addActionWithIcon(qsl(":/context_menu/delete"), QT_TRANSLATE_NOOP("context_menu", "Delete for me"), makeData(qsl("delete")));

        if (aimId != MyInfo()->aimId() && (isOutgoing() || Logic::getContactListModel()->isYouAdmin(aimId)))
            menu->addActionWithIcon(qsl(":/context_menu/deleteall"), QT_TRANSLATE_NOOP("context_menu", "Delete for all"), makeData(qsl("delete_all")));

        if (GetAppConfig().IsContextMenuFeaturesUnlocked())
            menu->addActionWithIcon(qsl(":/context_menu/copy"), qsl("Copy Message ID"), makeData(qsl("dev:copy_message_id")));

        connect(menu, &ContextMenu::triggered, this, &VoipEventItem::menu, Qt::QueuedConnection);
        connect(menu, &ContextMenu::triggered, menu, &ContextMenu::deleteLater, Qt::QueuedConnection);
        connect(menu, &ContextMenu::aboutToHide, menu, &ContextMenu::deleteLater, Qt::QueuedConnection);

        menu->popup(_pos);
    }

    void VoipEventItem::setTopMargin(const bool value)
    {
        HistoryControlPageItem::setTopMargin(value);

        if (!EventInfo_->isVisible())
            return;

        updateHeight();
    }

    void VoipEventItem::setHasAvatar(const bool _value)
    {
        if (!isOutgoing() && _value && isChat())
        {
            avatar_ = getAvatar(getContact(), friendlyName_, Utils::scale_bitmap(MessageStyle::getAvatarSize()));
            assert(!avatar_.isNull());
        }
        else
        {
            avatar_ = QPixmap();
        }

        HistoryControlPageItem::setHasAvatar(_value);
    }

    void VoipEventItem::mouseMoveEvent(QMouseEvent *_event)
    {
        isAvatarHovered_ = isAvatarHovered(_event->pos());
        setCursor(isAvatarHovered_ ? Qt::PointingHandCursor : Qt::ArrowCursor);

        MessageItemBase::mouseMoveEvent(_event);
    }

    void VoipEventItem::mousePressEvent(QMouseEvent* _event)
    {
        pressPoint_ = _event->pos();
        MessageItemBase::mousePressEvent(_event);
    }

    void VoipEventItem::mouseReleaseEvent(QMouseEvent *_event)
    {
        const auto pos = _event->pos();
        if (isAvatarHovered_ && isAvatarHovered(pos) && _event->button() == Qt::LeftButton)
            Utils::openDialogOrProfile(getContact());
        else if (BubbleRect_.contains(pos) && _event->button() == Qt::RightButton && isContextMenuEnabled())
            trackMenu(mapToGlobal(pos));

        pressPoint_ = QPoint();

        MessageItemBase::mouseReleaseEvent(_event);
    }

    void VoipEventItem::mouseDoubleClickEvent(QMouseEvent* _e)
    {
        if (_e->button() == Qt::LeftButton)
        {
            auto emitQuote = true;
            if (BubbleRect_.contains(_e->pos()))
            {
                const auto fixPos = _e->pos() - BubbleRect_.topLeft();
                const auto icon = getIcon(EventInfo_->isVideoCall(), EventInfo_->isMissed(), isSelected(), !EventInfo_->isIncomingCall());
                const auto iconArea = Utils::unscale_bitmap(icon.rect().translated(MessageStyle::getBubbleHorPadding(), MessageStyle::getBubbleVerPadding()));

                if (iconArea.contains(fixPos) || text_->contains(fixPos) || (duration_ && duration_->contains(fixPos)))
                    emitQuote = false;

                if (!confMembers_.empty())
                {
                    const auto confMemberArea = QRect(
                        BubbleRect_.left() + text_->horOffset() + text_->cachedSize().width(),
                        BubbleRect_.top() + confMemberTopMargin(),
                        BubbleRect_.width() - text_->horOffset() - text_->cachedSize().width() - MessageStyle::getBubbleHorPadding(),
                        confMemberSize() + confMemberCutWidth() * 2
                    );
                    if (confMemberArea.contains(_e->pos()))
                        emitQuote = false;
                }
            }

            if (emitQuote)
                emit quote({ getQuote() });
        }

        MessageItemBase::mouseDoubleClickEvent(_e);
    }

    void VoipEventItem::leaveEvent(QEvent* _event)
    {
        MessageItemBase::leaveEvent(_event);

        isAvatarHovered_ = false;
    }

    void VoipEventItem::paintEvent(QPaintEvent *)
    {
        if (!BubbleRect_.isValid())
            return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);

        if (const auto st = getLastStatus(); st != LastStatus::None)
            drawLastStatusIcon(p, st, getContact(), friendlyName_, 0);

        drawHeads(p);

        const auto bodyBrush = drawBubble(p);
        p.translate(BubbleRect_.topLeft());

        const auto icon = getIcon(EventInfo_->isVideoCall(), EventInfo_->isMissed(), isSelected(), !EventInfo_->isIncomingCall());
        p.drawPixmap(MessageStyle::getBubbleHorPadding(), MessageStyle::getBubbleVerPadding(), icon);

        text_->draw(p);
        if (duration_)
            duration_->draw(p);

        drawConfMembers(p, bodyBrush);

        if (!isOutgoing() && !avatar_.isNull())
            p.drawPixmap(getAvatarRect(), avatar_);

        if (Q_UNLIKELY(Ui::GetAppConfig().IsShowMsgIdsEnabled()))
        {
            p.setPen(Styling::getParameters().getColor(Styling::StyleVariable::TEXT_SOLID));
            p.setFont(Fonts::appFontScaled(10, Fonts::FontWeight::SemiBold));

            const auto x = BubbleRect_.width() - MessageStyle::getBorderRadius();
            const auto y = BubbleRect_.height();
            Utils::drawText(p, QPointF(x, y), Qt::AlignRight | Qt::AlignBottom, QString::number(getId()));
        }
    }

    void VoipEventItem::resizeEvent(QResizeEvent* _event)
    {
        const auto outgoing = isOutgoing();
        auto w = _event->size().width();

        auto left = MessageStyle::getLeftMargin(outgoing, w);
        if (!outgoing)
            left -= (MessageStyle::getAvatarSize() + MessageStyle::getAvatarRightMargin());

        QMargins margins(
            left,
            MessageStyle::getTopMargin(hasTopMargin()),
            MessageStyle::getRightMargin(outgoing, w),
            0
        );

        auto margin = 0;
        if (outgoing)
        {
            margin = w - MessageStyle::getVoipDesiredWidth() - margins.right() - margins.left();
            if (margin > 0)
                margins.setLeft(margins.left() + margin);
        }
        else
        {
            if (!avatar_.isNull())
                margins.setLeft(margins.left() + MessageStyle::getAvatarSize() + MessageStyle::getAvatarRightMargin());

            margin = w - MessageStyle::getVoipDesiredWidth() - margins.right() - margins.left();
            if (margin > 0)
                margins.setRight(margins.right() + margin);
        }

        QRect newBubbleRect(QPoint(), _event->size());
        newBubbleRect = newBubbleRect.marginsRemoved(margins);
        newBubbleRect.setHeight(newBubbleRect.height() - bottomOffset());

        if (BubbleRect_ != newBubbleRect)
        {
            const bool widthChanged = newBubbleRect.width() != BubbleRect_.width();
            BubbleRect_ = (newBubbleRect.isValid() ? newBubbleRect : QRect());
            resetBubblePath();

            if (widthChanged && BubbleRect_.isValid())
            {
                if (const auto textWidth = getTextWidth(); textWidth > 0 && textWidth != text_->cachedSize().width())
                {
                    const auto oldTextH = text_->cachedSize().height();
                    const auto newTextH = text_->getHeight(textWidth);

                    if (duration_)
                        duration_->getHeight(textWidth);

                    if (oldTextH != newTextH)
                        updateHeight(UpdateHeightOption::forceRecalcTextHeight);
                }
            }
        }

        updateTimePosition();
        updateCallButton();

        HistoryControlPageItem::resizeEvent(_event);
    }

    void VoipEventItem::init()
    {
        assert(EventInfo_);
        setContact(EventInfo_->getContactAimid());

        if (EventInfo_->hasDuration())
        {
            duration_ = TextRendering::MakeTextUnit(QT_TRANSLATE_NOOP("chat_event", "Duration %1").arg(EventInfo_->formatDurationText()));
            duration_->init(getDurationFont(), Styling::getParameters().getColor(Styling::StyleVariable::BASE_PRIMARY), QColor(), QColor(), QColor(), TextRendering::HorAligment::LEFT, 1);
            duration_->evaluateDesiredSize();
        }
        else if (duration_)
        {
            duration_.reset();
        }

        updateText();

        updateColors();

        confMembers_.clear();
        if (const auto& members = EventInfo_->getConferenceMembers(); !members.empty())
        {
            for (const auto& m : members)
            {
                if (m != MyInfo()->aimId())
                    confMembers_[m] = getAvatar(m, Logic::GetFriendlyContainer()->getFriendly(m), Utils::scale_bitmap(confMemberSize()));
            }
        }

        timeWidget_->setContact(getContact());
        timeWidget_->setOutgoing(isOutgoing());
        timeWidget_->setTime(EventInfo_->getTimestamp());
    }

    QRect VoipEventItem::getAvatarRect() const
    {
        if (!hasAvatar() || !isChat())
            return QRect();

        return QRect(
            MessageStyle::getLeftMargin(isOutgoing(), width()),
            BubbleRect_.top(),
            MessageStyle::getAvatarSize(),
            MessageStyle::getAvatarSize()
        );
    }

    bool VoipEventItem::isAvatarHovered(const QPoint &mousePos) const
    {
        return !hasAvatar() && getAvatarRect().contains(mousePos);
    }

    bool VoipEventItem::isOutgoing() const
    {
        return !EventInfo_->isIncomingCall();
    }

    int32_t VoipEventItem::getTime() const
    {
        return EventInfo_->getTimestamp();
    }

    void VoipEventItem::setLastStatus(LastStatus _lastStatus)
    {
        if (getLastStatus() != _lastStatus)
        {
            HistoryControlPageItem::setLastStatus(_lastStatus);
            update();
        }
    }

    void VoipEventItem::setId(const qint64 _id, const QString& _internalId)
    {
        id_ = _id;
        internalId_ = _internalId;
    }

    qint64 VoipEventItem::getId() const
    {
        return id_;
    }

    void VoipEventItem::selectByPos(const QPoint& _from, const QPoint& _to)
    {
        if (!isSelection_)
        {
            isSelection_ = true;
            startSelectY_ = _from.y();
        }

        const QRect widgetRect = QRect(mapToGlobal(rect().topLeft()), mapToGlobal(rect().bottomRight()));
        const auto isCursorOverWidget =
            (widgetRect.top() <= _from.y() && widgetRect.bottom() >= _to.y()) ||
            (widgetRect.top() >= _from.y() && widgetRect.bottom() <= _to.y());

        if (isCursorOverWidget)
        {
            if (!isSelected())
                select();
            return;
        }

        const auto distanceToWidgetTop = std::abs(_from.y() - widgetRect.top());
        const auto distanceToWidgetBottom = std::abs(_from.y() - widgetRect.bottom());
        const auto isCursorCloserToTop = (distanceToWidgetTop < distanceToWidgetBottom);

        if (Direction_ == SelectDirection::NONE)
            Direction_ = (isCursorCloserToTop ? SelectDirection::DOWN : SelectDirection::UP);

        const auto isDirectionDown = (Direction_ == SelectDirection::DOWN);
        const auto isDirectionUp = (Direction_ == SelectDirection::UP);

        if (isSelected())
        {
            const auto needToClear =
                (isCursorCloserToTop && isDirectionDown) ||
                (!isCursorCloserToTop && isDirectionUp);

            if (needToClear && (startSelectY_ < widgetRect.top() || startSelectY_ > widgetRect.bottom()))
            {
                clearSelection();
                isSelection_ = true;
            }
            return;
        }

        const auto needToSelect =
            (isCursorCloserToTop && isDirectionUp) ||
            (!isCursorCloserToTop && isDirectionDown);

        if (needToSelect)
            select();
    }

    void VoipEventItem::clearSelection()
    {
        isSelection_ = false;
        Direction_ = SelectDirection::NONE;

        HistoryControlPageItem::clearSelection();
    }

    void VoipEventItem::updateStyle()
    {
        timeWidget_->updateStyle();
        update();
    }

    void VoipEventItem::updateFonts()
    {
        if (text_)
        {
            text_->init(getTextFont(), MessageStyle::getTextColor(), QColor(), QColor(), QColor(), TextRendering::HorAligment::LEFT, 2, TextRendering::LineBreakType::PREFER_SPACES);
            text_->evaluateDesiredSize();
        }

        if (duration_)
        {
            duration_->init(getDurationFont(), Styling::getParameters().getColor(Styling::StyleVariable::BASE_PRIMARY), QColor(), QColor(), QColor(), TextRendering::HorAligment::LEFT, 1);
            duration_->evaluateDesiredSize();
        }

        timeWidget_->updateText();
        callButton_->setFont(getButtonFont());

        updateHeight(UpdateHeightOption::forceRecalcTextHeight);
        update();
    }

    void VoipEventItem::updateWith(VoipEventItem& _voipItem)
    {
        if (EventInfo_ == _voipItem.EventInfo_ || !_voipItem.EventInfo_)
            return;

        setChainedToPrev(_voipItem.isChainedToNextMessage());
        setChainedToPrev(_voipItem.isChainedToPrevMessage());

        EventInfo_ = _voipItem.EventInfo_;
        init();
        updateHeight(UpdateHeightOption::forceRecalcTextHeight);
        update();
    }

    void VoipEventItem::setQuoteSelection()
    {
        QuoteAnimation_.startQuoteAnimation();
    }

    void VoipEventItem::updateSize()
    {
        updateHeight();
    }

    void VoipEventItem::onChainsChanged()
    {
        resetBubblePath();
    }

    QColor VoipEventItem::getTextColor(const bool isHovered)
    {
        return isHovered ? QColor(ql1s("#ffffff")) : MessageStyle::getTextColor();
    }

    void VoipEventItem::updateText()
    {
        const auto eventText = EventInfo_->formatItemText();
        if (!text_)
        {
            text_ = TextRendering::MakeTextUnit(eventText, {}, TextRendering::LinksVisible::DONT_SHOW_LINKS, TextRendering::ProcessLineFeeds::REMOVE_LINE_FEEDS);
            text_->init(getTextFont(), MessageStyle::getTextColor(), QColor(), QColor(), QColor(), TextRendering::HorAligment::LEFT, 2, TextRendering::LineBreakType::PREFER_SPACES);
        }
        else
        {
            text_->setText(eventText);
        }

        text_->evaluateDesiredSize();
    }

    void VoipEventItem::updateTimePosition()
    {
        if (!BubbleRect_.isValid())
            return;

        const auto x = BubbleRect_.right() + 1 - timeWidget_->width() - timeWidget_->getHorMargin();
        const auto y = BubbleRect_.bottom() + 1 - timeWidget_->height() - timeWidget_->getVerMargin();

        timeWidget_->move(x, y);
    }

    void VoipEventItem::updateCallButton()
    {
        if (!BubbleRect_.isValid())
            return;

        callButton_->setFixedWidth(BubbleRect_.width() - MessageStyle::getBubbleHorPadding() * 2);
        callButton_->move(BubbleRect_.left() + MessageStyle::getBubbleHorPadding(), BubbleRect_.bottom() + 1 - buttonBottomMargin() - buttonHeight());
    }

    void VoipEventItem::updateColors()
    {
        const auto sel = isSelected();
        text_->setColor(sel ? Styling::getParameters().getColor(Styling::StyleVariable::TEXT_SOLID_PERMANENT) : (EventInfo_->isMissed() ? missedColor() : MessageStyle::getTextColor()));

        if (duration_)
            duration_->setColor(sel ? Styling::StyleVariable::TEXT_SOLID_PERMANENT : (isOutgoing() ? Styling::StyleVariable::PRIMARY_PASTEL : Styling::StyleVariable::BASE_PRIMARY), getContact());

        callButton_->setSelected(sel);
        timeWidget_->setSelected(sel);

        update();
    }

    int VoipEventItem::getTextWidth() const
    {
        const auto bubbleWidth = BubbleRect_.isValid() ? BubbleRect_.width() : MessageStyle::getVoipDesiredWidth();
        auto size = bubbleWidth - getIconSize().width() - MessageStyle::getBubbleHorPadding() * 3;

        if (const auto confMembers = confMembers_.size())
            size -= (confMembers * confMemberSize() - (confMembers - 1) * confMemberOverlap()) + Utils::scale_value(6);

        return size;
    }

    int VoipEventItem::getTextTopMargin() const
    {
        return (text_->getLineCount() > 1 || duration_) ? Utils::scale_value(10) : Utils::scale_value(18);
    }

    int VoipEventItem::getTextBottomMargin() const
    {
        return (text_->getLineCount() > 1 || duration_) ? Utils::scale_value(16) : Utils::scale_value(20);
    }

    QBrush VoipEventItem::drawBubble(QPainter& _p)
    {
        const auto outgoing = isOutgoing();

        if (Bubble_.isEmpty())
        {
            int flags = Utils::RenderBubbleFlags::AllRounded;
            if (isChainedToPrevMessage())
                flags |= (isOutgoing() ? Utils::RenderBubbleFlags::RightTopSmall : Utils::RenderBubbleFlags::LeftTopSmall);

            if (isChainedToNextMessage())
                flags |= (isOutgoing() ? Utils::RenderBubbleFlags::RightBottomSmall : Utils::RenderBubbleFlags::LeftBottomSmall);

            Bubble_ = Utils::renderMessageBubble(BubbleRect_, MessageStyle::getBorderRadius(), MessageStyle::getBorderRadiusSmall(), (Utils::RenderBubbleFlags)flags);
            assert(!Bubble_.isEmpty());
        }

        assert(BubbleRect_.width() > 0);

        auto bodyBrush = Ui::MessageStyle::getBodyBrush(outgoing, isSelected(), getContact());

        const auto needBorder = !outgoing && Styling::getParameters(getContact()).isBorderNeeded();
        if (!needBorder)
            Utils::drawBubbleShadow(_p, Bubble_);

        _p.fillPath(Bubble_, bodyBrush);
        if (QColor qColor = QuoteAnimation_.quoteColor(); qColor.isValid())
        {
            _p.fillPath(Bubble_, qColor);
            bodyBrush.setColor(blendColors(bodyBrush.color(), qColor, qColor.alphaF()));
        }

        if (needBorder)
            Utils::drawBubbleRect(_p, Bubble_.boundingRect(), MessageStyle::getBorderColor(), MessageStyle::getBorderWidth(), MessageStyle::getBorderRadius());

        return bodyBrush;
    }

    void VoipEventItem::drawConfMembers(QPainter& _p, const QBrush& _bodyBrush)
    {
        if (confMembers_.empty())
            return;

        Utils::PainterSaver ps(_p);

        _p.setPen(Qt::NoPen);
        _p.setBrush(_bodyBrush);

        int i = 0;
        for (const auto& [_, avatar] : confMembers_)
        {
            assert(!avatar.isNull());

            const auto x = BubbleRect_.width()
                - MessageStyle::getBubbleHorPadding()
                - i * (confMemberSize() - confMemberOverlap())
                - confMemberSize();
            const auto y = confMemberTopMargin();

            if (i > 0)
                _p.drawEllipse(
                    x - confMemberCutWidth(),
                    y - confMemberCutWidth(),
                    confMemberSize() + confMemberCutWidth() * 2,
                    confMemberSize() + confMemberCutWidth() * 2
                );

            _p.drawPixmap(x, y, avatar);

            ++i;
        }
    }

    void VoipEventItem::resetBubblePath()
    {
        Bubble_ = QPainterPath();
    }

    void VoipEventItem::onAvatarChanged(const QString& _aimId)
    {
        if (getContact() == _aimId && !avatar_.isNull())
        {
            avatar_ = getAvatar(getContact(), friendlyName_, Utils::scale_bitmap(MessageStyle::getAvatarSize()));
            update();
        }
        else if (std::any_of(confMembers_.begin(), confMembers_.end(), [&_aimId](const auto& _p) { return _p.first == _aimId; }))
        {
            confMembers_[_aimId] = getAvatar(_aimId, Logic::GetFriendlyContainer()->getFriendly(_aimId), Utils::scale_bitmap(confMemberSize()));
            update();
        }
    }

    void VoipEventItem::onContactNameChanged(const QString& _aimId, const QString& _friendlyName)
    {
        if (getContact() == _aimId)
        {
            friendlyName_ = _friendlyName;
            updateText();
            updateHeight(UpdateHeightOption::forceRecalcTextHeight);
            update();
        }
    }

    void VoipEventItem::onCallButtonClicked()
    {
        const auto &contactAimid = getContact();
        assert(!contactAimid.isEmpty());
        if (EventInfo_->isVideoCall())
            Ui::GetDispatcher()->getVoipController().setStartV(contactAimid.toUtf8().constData(), false, "ChatVideo");
        else
            Ui::GetDispatcher()->getVoipController().setStartA(contactAimid.toUtf8().constData(), false, "ChatVideo");
    }

    void VoipEventItem::menu(QAction* _action)
    {
        const auto params = _action->data().toMap();
        const auto command = params[qsl("command")].toString();

        if (command == ql1s("dev:copy_message_id"))
        {
            const auto idStr = QString::number(getId());

            QApplication::clipboard()->setText(idStr);
            qDebug() << "message id" << idStr;
        }
        else if (command == ql1s("copy"))
        {
            emit copy(formatCopyText());
        }
        else if (command == ql1s("quote"))
        {
            emit quote({ getQuote() });
        }
        else if (command == ql1s("forward"))
        {
            emit forward({ getQuote() });
        }
        else if (command == ql1s("delete") || command == ql1s("delete_all"))
        {
            QString text = QT_TRANSLATE_NOOP("popup_window", "Are you sure you want to delete message?");

            auto confirm = Utils::GetConfirmationWithTwoButtons(
                QT_TRANSLATE_NOOP("popup_window", "CANCEL"),
                QT_TRANSLATE_NOOP("popup_window", "YES"),
                text,
                QT_TRANSLATE_NOOP("popup_window", "Delete message"),
                nullptr
            );

            if (confirm)
            {
                const auto is_shared = (command == ql1s("delete_all"));
                GetDispatcher()->deleteMessage(getId(), internalId_, getContact(), is_shared);
            }
        }
    }
}
