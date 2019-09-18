#include "stdafx.h"

#include "../../../../corelib/enumerations.h"

#include "../../../controls/TextUnit.h"

#include "../../../core_dispatcher.h"
#include "../../../fonts.h"
#include "../../../gui_settings.h"
#include "../../../utils/PainterPath.h"
#include "../../../utils/utils.h"
#include "../../../styles/ThemeParameters.h"
#include "../../../cache/ColoredCache.h"
#include "../../contact_list/ContactListModel.h"

#include "../../input_widget/InputWidgetUtils.h"

#include "../../sounds/SoundsManager.h"

#include "../ActionButtonWidget.h"
#include "../MessageStyle.h"
#include "../MessageStatusWidget.h"
#include "../FileSharingInfo.h"

#include "../../MainPage.h"

#include "ComplexMessageItem.h"
#include "PttBlockLayout.h"
#include "Selection.h"


#include "PttBlock.h"

namespace
{
    QString formatDuration(const int32_t _seconds)
    {
        if (_seconds < 0)
            return qsl("00:00");

        const auto minutes = (_seconds / 60);
        const auto seconds = (_seconds % 60);

        return qsl("%1:%2")
            .arg(minutes, 2, 10, ql1c('0'))
            .arg(seconds, 2, 10, ql1c('0'));
    }

    QSize getButtonSize()
    {
        return Utils::scale_value(QSize(40, 40));
    }

    QSize getPlayIconSize()
    {
        return QSize(18, 18);
    }

    constexpr std::chrono::milliseconds maxPttLength = std::chrono::minutes(5);
    constexpr std::chrono::milliseconds animDuration = std::chrono::seconds(2);
    constexpr std::chrono::milliseconds animDownloadDelay = std::chrono::milliseconds(300);
    constexpr auto QT_ANGLE_MULT = 16;
    constexpr double idleProgressValue = 0.75;
}

UI_COMPLEX_MESSAGE_NS_BEGIN

namespace PttDetails
{
    struct PlayButtonIcons
    {
        QPixmap play_;
        QPixmap pause_;

        void fill(const QColor& _color)
        {
            const auto makeIcon = [](const QString& _path, const QColor& _clr)
            {
                return Utils::renderSvgScaled(_path, getPlayIconSize(), _clr);
            };

            play_ = makeIcon(qsl(":/videoplayer/video_play"), _color);
            pause_ = makeIcon(qsl(":/videoplayer/video_pause"), _color);
        }
    };

    PlayButton::PlayButton(QWidget* _parent, const QString& _aimId)
        : ClickableWidget(_parent)
        , aimId_(_aimId)
        , isSelected_(false)
        , isPressed_(false)
        , state_(ButtonState::play)
    {
        updateStyle();
        setFixedSize(getButtonSize());
        connect(this, &PlayButton::hoverChanged, this, Utils::QOverload<>::of(&PlayButton::update));
        connect(this, &PlayButton::pressed, this, [this]() { setPressed(true); });
        connect(this, &PlayButton::released, this, [this]() { setPressed(false); });
    }

    void PlayButton::setSelected(const bool _isSelected)
    {
        if (isSelected_ != _isSelected)
        {
            isSelected_ = _isSelected;
            update();
        }
    }

    void PlayButton::setPressed(const bool _isPressed)
    {
        if (isPressed_ != _isPressed)
        {
            isPressed_ = _isPressed;
            update();
        }

        MainPage::instance()->setFocusOnInput();
    }

    void PlayButton::setState(const ButtonState _state)
    {
        if (state_ != _state)
        {
            state_ = _state;
            update();
        }
    }

    void PlayButton::updateStyle()
    {
        assert(!aimId_.isEmpty());

        const auto params = Styling::getParameters(aimId_);
        normal_  = params.getColor(Styling::StyleVariable::PRIMARY);
        hovered_ = params.getColor(Styling::StyleVariable::PRIMARY_HOVER);
        pressed_ = params.getColor(Styling::StyleVariable::PRIMARY_ACTIVE);
        selected_= params.getColor(Styling::StyleVariable::TEXT_SOLID_PERMANENT);

        update();
    }

    void PlayButton::paintEvent(QPaintEvent* _e)
    {
        static Utils::ColoredCache<PlayButtonIcons> iconsNormal(Styling::StyleVariable::TEXT_SOLID_PERMANENT);
        static Utils::ColoredCache<PlayButtonIcons> iconsSelected(Styling::StyleVariable::PRIMARY);

        QColor bgColor;
        if (isSelected_)
        {
            bgColor = selected_;
        }
        else if (isHovered())
        {
            if (isPressed_)
                bgColor = pressed_;
            else
                bgColor = hovered_;
        }
        else
        {
            bgColor = normal_;
        }

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(bgColor);
        p.drawEllipse(rect());

        const auto& cache = isSelected_ ? iconsSelected : iconsNormal;
        const auto& icons = cache.get(aimId_);
        const auto icon = state_ == ButtonState::play ? icons.play_ : icons.pause_;

        const auto ratio = Utils::scale_bitmap_ratio();
        p.drawPixmap((width() - icon.width() / ratio) / 2, (height() - icon.height() / ratio) / 2, icon);
    }

    ButtonWithBackground::ButtonWithBackground(QWidget* _parent, const QString& _icon, const QString& _aimId, bool _isOutgoing)
        : CustomButton(_parent, _icon, QSize(20, 20))
        , aimId_(_aimId)
        , isOutgoing_(_isOutgoing)
    {
        updateStyle();
        setFixedSize(getButtonSize());
    }

    void ButtonWithBackground::updateStyle()
    {
        setDefaultColor(Styling::getParameters(aimId_).getColor(Styling::StyleVariable::PRIMARY_INVERSE));
        setActiveColor(Styling::getParameters(aimId_).getColor(Styling::StyleVariable::TEXT_SOLID_PERMANENT));
    }

    void ButtonWithBackground::paintEvent(QPaintEvent * _e)
    {
        {
            auto bgVar = isOutgoing() ? Styling::StyleVariable::PRIMARY_BRIGHT : Styling::StyleVariable::BASE_BRIGHT;
            if (isActive())
            {
                bgVar = Styling::StyleVariable::GHOST_SECONDARY;
            }
            else if (isHovered())
            {
                if (isPressed())
                    bgVar = isOutgoing() ? Styling::StyleVariable::PRIMARY_BRIGHT_ACTIVE :Styling::StyleVariable::BASE_BRIGHT_ACTIVE;
                else
                    bgVar = isOutgoing() ? Styling::StyleVariable::PRIMARY_BRIGHT_HOVER :Styling::StyleVariable::BASE_BRIGHT_HOVER;
            }

            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(Styling::getParameters(aimId_).getColor(bgVar));
            p.drawEllipse(rect());
        }

        CustomButton::paintEvent(_e);
    }

    bool ButtonWithBackground::isOutgoing() const
    {
        return isOutgoing_;
    }

    ProgressWidget::ProgressWidget(QWidget* _parent, const ButtonType _type, bool _isOutgoing)
        : QWidget(_parent)
        , type_(_type)
        , isSelected_(false)
        , progress_(0.)
        , isOutgoing_(_isOutgoing)
    {
        setFixedSize(getButtonSize());

        anim_.start([this]() { update(); }, 0., 360., animDuration.count(), anim::linear, -1);
    }

    ProgressWidget::~ProgressWidget()
    {
        anim_.finish();
    }

    void ProgressWidget::setSelected(const bool _isSelected)
    {
        if (isSelected_ != _isSelected)
        {
            isSelected_ = _isSelected;
            update();
        }
    }

    void ProgressWidget::setProgress(const double _progress)
    {
        progress_ = _progress;
        update();
    }

    void ProgressWidget::updateStyle()
    {
    }

    void ProgressWidget::paintEvent(QPaintEvent* _e)
    {
        QColor bgColor;
        if (type_ == ButtonType::play)
        {
            bgColor = isSelected_ ? Qt::white : Styling::getParameters().getColor(Styling::StyleVariable::PRIMARY);
        }
        else
        {
            bgColor = isSelected_
                ? Styling::getParameters().getColor(Styling::StyleVariable::GHOST_TERTIARY)
                : (isOutgoing_
                    ? Styling::getParameters().getColor(Styling::StyleVariable::PRIMARY_BRIGHT)
                    : Styling::getParameters().getColor(Styling::StyleVariable::BASE_BRIGHT));
        }

        QColor lineColor;
        if (type_ == ButtonType::play)
        {
            lineColor = isSelected_ ? Styling::getParameters().getColor(Styling::StyleVariable::PRIMARY) : Styling::getParameters().getColor(Styling::StyleVariable::PRIMARY_BRIGHT);
        }
        else
        {
            lineColor = isSelected_
                ? Styling::getParameters().getColor(Styling::StyleVariable::TEXT_SOLID_PERMANENT)
                : Styling::getParameters().getColor(Styling::StyleVariable::PRIMARY_INVERSE);
        }

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(bgColor);
        p.drawEllipse(rect());

        const auto animAngle = anim_.isRunning() ? anim_.current() : 0.0;
        const auto baseAngle = (animAngle * QT_ANGLE_MULT);
        const auto progressAngle = (int)std::ceil(progress_ * 360 * QT_ANGLE_MULT);

        const QPen pen(lineColor, Utils::scale_value(2));
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        static const auto r2 = rect().adjusted(Utils::scale_value(8), Utils::scale_value(8), -Utils::scale_value(8), -Utils::scale_value(8));
        p.drawArc(r2, -baseAngle, -progressAngle);
    }
}

PttBlock::PttBlock(
    ComplexMessageItem *_parent,
    const QString &_link,
    const int32_t _durationSec,
    int64_t _id,
    int64_t _prevId)
    : FileSharingBlockBase(_parent, _link, core::file_sharing_content_type(core::file_sharing_base_content_type::ptt))
    , durationMSec_(_durationSec * 1000)
    , durationText_(formatDuration(_durationSec))
    , isDecodedTextCollapsed_(true)
    , isPlayed_(false)
    , isPlaybackScheduled_(false)
    , playbackState_(PlaybackState::Stopped)
    , playbackProgressMsec_(0)
    , playbackProgressAnimation_(nullptr)
    , playingId_(-1)
    , pttLayout_(nullptr)
    , textRequestId_(-1)
    , id_(_id)
    , prevId_(_prevId)
    , isOutgoing_(_parent->isOutgoing())
    , buttonPlay_(nullptr)
    , buttonText_(nullptr)
    , buttonCollapse_(nullptr)
    , progressPlay_(nullptr)
    , progressText_(nullptr)
    , downloadAnimDelay_(nullptr)
{
    assert(_durationSec >= 0);

    if (!getParentComplexMessage()->isHeadless())
        buttonPlay_ = new PttDetails::PlayButton(this, getChatAimid());

    pttLayout_ = new PttBlockLayout();
    setBlockLayout(pttLayout_);
    setLayout(pttLayout_);
    setMouseTracking(true);
}

PttBlock::PttBlock(ComplexMessageItem *_parent,
    const HistoryControl::FileSharingInfoSptr& _fsInfo,
    const int32_t _durationSec,
    int64_t _id,
    int64_t _prevId)
    : FileSharingBlockBase(_parent, _fsInfo->GetUri(), core::file_sharing_content_type(core::file_sharing_base_content_type::ptt))
    , durationMSec_(_durationSec * 1000)
    , durationText_(formatDuration(_durationSec))
    , isDecodedTextCollapsed_(true)
    , isPlayed_(false)
    , isPlaybackScheduled_(false)
    , playbackState_(PlaybackState::Stopped)
    , playbackProgressMsec_(0)
    , playbackProgressAnimation_(nullptr)
    , playingId_(-1)
    , pttLayout_(nullptr)
    , textRequestId_(-1)
    , id_(_id)
    , prevId_(_prevId)
    , isOutgoing_(_parent->isOutgoing())
    , buttonPlay_(nullptr)
    , buttonText_(nullptr)
    , buttonCollapse_(nullptr)
    , progressPlay_(nullptr)
    , progressText_(nullptr)
    , downloadAnimDelay_(nullptr)
{
    assert(_durationSec >= 0);

    if (!getParentComplexMessage()->isHeadless())
        buttonPlay_ = new PttDetails::PlayButton(this, getChatAimid());

    pttLayout_ = new PttBlockLayout();
    setBlockLayout(pttLayout_);
    setLayout(pttLayout_);
    setMouseTracking(true);

    if (_fsInfo->IsOutgoing())
    {
        setLocalPath(_fsInfo->GetLocalPath());
        const auto &uploadingProcessId = _fsInfo->GetUploadingProcessId();
        if (!uploadingProcessId.isEmpty())
        {
            setLoadedFromLocal(true);
            setUploadId(uploadingProcessId);
            onDataTransferStarted();
            QObject::connect(this, &PttBlock::uploaded, this, [this]()
            {
                requestMetainfo(false);
            });
        }
    }
}

PttBlock::~PttBlock() = default;

void PttBlock::clearSelection()
{
    FileSharingBlockBase::clearSelection();

    if (decodedTextCtrl_)
    {
        decodedTextCtrl_->clearSelection();
        updateDecodedTextSelection(isSelected());
        update();
    }
}

QSize PttBlock::getCtrlButtonSize() const
{
    return getButtonSize();
}

QString PttBlock::getSelectedText(const bool, const TextDestination) const
{
    QString result;
    result.reserve(512);

    if (isSelected())
    {
        result += getLink();
        result += QChar::LineFeed;
    }

    if (decodedTextCtrl_)
        result += decodedTextCtrl_->getSelectedText();

    return result;
}

bool PttBlock::updateFriendly(const QString&/* _aimId*/, const QString&/* _friendly*/)
{
    return false;
}

QSize PttBlock::getTextButtonSize() const
{
    return getButtonSize();
}

bool PttBlock::hasDecodedText() const
{
    return !decodedText_.isEmpty();
}

bool PttBlock::isDecodedTextCollapsed() const
{
    return isDecodedTextCollapsed_;
}

void PttBlock::selectByPos(const QPoint& from, const QPoint& to, const BlockSelectionType selection)
{
    assert(to.y() >= from.y());

    if (!isDecodedTextVisible())
    {
        FileSharingBlockBase::selectByPos(from, to, selection);

        if (decodedTextCtrl_)
            decodedTextCtrl_->clearSelection();
    }
    else if (selection == BlockSelectionType::Full)
    {
        setSelected(true);

        if (decodedTextCtrl_)
            decodedTextCtrl_->selectAll();
    }
    else
    {
        const auto localFrom = mapFromGlobal(from);
        const auto localTo = mapFromGlobal(to);

        const auto isHeaderSelected = (localFrom.y() < MessageStyle::Ptt::getPttBlockHeight());
        setSelected(isHeaderSelected);

        if (decodedTextCtrl_)
            decodedTextCtrl_->select(localFrom, localTo);
    }

    updateDecodedTextSelection(isSelected());

    update();
}

void PttBlock::setCtrlButtonGeometry(const QRect &_rect)
{
    assert(!_rect.isEmpty());
    assert(!getParentComplexMessage()->isHeadless());

    buttonPlay_->move(_rect.topLeft());
    buttonPlay_->show();

    if (progressPlay_)
        progressPlay_->move(_rect.topLeft());
}

int32_t PttBlock::setDecodedTextWidth(const int32_t _width)
{
    return decodedTextCtrl_ ? decodedTextCtrl_->getHeight(_width) : 0;
}

void PttBlock::setDecodedTextOffsets(int _x, int _y)
{
    if (decodedTextCtrl_)
        decodedTextCtrl_->setOffsets(_x, _y);
}

void PttBlock::setTextButtonGeometry(const QPoint& _pos)
{
    if (buttonText_)
        buttonText_->move(_pos);

    if (buttonCollapse_)
        buttonCollapse_->move(_pos);

    if (progressText_)
        progressText_->move(_pos);
}

void PttBlock::onMenuOpenFolder()
{
    showFileInDir(Utils::OpenAt::Folder);
}

void PttBlock::drawBlock(QPainter &_p, const QRect& _rect, const QColor& quote_color)
{
    if (!isStandalone() && isSelected())
    {
        const auto &bubbleRect = pttLayout_->getContentRect();
        renderClipPaths(bubbleRect);
        drawBubble(_p, bubbleRect);
    }

    drawDuration(_p);
    drawPlaybackProgress(_p, playbackProgressMsec_, durationMSec_);

    if (decodedTextCtrl_ && !isDecodedTextCollapsed_)
        decodedTextCtrl_->draw(_p);
}

void PttBlock::initializeFileSharingBlock()
{
    connectSignals();

    requestMetainfo(false);
}

void PttBlock::onDataTransferStarted()
{
    if (isFileDownloaded())
        return;

    if (!downloadAnimDelay_)
    {
        downloadAnimDelay_ = new QTimer(this);
        downloadAnimDelay_->setSingleShot(true);
        downloadAnimDelay_->setInterval(animDownloadDelay.count());
        connect(downloadAnimDelay_, &QTimer::timeout, this, &PttBlock::showDownloadAnimation);
    }

    downloadAnimDelay_->start();
}

void PttBlock::showDownloadAnimation()
{
    assert((!getParentComplexMessage()->isHeadless()));

    if (isFileDownloaded())
        return;

    if (!progressPlay_)
    {
        progressPlay_ = new PttDetails::ProgressWidget(this, PttDetails::ProgressWidget::ButtonType::play, isOutgoing());
        progressPlay_->setSelected(isSelected());
        progressPlay_->setProgress(idleProgressValue);
        progressPlay_->move(buttonPlay_->pos());
        progressPlay_->show();
    }

    buttonPlay_->hide();
}

void PttBlock::onDataTransferStopped()
{
}

void PttBlock::onDownloaded()
{
    assert(!getParentComplexMessage()->isHeadless());

    buttonPlay_->show();

    if (progressPlay_)
    {
        progressPlay_->hide();
        progressPlay_->deleteLater();
        progressPlay_ = nullptr;
    }

    if (isPlaybackScheduled_)
    {
        isPlaybackScheduled_ = false;

        startPlayback();
    }
}

void PttBlock::onDownloadedAction()
{
    if (isPlaybackScheduled_)
    {
        isPlaybackScheduled_ = false;

        startPlayback();
    }
}

void PttBlock::onDataTransfer(const int64_t _bytesTransferred, const int64_t _bytesTotal)
{
    if (progressPlay_)
    {
        const auto progress = ((double)_bytesTransferred / (double)_bytesTotal);
        progressPlay_->setProgress(progress);
    }
}

void PttBlock::onDownloadingFailed(const int64_t _seq)
{
    assert(textRequestId_ >= -1);
    assert(_seq >= -1);

    if (textRequestId_ != _seq)
        return;

    getParentComplexMessage()->replaceBlockWithSourceText(this);
}

void PttBlock::onLocalCopyInfoReady(const bool isCopyExists)
{
    const auto isPlayed = (isPlayed_ || isCopyExists);

    const auto isPlayedStatusChanged = (isPlayed != isPlayed_);
    if (!isPlayedStatusChanged)
        return;

    isPlayed_ = isPlayed;

    updateButtonsStates();
}

void PttBlock::onMetainfoDownloaded()
{
    if (buttonText_)
    {
        buttonText_->setVisible(recognize_);
    }
    else if (recognize_ && !build::is_dit())
    {
        buttonText_ = new PttDetails::ButtonWithBackground(this, qsl(":/ptt/text_icon"), getChatAimid(), isOutgoing());
        buttonText_->setActive(isSelected());
        buttonText_->setFixedSize(getButtonSize());
        buttonText_->move(pttLayout_->getTextButtonRect().topLeft());
        buttonText_->show();

        connect(buttonText_, &PttDetails::ButtonWithBackground::clicked, this, &PttBlock::onTextButtonClicked);
    }
}

void PttBlock::onPreviewMetainfoDownloaded(const QString &_miniPreviewUri, const QString &_fullPreviewUri)
{
    Q_UNUSED(_miniPreviewUri);
    Q_UNUSED(_fullPreviewUri);

    assert(!"you're not expected to be here");
}

void PttBlock::connectSignals()
{
    assert(!getParentComplexMessage()->isHeadless());

    connect(buttonPlay_, &PttDetails::PlayButton::clicked, this, &PttBlock::onPlayButtonClicked);

    connect(GetDispatcher(), &core_dispatcher::speechToText, this, &PttBlock::onPttText);

    connect(GetSoundsManager(), &SoundsManager::pttPaused,   this, &PttBlock::onPttPaused);
    connect(GetSoundsManager(), &SoundsManager::pttFinished, this, &PttBlock::onPttFinished);
}

void PttBlock::drawBubble(QPainter &_p, const QRect &_bubbleRect)
{
    assert(!_bubbleRect.isEmpty());
    assert(!isStandalone());

    Utils::PainterSaver ps(_p);
    _p.setRenderHint(QPainter::Antialiasing);

    const auto &bodyBrush = MessageStyle::getBodyBrush(isOutgoing(), isSelected(), getChatAimid());
    _p.setBrush(bodyBrush);
    _p.setPen(Qt::NoPen);

    _p.drawRoundedRect(_bubbleRect, MessageStyle::getBorderRadius(), MessageStyle::getBorderRadius());
}

void PttBlock::drawDuration(QPainter &_p)
{
    assert(!durationText_.isEmpty());
    assert(!getParentComplexMessage()->isHeadless());

    const auto durationLeftMargin = Utils::scale_value(12);
    const auto durationBaseline = Utils::scale_value(34);

    const auto textX = buttonPlay_->pos().x() + getButtonSize().width() + MessageStyle::getBubbleHorPadding();
    const auto textY = durationBaseline + (isStandalone() ? pttLayout_->getTopMargin() : MessageStyle::getBubbleVerPadding());

    Utils::PainterSaver ps(_p);

    _p.setFont(MessageStyle::Ptt::getDurationFont());
    _p.setPen(Styling::getParameters(getChatAimid()).getColor(isSelected() ? Styling::StyleVariable::TEXT_SOLID_PERMANENT : Styling::StyleVariable::TEXT_PRIMARY));

    const auto secondsLeft = (durationMSec_ - playbackProgressMsec_);
    const auto &text = (isPlaying() || isPaused()) ? formatDuration(secondsLeft / 1000) : (isPlayed_ ? durationText_ : durationText_ % ql1c(' ') % QChar(0x2022));

    assert(!text.isEmpty());
    _p.drawText(textX, textY, text);
}

void PttBlock::drawPlaybackProgress(QPainter &_p, const int32_t _progressMsec, const int32_t _durationMsec)
{
    assert(_progressMsec >= 0);
    assert(_durationMsec > 0);
    assert(!getParentComplexMessage()->isHeadless());

    const auto leftMargin  = buttonPlay_->pos().x() + getButtonSize().width() + MessageStyle::getBubbleHorPadding();
    const auto rightMargin = (recognize_ ? width() - pttLayout_->getTextButtonRect().left() : 0) + MessageStyle::getBubbleHorPadding();

    const auto totalWidth = width() - leftMargin - rightMargin;
    const auto progress = _durationMsec ? (double)_progressMsec / _durationMsec : 0.;
    const auto playedWidth = std::min(int(progress * totalWidth), totalWidth);

    const auto x = leftMargin;
    const auto y = Utils::scale_value(17) + (isStandalone() ? pttLayout_->getTopMargin() : MessageStyle::getBubbleVerPadding());

    const auto drawLine = [&_p, x, y](const auto& _color, const auto& _width)
    {
        _p.setPen(QPen(_color, MessageStyle::Ptt::getPttProgressWidth(), Qt::SolidLine, Qt::RoundCap));

        const auto halfPen = MessageStyle::Ptt::getPttProgressWidth();
        const auto top = y + halfPen;
        const auto retinaShift = Utils::is_mac_retina() ? 1 : 0;
        const auto left = x + halfPen - retinaShift;
        const auto right = std::max(x + _width - halfPen + retinaShift, left);
        _p.drawLine(left, top, right, top);
    };

    Utils::PainterSaver ps(_p);
    if (!isPlayed_)
    {
        drawLine(getPlaybackColor(), totalWidth);
    }
    else
    {
        drawLine(getProgressColor(), totalWidth);

        if (playedWidth > 0)
            drawLine(getPlaybackColor(), playedWidth);
    }
}

int32_t PttBlock::getPlaybackProgress() const
{
    assert(playbackProgressMsec_ >= 0);
    assert(playbackProgressMsec_ < maxPttLength.count());

    return playbackProgressMsec_;
}

void PttBlock::setPlaybackProgress(const int32_t _value)
{
    assert(_value >= 0);
    assert(_value < maxPttLength.count());

    playbackProgressMsec_ = _value;

    update();
}

void PttBlock::initializeDecodedTextCtrl()
{
    if (!decodedTextCtrl_)
    {
        decodedTextCtrl_ = TextRendering::MakeTextUnit(decodedText_);
        updateDecodedTextStyle();
    }
    else
    {
        decodedTextCtrl_->setText(decodedText_);
    }
}

bool PttBlock::isDecodedTextVisible() const
{
    return (hasDecodedText() && !isDecodedTextCollapsed());
}

bool PttBlock::isPaused() const
{
    return (playbackState_ == PlaybackState::Paused);
}

bool PttBlock::isPlaying() const
{
    return (playbackState_ == PlaybackState::Playing);
}

bool PttBlock::isStopped() const
{
    return (playbackState_ == PlaybackState::Stopped);
}

bool PttBlock::isTextRequested() const
{
    return (textRequestId_ != -1);
}

void PttBlock::renderClipPaths(const QRect &_bubbleRect)
{
    assert(!_bubbleRect.isEmpty());
    if (_bubbleRect.isEmpty())
        return;

    const auto isBubbleRectChanged = (_bubbleRect != bubbleClipRect_);
    if (isBubbleRectChanged)
    {
        bubbleClipPath_ = Utils::renderMessageBubble(_bubbleRect, MessageStyle::getBorderRadius());
        bubbleClipRect_ = _bubbleRect;
    }
}

void PttBlock::requestText()
{
    assert(textRequestId_ == -1);

    textRequestId_ = GetDispatcher()->pttToText(getLink(), Utils::GetTranslator()->getLang());

    isPlayed_ = true;

    updateButtonsStates();

    startTextRequestProgressAnimation();
}

void PttBlock::startTextRequestProgressAnimation()
{
    if (buttonText_ && buttonText_->isVisible())
    {
        buttonText_->hide();

        if (!progressText_)
        {
            progressText_ = new PttDetails::ProgressWidget(this, PttDetails::ProgressWidget::ButtonType::text, isOutgoing());
            progressText_->move(buttonText_->pos());
            progressText_->setProgress(idleProgressValue);
            progressText_->setSelected(isSelected());
            progressText_->show();
        }
    }
}

void PttBlock::stopTextRequestProgressAnimation()
{
    if (buttonText_)
    {
        buttonText_->show();

        if (progressText_)
        {
            progressText_->hide();
            progressText_->deleteLater();
            progressText_ = nullptr;
        }
    }
}

void PttBlock::startPlayback()
{
    isPlayed_ = true;

    assert(!isPlaying());
    if (isPlaying())
        return;

    playbackState_ = PlaybackState::Playing;

    int duration = 0;
    playingId_ = GetSoundsManager()->playPtt(getFileLocalPath(), playingId_, duration);

    Ui::GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::chatscr_playptt_action, { { "from_gallery", "no" },  { "chat_type", Ui::getStatsChatType() } });

    if (!playbackProgressAnimation_)
    {
        durationMSec_ = duration;
        assert(duration > 0);

        playbackProgressAnimation_ = new QPropertyAnimation(this, QByteArrayLiteral("PlaybackProgress"), this);
        playbackProgressAnimation_->setDuration(duration);
        playbackProgressAnimation_->setLoopCount(1);
        playbackProgressAnimation_->setStartValue(0);
        playbackProgressAnimation_->setEndValue(duration);
    }

    playbackProgressAnimation_->start();

    updateButtonsStates();
}

void PttBlock::pausePlayback()
{
    if (!isPlaying())
        return;

    playbackState_ = PlaybackState::Paused;

    assert(playingId_ > 0);
    GetSoundsManager()->pausePtt(playingId_);

    playbackProgressAnimation_->pause();

    updateButtonsStates();
}

void PttBlock::updateButtonsStates()
{
    updatePlayButtonState();
}

void PttBlock::updatePlayButtonState()
{
    assert(!getParentComplexMessage()->isHeadless());

    if (isPlaying())
        buttonPlay_->setState(PttDetails::PlayButton::ButtonState::pause);
    else
        buttonPlay_->setState(PttDetails::PlayButton::ButtonState::play);
}

void PttBlock::updateCollapseState()
{
    if (!isDecodedTextCollapsed_ && !buttonCollapse_)
    {
        buttonCollapse_ = new PttDetails::ButtonWithBackground(this, qsl(":/controls/top_icon"), getChatAimid());
        buttonCollapse_->setActive(isSelected());
        buttonCollapse_->setFixedSize(getButtonSize());
        buttonCollapse_->move(pttLayout_->getTextButtonRect().topLeft());

        connect(buttonCollapse_, &PttDetails::ButtonWithBackground::clicked, this, &PttBlock::onTextButtonClicked);
    }

    buttonCollapse_->setVisible(!isDecodedTextCollapsed_);
    buttonText_->setVisible(isDecodedTextCollapsed_);

    notifyBlockContentsChanged();
}

void PttBlock::updateStyle()
{
    if (buttonPlay_)
        buttonPlay_->updateStyle();

    if (buttonText_)
        buttonText_->updateStyle();

    if (buttonCollapse_)
        buttonCollapse_->updateStyle();

    if (progressText_)
        progressText_->updateStyle();

    if (progressPlay_)
        progressPlay_->updateStyle();

    updateDecodedTextStyle();

    update();
}

void PttBlock::updateFonts()
{
    updateDecodedTextStyle();
}

void PttBlock::updateDecodedTextStyle()
{
    if (decodedTextCtrl_)
    {
        decodedTextCtrl_->init(MessageStyle::getTextFont(), getDecodedTextColor(), MessageStyle::getLinkColor(), MessageStyle::getTextSelectionColor(getChatAimid()), MessageStyle::getHighlightColor());
        notifyBlockContentsChanged();
    }
}

void PttBlock::updateDecodedTextSelection(bool _isFullSelection)
{
    if (decodedTextCtrl_)
    {
        decodedTextCtrl_->setColor(getDecodedTextColor());
        decodedTextCtrl_->setSelectionColor(_isFullSelection ? QColor(Qt::transparent) : MessageStyle::getTextSelectionColor(getChatAimid()));
        notifyBlockContentsChanged();
    }
}

QColor PttBlock::getDecodedTextColor() const
{
    return Styling::getParameters(getChatAimid()).getColor(isSelected() ? Styling::StyleVariable::TEXT_SOLID_PERMANENT : Styling::StyleVariable::TEXT_SOLID);
}

QColor PttBlock::getProgressColor() const
{
    return Styling::getParameters(getChatAimid()).getColor(isSelected() ? Styling::StyleVariable::PRIMARY_BRIGHT : Styling::StyleVariable::GHOST_SECONDARY);
}

QColor PttBlock::getPlaybackColor() const
{
    return Styling::getParameters(getChatAimid()).getColor(isSelected() ? Styling::StyleVariable::TEXT_SOLID_PERMANENT : Styling::StyleVariable::PRIMARY);
}

bool PttBlock::isOutgoing() const
{
    return isOutgoing_;
}

void PttBlock::onPlayButtonClicked()
{
    emit Utils::InterConnector::instance().stopPttRecord();
    if (isPlaying())
    {
        pausePlayback();
        return;
    }

    isPlaybackScheduled_ = true;

    if (isFileDownloading())
        return;

    startDownloading(true);
}

void PttBlock::onTextButtonClicked()
{
    if (hasDecodedText())
    {
        isDecodedTextCollapsed_ = !isDecodedTextCollapsed_;

        updateCollapseState();
    }
    else
    {
        requestText();
    }

    MainPage::instance()->setFocusOnInput();
}

void PttBlock::onPttFinished(int _id, bool _byPlay)
{
    if (_id < 0 || _id != playingId_)
        return;

    playingId_ = -1;

    if (playbackProgressAnimation_)
    {
        playbackProgressAnimation_->stop();
        playbackProgressMsec_ = durationMSec_;
    }

    playbackState_ = PlaybackState::Stopped;

    updateButtonsStates();

    if (_byPlay)
        pttPlayed(id_);

    update();
}

void PttBlock::onPttPaused(int _id)
{
    if (_id < 0)
        return;

    if (_id == playingId_ && isPlaying())
        pausePlayback();
}

void PttBlock::onPttText(qint64 _seq, int _error, QString _text, int _comeback)
{
    assert(_seq > 0);

    if (textRequestId_ != _seq)
    {
        return;
    }

    textRequestId_  = -1;

    const auto isError = (_error != 0);
    const auto isComeback = (_comeback > 0);

    if (isComeback)
    {
        const auto retryTimeoutMsec = ((_comeback + 1) * 1000);

        QTimer::singleShot(
            retryTimeoutMsec,
            Qt::VeryCoarseTimer,
            this,
            &PttBlock::requestText);

        return;
    }

    decodedText_ = isError ? QT_TRANSLATE_NOOP("ptt_widget", "unclear message") : _text;

    Ui::GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::chatscr_recognptt_action, {
        { "from_gallery", "no" },
        { "chat_type", Ui::getStatsChatType() },
        { "result", isError ? "fail" : "success" } });

    stopTextRequestProgressAnimation();

    initializeDecodedTextCtrl();

    GetDispatcher()->setUrlPlayed(getLink(), true);

    isDecodedTextCollapsed_ = false;
    updateCollapseState();
}

void PttBlock::pttPlayed(qint64 id)
{
    if (id != prevId_ || getChatAimid() != Logic::getContactListModel()->selectedContact())
        return;

    if (!isPlayed_ && !isPlaying())
    {
        isPlaybackScheduled_ = true;

        if (isFileDownloading())
        {
            return;
        }

        startDownloading(true);
    }
}

int PttBlock::desiredWidth(int) const
{
    return MessageStyle::Ptt::getPttBlockMaxWidth() + (isStandalone() ? 0 : 2 * MessageStyle::getBubbleHorPadding());
}

bool PttBlock::isNeedCheckTimeShift() const
{
    if (isStandalone() && !isDecodedTextCollapsed() && decodedTextCtrl_)
    {
        if (const auto cm = getParentComplexMessage())
        {
            if (const auto timeWidget = cm->getTimeWidget())
            {
                const auto timeWidth = timeWidget->width() + MessageStyle::getTimeLeftSpacing();
                const auto lastLineW = decodedTextCtrl_->getLastLineWidth();

                return width() - lastLineW < timeWidth;
            }
        }
    }

    return false;
}

void PttBlock::setSelected(const bool _isSelected)
{
    if (buttonPlay_)
        buttonPlay_->setSelected(_isSelected);

    if (buttonText_)
        buttonText_->setActive(_isSelected);

    if (buttonCollapse_)
        buttonCollapse_->setActive(_isSelected);

    if (decodedTextCtrl_)
        decodedTextCtrl_->setColor(getDecodedTextColor());

    if (progressText_)
        progressText_->setSelected(_isSelected);

    if (progressPlay_)
        progressPlay_->setSelected(_isSelected);

    FileSharingBlockBase::setSelected(_isSelected);
}

bool PttBlock::clicked(const QPoint& _p)
{
    if (decodedTextCtrl_ && !isDecodedTextCollapsed_)
    {
        QPoint mappedPoint = mapFromParent(_p, pttLayout_->getBlockGeometry());
        decodedTextCtrl_->clicked(mappedPoint);
    }

    return true;
}

void PttBlock::doubleClicked(const QPoint& _p, std::function<void(bool)> _callback)
{
    if (decodedTextCtrl_ && !isDecodedTextCollapsed_)
    {
        QPoint mappedPoint = mapFromParent(_p, pttLayout_->getBlockGeometry());
        const auto textArea = QRect(decodedTextCtrl_->offsets(), rect().bottomRight());
        if (textArea.contains(mappedPoint))
        {
            decodedTextCtrl_->doubleClicked(mappedPoint, true, _callback);
            update();
            return;
        }
    }

    if (_callback)
        _callback(true);
}

void PttBlock::releaseSelection()
{
    if (decodedTextCtrl_)
    {
        decodedTextCtrl_->releaseSelection();
        //updateDecodedTextSelection(isSelected());
    }
    update();
}

void PttBlock::onVisibilityChanged(const bool isVisible)
{
    if (!isVisible)
        pausePlayback();
}

UI_COMPLEX_MESSAGE_NS_END
