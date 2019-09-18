#include "stdafx.h"

#include "exif.h"
#include "utils.h"
#include "features.h"

#include "gui_coll_helper.h"
#include "InterConnector.h"
#include "SChar.h"
#include "profiling/auto_stop_watch.h"
#include "translit.h"
#include "../gui_settings.h"
#include "../core_dispatcher.h"
#include "../cache/countries.h"
#include "../cache/emoji/Emoji.h"
#include "../cache/stickers/stickers.h"

#include "../controls/DpiAwareImage.h"
#include "../controls/GeneralDialog.h"
#include "../controls/TextEditEx.h"
#include "../controls/TextUnit.h"
#include "../main_window/ContactDialog.h"
#include "../main_window/MainPage.h"
#include "../main_window/MainWindow.h"
#include "../main_window/contact_list/Common.h"
#include "../main_window/contact_list/ContactListModel.h"
#include "../main_window/contact_list/RecentsModel.h"
#include "../main_window/contact_list/UnknownsModel.h"
#include "../main_window/history_control/MessageStyle.h"
#include "../main_window/friendly/FriendlyContainer.h"
#include "../styles/ThemesContainer.h"
#include "../styles/ThemeParameters.h"
#include "../../common.shared/version_info.h"
#include "UrlCommand.h"
#include "log/log.h"
#include "../app_config.h"

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include "macos/mac_support.h"
    typedef unsigned char byte;
#elif defined(__linux__)
    #define byte uint8_t
#endif

Q_LOGGING_CATEGORY(linuxRunCommand, "linuxRunCommand")
Q_LOGGING_CATEGORY(icons, "icons")

namespace
{
    const int mobile_rect_width = 8;
    const int mobile_rect_height = 12;

    const int mobile_rect_width_mini = 5;
    const int mobile_rect_height_mini = 8;

    const int drag_preview_max_width = 320;
    const int drag_preview_max_height = 240;

    const QColor colorTable3[][3] =
    {
#ifdef __APPLE__
        { { 0xFB, 0xE8, 0x9F }, { 0xF1, 0xA1, 0x6A }, { 0xE9, 0x99, 0x36 } },
        { { 0xFB, 0xE7, 0xB0 }, { 0xE9, 0x62, 0x64 }, { 0xEF, 0x8B, 0x70 } },
        { { 0xFF, 0xBB, 0xAD }, { 0xD9, 0x3B, 0x50 }, { 0xED, 0x61, 0x5B } },
        { { 0xDB, 0xED, 0xB5 }, { 0x48, 0xAB, 0x68 }, { 0x3C, 0x9E, 0x55 } },
        { { 0xD4, 0xC7, 0xFC }, { 0x9E, 0x60, 0xC3 }, { 0xB5, 0x7F, 0xDB } },
        { { 0xE3, 0xAA, 0xEE }, { 0xCB, 0x55, 0x9C }, { 0xE1, 0x7C, 0xD0 } },
        { { 0xFE, 0xC8, 0x94 }, { 0xD3, 0x4F, 0xB6 }, { 0xEF, 0x72, 0x86 } },
        { { 0x95, 0xDF, 0xF7 }, { 0x8C, 0x75, 0xD0 }, { 0x68, 0x86, 0xE6 } },
        { { 0xB6, 0xEE, 0xFF }, { 0x1A, 0xB1, 0xD1 }, { 0x21, 0xA8, 0xCC } },
        { { 0x7F, 0xED, 0x9B }, { 0x3D, 0xB7, 0xC0 }, { 0x13, 0xA9, 0x8A } },
        { { 0xE2, 0xF4, 0x96 }, { 0x69, 0xBA, 0x3C }, { 0x6C, 0xCA, 0x30 } }
#else
        { { 0xFF, 0xEA, 0x90 }, { 0xFF, 0x9E, 0x5C }, { 0xFF, 0x9B, 0x20 } },
        { { 0xFF, 0xEA, 0xA6 }, { 0xFF, 0x5C, 0x60 }, { 0xFF, 0x82, 0x60 } },
        { { 0xFF, 0xBE, 0xAE }, { 0xF2, 0x33, 0x4C }, { 0xFE, 0x51, 0x4A } },
        { { 0xD8, 0xEF, 0xAB }, { 0x37, 0xB0, 0x5F }, { 0x26, 0xB4, 0x4A } },
        { { 0xD5, 0xC8, 0xFD }, { 0xA7, 0x5A, 0xD3 }, { 0xA3, 0x54, 0xDB } },
        { { 0xEB, 0xA6, 0xF8 }, { 0xDC, 0x4D, 0xA2 }, { 0xE1, 0x4E, 0xC8 } },
        { { 0xFF, 0xCA, 0x94 }, { 0xE6, 0x45, 0xC4 }, { 0xFF, 0x62, 0x7B } },
        { { 0x88, 0xE3, 0xFF }, { 0x81, 0x60, 0xDE }, { 0x56, 0x7D, 0xF8 } },
        { { 0xB6, 0xEE, 0xFF }, { 0x1A, 0xB1, 0xD1 }, { 0x0A, 0xB6, 0xE3 } },
        { { 0x70, 0xF2, 0x90 }, { 0x27, 0xBC, 0xC8 }, { 0x00, 0xBC, 0x95 } },
        { { 0xE4, 0xF5, 0x98 }, { 0x5B, 0xBD, 0x25 }, { 0x53, 0xCA, 0x07 } }
#endif
    };
    const auto colorTableSize = sizeof(colorTable3)/sizeof(colorTable3[0]);

    const quint32 CRC32Table[ 256 ] =
    {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
        0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
        0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
        0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
        0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
        0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
        0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
        0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
        0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
        0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
        0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
        0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
        0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
        0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
        0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
        0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
        0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
        0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
        0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
        0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
        0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    quint32 crc32FromIODevice(QIODevice* _device)
    {
        quint32 crc32 = 0xffffffff;
        char* buf = new char[256];
        qint64 n;

        while((n = _device->read(buf, 256)) > 0)
            for (qint64 i = 0; i < n; i++)
                crc32 = (crc32 >> 8) ^ CRC32Table[(crc32 ^ buf[i]) & 0xff];
        delete[] buf;

        crc32 ^= 0xffffffff;
        return crc32;
    }

    quint32 crc32FromByteArray(const QByteArray& _array)
    {
        QBuffer buffer;
        buffer.setData(_array);
        if (!buffer.open(QIODevice::ReadOnly))
            return 0;

        return crc32FromIODevice(&buffer);
    }

    quint32 crc32FromString(const QString& _text)
    {
        return crc32FromByteArray(_text.toLatin1());
    }

#ifdef _WIN32
    QChar VKeyToChar( short _vkey, HKL _layout)
    {
        DWORD dwScan=MapVirtualKeyEx(_vkey & 0x00ff, 0, _layout);
        byte KeyStates[256] = {0};
        KeyStates[_vkey & 0x00ff] = 0x80;
        DWORD dwFlag=0;
        if(_vkey & 0x0100)
        {
            KeyStates[VK_SHIFT] = 0x80;
        }
        if(_vkey & 0x0200)
        {
            KeyStates[VK_CONTROL] = 0x80;
        }
        if(_vkey & 0x0400)
        {
            KeyStates[VK_MENU] = 0x80;
            dwFlag = 1;
        }
        wchar_t Result = L' ';
        if (!ToUnicodeEx(_vkey & 0x00ff, dwScan, KeyStates, &Result, 1, dwFlag, _layout) == 1)
        {
            return ql1c(' ');
        }
        return QChar(Result);
    }
#endif //_WIN32

    QString msgIdFromUidl(const QString& uidl)
    {
        if (uidl.size() != 16)
        {
            assert(!"wrong uidl");
            return QString();
        }

        byte b[8] = {0};
        bool ok;
        b[0] = uidl.midRef(0, 2).toUInt(&ok, 16);
        b[1] = uidl.midRef(2, 2).toUInt(&ok, 16);
        b[2] = uidl.midRef(4, 2).toUInt(&ok, 16);
        b[3] = uidl.midRef(6, 2).toUInt(&ok, 16);
        b[4] = uidl.midRef(8, 2).toUInt(&ok, 16);
        b[5] = uidl.midRef(10, 2).toUInt(&ok, 16);
        b[6] = uidl.midRef(12, 2).toUInt(&ok, 16);
        b[7] = uidl.midRef(14, 2).toUInt(&ok, 16);

        return QString::asprintf("%u%010u", *(int*)(b), *(int*)(b + 4*sizeof(byte)));
    }

    const QString redirect = qsl("&noredirecttologin=1");

    const QString& getPage()
    {
        static const QString r_mail_ru = QString::fromStdString(Ui::GetAppConfig().getUrlRMailRu()).trimmed();
        static const QString page = qsl("https://") % (r_mail_ru.isEmpty() ? QString() : r_mail_ru  % qsl(":443/cls3564/")) % QString::fromStdString(Ui::GetAppConfig().getUrlWinMailRu());

        return page;
    }

    const QString& getFailPage()
    {
        static const QString r_mail_ru = QString::fromStdString(Ui::GetAppConfig().getUrlRMailRu()).trimmed();
        static const QString page = qsl("https://") % (r_mail_ru.isEmpty() ? QString() : r_mail_ru % qsl(":443/cls3564/")) % QString::fromStdString(Ui::GetAppConfig().getUrlWinMailRu());

        return page;
    }

    const QString& getReadMsgPage()
    {
        static const QString r_mail_ru = QString::fromStdString(Ui::GetAppConfig().getUrlRMailRu()).trimmed();
        static const QString page = qsl("https://") % (r_mail_ru.isEmpty() ? QString() : r_mail_ru % qsl(":443/cln8791/")) % QString::fromStdString(Ui::GetAppConfig().getUrlReadMsg()) % qsl("?id=");

        return page;
    }

    const QString& getBaseMailUrl()
    {
        static const QString base_mail_url = qsl("https://") % QString::fromStdString(Ui::GetAppConfig().getUrlAuthMailRu()) % qsl("?Login=%1&agent=%2&ver=%3&agentlang=%4");

        return base_mail_url;
    }

    const QString& getMailUrl()
    {
        static const QString mail_url = getBaseMailUrl() % redirect % ql1s("&page=") % getPage() % ql1s("&FailPage=") % getFailPage();

        return mail_url;
    }

    const QString& getMailOpenUrl()
    {
        static const QString mail_open_url = getBaseMailUrl() % redirect % ql1s("&page=") % getReadMsgPage() % ql1s("%5&lang=%4") % ql1s("&FailPage=") % getReadMsgPage() % ql1s("%5&lang=%4");

        return mail_open_url;
    }




    QString getCpuId()
    {
#if defined(_WIN32)
        std::array<int, 4> cpui;
        std::vector<std::array<int, 4>> extdata;

        __cpuid(cpui.data(), 0x80000000);

        int nExIds = cpui[0];
        for (int i = 0x80000000; i <= nExIds; ++i)
        {
            __cpuidex(cpui.data(), i, 0);
            extdata.push_back(cpui);
        }

        char brand[0x40];
        memset(brand, 0, sizeof(brand));

        // Interpret CPU brand string if reported
        if (nExIds >= 0x80000004)
        {
            memcpy(brand, extdata[2].data(), sizeof(cpui));
            memcpy(brand + 16, extdata[3].data(), sizeof(cpui));
            memcpy(brand + 32, extdata[4].data(), sizeof(cpui));
            return QString::fromLatin1(brand);
        }
        return QString();
#elif defined(__APPLE__)
        const auto BUFFERLEN = 128;
        char buffer[BUFFERLEN];
        size_t bufferlen = BUFFERLEN;
        sysctlbyname("machdep.cpu.brand_string", &buffer, &bufferlen, NULL, 0);
        return QString::fromLatin1(buffer);
#elif defined(__linux__)
        std::string line;
        std::ifstream finfo("/proc/cpuinfo");
        while (std::getline(finfo, line))
        {
            std::stringstream str(line);
            std::string itype;
            std::string info;
            if (std::getline(str, itype, ':') && std::getline(str, info) && itype.substr(0, 10) == "model name")
                return QString::fromStdString(info);
        }
        return QString();
#endif
    }

    unsigned int getColorTableIndex(const QString& _uin)
    {
        const auto &str = _uin.isEmpty() ? qsl("0") : _uin;
        const auto crc32 = crc32FromString(str);
        return (crc32 % colorTableSize);
    }

    struct AvatarBadge
    {
        QSize size_;
        QPoint offset_;
        QPixmap officialIcon_;
        QPixmap muteIcon_;
        int cutWidth_;

        AvatarBadge(const int _size, const int _offsetX, const int _offsetY, const int _cutWidth, const QString& _officialPath, const QString& _mutedPath)
            : size_(Utils::scale_value(QSize(_size, _size)))
            , offset_(Utils::scale_value(_offsetX), Utils::scale_value(_offsetY))
            , cutWidth_(Utils::scale_value(_cutWidth))
        {
            if (!_officialPath.isEmpty())
                officialIcon_ = Utils::renderSvg(_officialPath, size_);

            if (!_mutedPath.isEmpty())
                muteIcon_ = Utils::renderSvg(_mutedPath, size_);
        }
    };

    using badgeDataMap = std::map<int, AvatarBadge>;
    const badgeDataMap& getBadgesData()
    {
        static const badgeDataMap badges =
        {
            { Utils::scale_bitmap_with_value(32),  AvatarBadge(12, 22 , 20 , 2, qsl(":/avatar_badges/official_32"), qsl(":/avatar_badges/mute_32")) },
            { Utils::scale_bitmap_with_value(44),  AvatarBadge(14, 30 , 30 , 2, qsl(":/avatar_badges/official_44"), QString()) },
            { Utils::scale_bitmap_with_value(52),  AvatarBadge(16, 35 , 35 , 2, qsl(":/avatar_badges/official_52"), qsl(":/avatar_badges/mute_52")) },
            { Utils::scale_bitmap_with_value(60),  AvatarBadge(20, 42 , 40 , 4, qsl(":/avatar_badges/official_60"), QString()) },
            { Utils::scale_bitmap_with_value(80),  AvatarBadge(20, 57 , 57 , 2, qsl(":/avatar_badges/official_60"), QString()) },
            { Utils::scale_bitmap_with_value(138), AvatarBadge(24, 106, 106, 2, qsl(":/avatar_badges/official_138"),QString()) },
        };
        return badges;
    };
}

namespace Utils
{
    QString getAppTitle()
    {
        return build::GetProductVariant(QT_TRANSLATE_NOOP("title", "ICQ"),
                                        QT_TRANSLATE_NOOP("title", "Mail.ru Agent"),
                                        QT_TRANSLATE_NOOP("title", "Myteam"),
                                        QT_TRANSLATE_NOOP("title", "Messenger"));
    }

    QString getVersionLabel()
    {
        core::tools::version_info infoCurrent;
        return getAppTitle() % ql1s(" (") % QString::fromUtf8(infoCurrent.get_version().c_str()) % ql1c(')');
    }

    QString getVersionPrintable()
    {
        core::tools::version_info infoCurrent;
        return getAppTitle() % ql1c(' ') % QString::fromUtf8(infoCurrent.get_version().c_str());
    }

    void drawText(QPainter& _painter, const QPointF& _point, int _flags,
        const QString& _text, QRectF* _boundingRect)
    {
        const qreal size = 32767.0;
        QPointF corner(_point.x(), _point.y() - size);

        if (_flags & Qt::AlignHCenter)
            corner.rx() -= size/2.0;
        else if (_flags & Qt::AlignRight)
            corner.rx() -= size;

        if (_flags & Qt::AlignVCenter)
            corner.ry() += size/2.0;
        else if (_flags & Qt::AlignTop)
            corner.ry() += size;
        else _flags |= Qt::AlignBottom;

        QRectF rect(corner, QSizeF(size, size));
        _painter.drawText(rect, _flags, _text, _boundingRect);
    }

    ShadowWidgetEventFilter::ShadowWidgetEventFilter(int _shadowWidth)
        : QObject(nullptr)
        , ShadowWidth_(_shadowWidth)
    {
    }

    bool ShadowWidgetEventFilter::eventFilter(QObject* obj, QEvent* _event)
    {
        if (_event->type() == QEvent::Paint)
        {
            QWidget* w = qobject_cast<QWidget*>(obj);

            QRect origin = w->rect();

            QRect right = QRect(
                QPoint(origin.width() - ShadowWidth_, origin.y() + ShadowWidth_ + 1),
                QPoint(origin.width(), origin.height() - ShadowWidth_ - 1)
            );

            QRect left = QRect(
                QPoint(origin.x(), origin.y() + ShadowWidth_),
                QPoint(origin.x() + ShadowWidth_ - 1, origin.height() - ShadowWidth_ - 1)
            );

            QRect top = QRect(
                QPoint(origin.x() + ShadowWidth_ + 1, origin.y()),
                QPoint(origin.width() - ShadowWidth_ - 1, origin.y() + ShadowWidth_)
            );

            QRect bottom = QRect(
                QPoint(origin.x() + ShadowWidth_ + 1, origin.height() - ShadowWidth_),
                QPoint(origin.width() - ShadowWidth_ - 1, origin.height())
            );

            QRect topLeft = QRect(
                origin.topLeft(),
                QPoint(origin.x() + ShadowWidth_, origin.y() + ShadowWidth_)
            );

            QRect topRight = QRect(
                QPoint(origin.width() - ShadowWidth_, origin.y()),
                QPoint(origin.width(), origin.y() + ShadowWidth_)
            );

            QRect bottomLeft = QRect(
                QPoint(origin.x(), origin.height() - ShadowWidth_),
                QPoint(origin.x() + ShadowWidth_, origin.height())
            );

            QRect bottomRight = QRect(
                QPoint(origin.width() - ShadowWidth_, origin.height() - ShadowWidth_),
                origin.bottomRight()
            );

            QPainter p(w);

            bool isActive = w->isActiveWindow();

            QLinearGradient lg = QLinearGradient(right.topLeft(), right.topRight());
            setGradientColor(lg, isActive);
            p.fillRect(right, QBrush(lg));

            lg = QLinearGradient(left.topRight(), left.topLeft());
            setGradientColor(lg, isActive);
            p.fillRect(left, QBrush(lg));

            lg = QLinearGradient(top.bottomLeft(), top.topLeft());
            setGradientColor(lg, isActive);
            p.fillRect(top, QBrush(lg));

            lg = QLinearGradient(bottom.topLeft(), bottom.bottomLeft());
            setGradientColor(lg, isActive);
            p.fillRect(bottom, QBrush(lg));

            QRadialGradient g = QRadialGradient(topLeft.bottomRight(), ShadowWidth_);
            setGradientColor(g, isActive);
            p.fillRect(topLeft, QBrush(g));

            g = QRadialGradient(topRight.bottomLeft(), ShadowWidth_);
            setGradientColor(g, isActive);
            p.fillRect(topRight, QBrush(g));

            g = QRadialGradient(bottomLeft.topRight(), ShadowWidth_);
            setGradientColor(g, isActive);
            p.fillRect(bottomLeft, QBrush(g));

            g = QRadialGradient(bottomRight.topLeft(), ShadowWidth_);
            setGradientColor(g, isActive);
            p.fillRect(bottomRight, QBrush(g));
        }

        return QObject::eventFilter(obj, _event);
    }

    void ShadowWidgetEventFilter::setGradientColor(QGradient& _gradient, bool _isActive)
    {
        QColor windowGradientColor(ql1s("#000000"));
        windowGradientColor.setAlphaF(0.2);
        _gradient.setColorAt(0, windowGradientColor);
        windowGradientColor.setAlphaF(_isActive ? 0.08 : 0.04);
        _gradient.setColorAt(0.2, windowGradientColor);
        windowGradientColor.setAlphaF(0.02);
        _gradient.setColorAt(0.6, _isActive ? windowGradientColor : Qt::transparent);
        _gradient.setColorAt(1, Qt::transparent);
    }

    bool isUin(const QString& _aimId)
    {
        return !_aimId.isEmpty() && std::all_of(_aimId.begin(), _aimId.end(), [](auto c) { return c.isDigit(); });
    }

    QString getCountryNameByCode(const QString& _isoCode)
    {
        const auto &countries = Ui::countries::get();
        const auto i = std::find_if(
            countries.begin(), countries.end(), [_isoCode](const Ui::countries::country &v)
        {
            return v.iso_code_.toLower() == _isoCode.toLower();
        });
        if (i != countries.end())
            return i->name_;
        return QString();
    }

    QMap<QString, QString> getCountryCodes()
    {
        QMap<QString, QString> result;
        for (const auto& country : Ui::countries::get())
            result.insert(country.name_, ql1c('+') % QString::number(country.phone_code_));
        return result;
    }

    QString ScaleStyle(const QString& _style, double _scale)
    {
        QString outString;
        QTextStream result(&outString);

        static const QRegularExpression semicolon(
            ql1s("\\;"),
            QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::OptimizeOnFirstUsageOption
        );
        static const QRegularExpression dip(
            ql1s("[\\-\\d]\\d*dip"),
            QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::OptimizeOnFirstUsageOption
        );

        const auto tokens = _style.split(semicolon);

        for (const auto& line : tokens)
        {
            int pos = line.indexOf(dip);

            if (pos != -1)
            {
                result << line.leftRef(pos);
                const auto tmp = line.midRef(pos, line.rightRef(pos).size());
                const int size = tmp.left(tmp.indexOf(ql1s("dip"))).toInt() * _scale;
                result << size;
                result << ql1s("px");
            }
            else
            {
                const double currentScale = _scale >= 2.0 ? 2.0 : _scale;

                const auto scalePostfix = ql1s("_100");
                pos = line.indexOf(scalePostfix);
                if (pos != -1)
                {
                    result << line.leftRef(pos);
                    result << ql1c('_');
                    result << (Utils::is_mac_retina() ? 2 : currentScale) * 100;
                    result << line.midRef(pos + scalePostfix.size(), line.size());
                }
                else
                {
                    const auto scaleDir = ql1s("/100/");
                    pos = line.indexOf(scaleDir);
                    if (pos != -1)
                    {
                        result << line.leftRef(pos);
                        result << ql1c('/');
                        result << Utils::scale_bitmap(currentScale) * 100;
                        result << ql1c('/');
                        result << line.midRef(pos + scaleDir.size(), line.size());
                    }
                    else
                    {
                        result << line;
                    }
                }
            }
            result << ql1c(';');
        }
        if (!outString.isEmpty())
            outString.chop(1);

        return outString;
    }

    void ApplyPropertyParameter(QWidget* _widget, const char* _property, QVariant _parameter)
    {
        if (_widget)
        {
            _widget->setProperty(_property, _parameter);
            _widget->style()->unpolish(_widget);
            _widget->style()->polish(_widget);
            _widget->update();
        }
    }

    void ApplyStyle(QWidget* _widget, const QString& _style)
    {
        if (_widget)
        {
            const QString newStyle = Fonts::SetFont(Utils::ScaleStyle(_style, Utils::getScaleCoefficient()));
            if (newStyle != _widget->styleSheet())
                _widget->setStyleSheet(newStyle);
        }
    }

    QString LoadStyle(const QString& _qssFile)
    {
        QFile file(_qssFile);

        if (!file.open(QIODevice::ReadOnly))
        {
            assert(!"open style file error");
            return QString();
        }

        QString qss = QString::fromUtf8(file.readAll());
        if (qss.isEmpty())
        {
            return QString();
        }

        return ScaleStyle(Fonts::SetFont(std::move(qss)), Utils::getScaleCoefficient());
    }

    // COLOR
    QPixmap getDefaultAvatar(const QString& _uin, const QString& _displayName, const int _sizePx)
    {
        const auto antialiasSizeMult = 8;
        const auto bigSizePx = (_sizePx * antialiasSizeMult);
        const auto bigSize = QSize(bigSizePx, bigSizePx);
        const auto isMailIcon = _uin == ql1s("mail");

        QImage bigResult(bigSize, QImage::Format_ARGB32);

        // evaluate colors
        const auto colorIndex = getColorTableIndex(_uin);
        QColor color1 = colorTable3[colorIndex][0];
        QColor color2 = colorTable3[colorIndex][1];

        if (_uin.isEmpty() && _displayName.isEmpty())
        {
            color1 = Styling::getParameters().getColor(Styling::StyleVariable::BASE_BRIGHT_INVERSE);
            color2 = color1;
        }

        if (isMailIcon)
        {
            color1 = Styling::getParameters().getColor(Styling::StyleVariable::PRIMARY);
            color2 = color1;
        }

        QPainter painter(&bigResult);

        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        // set pen

        QPen hollowAvatarPen(1);

        const auto hollowPenWidth = scale_value(2 * antialiasSizeMult);
        hollowAvatarPen.setWidth(hollowPenWidth);

        QPen filledAvatarPen(Qt::white);

        const QPen &avatarPen = isMailIcon ? hollowAvatarPen : filledAvatarPen;
        painter.setPen(avatarPen);

        // draw

        if (isMailIcon)
        {
            bigResult.fill(Qt::transparent);
            painter.setBrush(Styling::getParameters().getColor(Styling::StyleVariable::BASE_SECONDARY));

            const auto correction = ((hollowPenWidth / 2) + 1);
            const auto ellipseRadius = (bigSizePx - (correction * 2));
            painter.drawEllipse(correction, correction, ellipseRadius, ellipseRadius);
        }
        else
        {
            painter.setPen(Qt::NoPen);

            QLinearGradient radialGrad(QPointF(0, 0), QPointF(bigSizePx, bigSizePx));
            radialGrad.setColorAt(0, color1);
            radialGrad.setColorAt(1, color2);

            painter.fillRect(QRect(0, 0, bigSizePx, bigSizePx), radialGrad);
        }

        if (const auto overlay = Styling::getParameters().getColor(Styling::StyleVariable::GHOST_OVERLAY); overlay.isValid() && overlay.alpha() > 0)
        {
            Utils::PainterSaver ps(painter);
            painter.setCompositionMode(QPainter::CompositionMode_Overlay);
            painter.fillRect(QRect(0, 0, bigSizePx, bigSizePx), overlay);
        }

        auto scaledBigResult = bigResult.scaled(QSize(_sizePx, _sizePx), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        if (isMailIcon)
        {
            QPainter scaled(&scaledBigResult);
            scaled.setRenderHint(QPainter::Antialiasing);
            scaled.setRenderHint(QPainter::TextAntialiasing);
            scaled.setRenderHint(QPainter::SmoothPixmapTransform);
            scaled.drawPixmap(0, 0, renderSvg(qsl(":/mail_icon"), QSize(_sizePx, _sizePx)));
            return QPixmap::fromImage(std::move(scaledBigResult));
        }

        const auto trimmedDisplayName = QStringRef(&_displayName).trimmed();
        if (trimmedDisplayName.isEmpty())
        {
            QPainter scaled(&scaledBigResult);
            scaled.setRenderHint(QPainter::Antialiasing);
            scaled.setRenderHint(QPainter::TextAntialiasing);
            scaled.setRenderHint(QPainter::SmoothPixmapTransform);
            static const QPixmap img(Utils::loadPixmap(qsl(":/resources/photo_100.png")));
            const auto off = scaledBigResult.width() * 0.25;
            scaled.drawPixmap(QRectF(off, off, scaledBigResult.width() - off * 2., scaledBigResult.height() - off * 2.), img, img.rect());
            return QPixmap::fromImage(std::move(scaledBigResult));
        }

        const auto percent = _sizePx / scale_bitmap_ratio() <= Ui::MessageStyle::getLastReadAvatarSize() ? 0.64 : 0.54;
        const auto fontSize = _sizePx * percent;

        auto pos = 0;
        if (const auto startEmoji = Emoji::getEmoji(trimmedDisplayName, pos); startEmoji != Emoji::EmojiCode())
        {
            QPainter emojiPainter(&scaledBigResult);
            auto nearestSizeAvailable = _sizePx / 2 * scale_bitmap_ratio();
            const auto emoji = Ui::DpiAwareImage(Emoji::GetEmoji(startEmoji, nearestSizeAvailable));
            emoji.draw(emojiPainter, (_sizePx - emoji.width()) / 2, (_sizePx - emoji.height()) / 2 - (platform::is_apple() ? 0 : 1));
        }
        else
        {
            QFont font = Fonts::appFont(fontSize, Fonts::FontFamily::ROUNDED_MPLUS);

            QPainter letterPainter(&scaledBigResult);
            letterPainter.setRenderHint(QPainter::Antialiasing);
            letterPainter.setFont(font);
            letterPainter.setPen(avatarPen);

            const auto toDisplay = trimmedDisplayName.at(0).toUpper();

            auto rawFont = QRawFont::fromFont(font);
            if constexpr (platform::is_apple())
            {
                if (!rawFont.supportsCharacter(toDisplay))
                    rawFont = QRawFont::fromFont(QFont(qsl("AppleColorEmoji"), fontSize));
            }

            quint32 glyphIndex = 0;
            int numGlyphs = 1;
            const auto glyphSearchSucceed = rawFont.glyphIndexesForChars(&toDisplay, 1, &glyphIndex, &numGlyphs);
            if (!glyphSearchSucceed)
                return QPixmap::fromImage(std::move(scaledBigResult));

            assert(numGlyphs == 1);

            auto glyphPath = rawFont.pathForGlyph(glyphIndex);

            const auto rawGlyphBounds = glyphPath.boundingRect();
            glyphPath.translate(-rawGlyphBounds.x(), -rawGlyphBounds.y());
            const auto glyphBounds = glyphPath.boundingRect();
            const auto glyphHeight = glyphBounds.height();
            const auto glyphWidth = glyphBounds.width();

            QFontMetrics m(font);
            QRect pseudoRect(0, 0, _sizePx, _sizePx);
            auto centeredRect = m.boundingRect(pseudoRect, Qt::AlignCenter, QString(toDisplay));

            qreal y = (_sizePx / 2.0);
            y -= glyphHeight / 2.0;

            qreal x = centeredRect.x();
            x += (centeredRect.width() / 2.0);
            x -= (glyphWidth / 2.0);

            letterPainter.translate(x, y);
            letterPainter.fillPath(glyphPath, avatarPen.brush());
        }
        return QPixmap::fromImage(std::move(scaledBigResult));
    }

    void updateBgColor(QWidget* _widget, const QColor& _color)
    {
        assert(_widget);
        assert(_color.isValid());
        if (_widget)
        {
            assert(_widget->autoFillBackground());

            auto pal = _widget->palette();
            pal.setColor(QPalette::Background, _color);
            _widget->setPalette(pal);
            _widget->update();
        }
    }

    void setDefaultBackground(QWidget* _widget)
    {
        assert(_widget);
        if (_widget)
        {
            _widget->setAutoFillBackground(true);
            updateBgColor(_widget, Styling::getParameters().getColor(Styling::StyleVariable::BASE_GLOBALWHITE));
        }
    }

    void drawBackgroundWithBorders(QPainter& _p, const QRect& _rect, const QColor& _bg, const QColor& _border, const Qt::Alignment _borderSides, const int _borderWidth)
    {
        Utils::PainterSaver ps(_p);
        _p.setPen(Qt::NoPen);

        if (_bg.isValid())
            _p.fillRect(_rect, _bg);

        if (_border.isValid())
        {
            const QRect borderHor(0, 0, _rect.width(), _borderWidth);
            if (_borderSides & Qt::AlignTop)
                _p.fillRect(borderHor, _border);
            if (_borderSides & Qt::AlignBottom)
                _p.fillRect(borderHor.translated(0, _rect.height() - borderHor.height()), _border);

            const QRect borderVer(0, 0, _borderWidth, _rect.height());
            if (_borderSides & Qt::AlignLeft)
                _p.fillRect(borderVer, _border);
            if (_borderSides & Qt::AlignRight)
                _p.fillRect(borderVer.translated(_rect.width() - borderVer.width(), 0), _border);
        }
    }

    QColor getNameColor(const QString& _uin)
    {
        assert(!_uin.isEmpty());
        if (!_uin.isEmpty())
        {
            const auto colorIndex = getColorTableIndex(_uin);
            return colorTable3[colorIndex][2];
        }

        return Styling::getParameters().getColor(Styling::StyleVariable::BASE_PRIMARY);
    }

    std::vector<std::vector<QString>> GetPossibleStrings(const QString& _text, unsigned& _count)
    {
        _count = 0;
        std::vector<std::vector<QString>> result;
        result.reserve(_text.length());

        if (_text.isEmpty())
            return result;

#if defined(__linux__) || defined(_WIN32)
        for (int i = 0; i < _text.length(); ++i)
            result.push_back({ _text.at(i) });
#endif

#if defined _WIN32
        HKL aLayouts[8] = {0};
        HKL hCurrent = ::GetKeyboardLayout(0);
        _count = ::GetKeyboardLayoutList(8, aLayouts);

        for (int i = 0; i < _text.length(); i++)
        {
            const auto ucChar = _text.at(i).unicode();
            const auto scanEx = ::VkKeyScanEx(ucChar, hCurrent);

            if (scanEx == -1)
            {
                const auto found = std::any_of(std::cbegin(aLayouts), std::cend(aLayouts), [ucChar](auto layout) { return ::VkKeyScanEx(ucChar, layout); });

                if (!found)
                    return std::vector<std::vector<QString>>();
            }

            for (unsigned j = 0; j < _count; ++j)
            {
                if (hCurrent == aLayouts[j])
                    continue;

                if (scanEx != -1)
                    result[i].push_back(VKeyToChar(scanEx, aLayouts[j]));
            }
        }

#elif defined __APPLE__
        MacSupport::getPossibleStrings(_text, result, _count);
#elif defined __linux__
        _count = 1;
#endif

        const auto translit = Translit::getPossibleStrings(_text);

        if (!translit.empty())
        {
            for (int i = 0; i < std::min(Translit::getMaxSearchTextLength(), _text.length()); ++i)
            {
                for (const auto& pattern : translit[i])
                    result[i].push_back(pattern);
            }
        }

        for (auto& res : result)
            res.erase(std::remove_if(res.begin(), res.end(), [](const auto& _p) { return _p.isEmpty(); }), res.end());
        result.erase(std::remove_if(result.begin(), result.end(), [](const auto& _v) { return _v.empty(); }), result.end());

        return result;
    }

    QPixmap roundImage(const QPixmap& _img, const QString& _state, bool /*isDefault*/, bool _miniIcons)
    {
        const int scale = std::min(_img.height(), _img.width());
        QPixmap imageOut(scale, scale);
        imageOut.fill(Qt::transparent);

        QPainter painter(&imageOut);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::NoBrush);

        QPainterPath path;
        path.addEllipse(0, 0, scale, scale);

        const auto xRnd = 50;
        const auto yRnd = 36;

        if (_state == ql1s("mobile") || _state == ql1s("mobile_active"))
        {
            QPainterPath stPath(QPointF(0, 0));
            if (_miniIcons)
                stPath.addRoundRect(
                    QRect(
                        scale - scale_bitmap_with_value(mobile_rect_width_mini),
                        scale - scale_bitmap_with_value(mobile_rect_height_mini),
                        scale_bitmap_with_value(mobile_rect_width_mini),
                        scale_bitmap_with_value(mobile_rect_height_mini)
                    ),
                    xRnd,
                    yRnd
                );
            else
                stPath.addRoundRect(
                    QRect(
                        scale - scale_bitmap_with_value(mobile_rect_width),
                        scale - scale_bitmap_with_value(mobile_rect_height),
                        scale_bitmap_with_value(mobile_rect_width),
                        scale_bitmap_with_value(mobile_rect_height)
                    ),
                    xRnd,
                    yRnd
                );
            path -= stPath;
        }

        auto addedRadius = Utils::scale_value(8);
        if (_state == ql1s("photo enter") || _state == ql1s("photo leave"))
        {
            QPixmap p(Utils::parse_image_name(qsl(":/resources/addphoto_100.png")));
            int x = (scale - p.width());
            int y = (scale - p.height());
            QPainterPath stPath(QPointF(0, 0));
            stPath.addEllipse(x - addedRadius / 2, y - addedRadius / 2, p.width() + addedRadius, p.height() + addedRadius);
            path -= stPath;
        }

        painter.setClipPath(path);
        painter.drawPixmap(0, 0, _img);

        if (_state == ql1s("photo enter"))
        {
            QColor photoEnterColor(ql1s("#000000"));
            photoEnterColor.setAlphaF(0.3);
            painter.setBrush(QBrush(photoEnterColor));
            painter.drawEllipse(0, 0, scale, scale);
            painter.setBrush(Qt::NoBrush);

            const auto fontSize = Utils::scale_bitmap(18);
            painter.setFont(Fonts::appFontScaled(fontSize));
            painter.setPen(QPen(QColor(ql1s("#ffffff"))));

            painter.drawText(QRectF(0, 0, scale, scale), Qt::AlignCenter, QT_TRANSLATE_NOOP("avatar_upload", "Edit\nphoto"));
            painter.setPen(Qt::NoPen);
        }

        if (_state == ql1s("online") || _state == ql1s("dnd") || _state == ql1s("online_active"))
        {
            QPainterPath stPath(QPointF(0,0));
            stPath.addRect(0, 0, scale, scale);
            painter.setClipPath(stPath);
            QPixmap p;
            if (_state == ql1s("online_active"))
                p = QPixmap(Utils::parse_image_name(_miniIcons ? qsl(":/cl_status/online_mini_100_active") : qsl(":/cl_status/online_100_active")));
            else
                p = QPixmap(Utils::parse_image_name(_miniIcons ? qsl(":/cl_status/online_mini_100") : qsl(":/cl_status/online_100")));
            int x = (scale - p.width());
            int y = (scale - p.height());
            painter.drawPixmap(x, y, p);
        }
        else if (_state == ql1s("mobile") || _state == ql1s("mobile_active"))
        {
            QPainterPath stPath(QPointF(0,0));
            stPath.addRect(0, 0, scale, scale);
            painter.setClipPath(stPath);
            QPixmap p;
            if (_state == ql1s("mobile"))
                p = QPixmap(Utils::parse_image_name(_miniIcons ? qsl(":/cl_status/mobile_mini_100") : qsl(":/cl_status/mobile_100")));
            else
                p = QPixmap(Utils::parse_image_name(_miniIcons ? qsl(":/cl_status/mobile_mini_100_active") : qsl(":/cl_status/mobile_100_active")));
            int x = (scale - p.width());
            int y = (scale - p.height());
            painter.drawPixmap(x, y, p);
        }
        else if (_state == ql1s("photo enter") || _state == ql1s("photo leave"))
        {
            QPixmap p(Utils::parse_image_name(qsl(":/resources/addphoto_100.png")));
            int x = (scale - p.width());
            int y = (scale - p.height());

            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::NoBrush);
            QPainterPath stPath(QPointF(0, 0));
            stPath.addRect(0, 0, scale, scale);
            painter.setClipPath(stPath);

            painter.setBrush(Qt::transparent);
            painter.drawEllipse(x - addedRadius / 2, y - addedRadius / 2, p.width() + addedRadius, p.height() + addedRadius);
            painter.drawPixmap(x, y, p);
        }

        Utils::check_pixel_ratio(imageOut);

        return imageOut;
    }

    bool isValidEmailAddress(const QString& _email)
    {
        static const QRegularExpression re(
            qsl("^\\b[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,10}\\b$"),
            QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::OptimizeOnFirstUsageOption
        );
        return re.match(_email).hasMatch();
    }

    bool foregroundWndIsFullscreened()
    {
#ifdef _WIN32
        const HWND foregroundWindow = ::GetForegroundWindow();

        RECT rcScreen;
        ::GetWindowRect(GetDesktopWindow(), &rcScreen);

        RECT rcForegroundApp;
        GetWindowRect(foregroundWindow, &rcForegroundApp);

        if (foregroundWindow != ::GetDesktopWindow() && foregroundWindow != ::GetShellWindow())
        {
            return rcScreen.left == rcForegroundApp.left &&
                rcScreen.top == rcForegroundApp.top &&
                rcScreen.right == rcForegroundApp.right &&
                rcScreen.bottom == rcForegroundApp.bottom;
        }
        return false;
#elif __APPLE__
        return MacSupport::isFullScreen();
#else
        return false;
#endif
    }

    double fscale_value(const double _px) noexcept
    {
        return  _px * getScaleCoefficient();
    }

    int scale_value(const int _px) noexcept
    {
        return int(getScaleCoefficient() * _px);
    }

    QSize scale_value(const QSize _px) noexcept
    {
        return _px * getScaleCoefficient();
    }

    QSizeF scale_value(const QSizeF _px) noexcept
    {
        return _px * getScaleCoefficient();
    }

    QRect scale_value(const QRect _px) noexcept
    {
        return QRect(_px.topLeft(), scale_value(_px.size()));
    }

    int unscale_value(const int _px) noexcept
    {
        double scale = getScaleCoefficient();
        return int(_px / double(scale == 0 ? 1.0 : scale));
    }

    QSize unscale_value(const QSize& _px) noexcept
    {
        return _px / getScaleCoefficient();
    }

    QRect unscale_value(const QRect& _px) noexcept
    {
        return QRect(_px.topLeft(), unscale_value(_px.size()));
    }

    int scale_bitmap_ratio() noexcept
    {
        return is_mac_retina() ? 2 : 1;
    }

    int scale_bitmap(const int _px) noexcept
    {
        return _px * scale_bitmap_ratio();
    }

    double fscale_bitmap(const double _px) noexcept
    {
        return _px * scale_bitmap_ratio();
    }

    QSize scale_bitmap(const QSize& _px) noexcept
    {
        return _px * scale_bitmap_ratio();
    }

    QSizeF scale_bitmap(const QSizeF& _px) noexcept
    {
        return _px * scale_bitmap_ratio();
    }

    QRect scale_bitmap(const QRect& _px) noexcept
    {
        return QRect(_px.topLeft(), scale_bitmap(_px.size()));
    }

    int unscale_bitmap(const int _px) noexcept
    {
        return _px / scale_bitmap_ratio();
    }

    QSize unscale_bitmap(const QSize& _px) noexcept
    {
        return _px / scale_bitmap_ratio();
    }

    QRect unscale_bitmap(const QRect& _px) noexcept
    {
        return QRect(_px.topLeft(), unscale_bitmap(_px.size()));
    }

    int scale_bitmap_with_value(const int _px) noexcept
    {
        return scale_value(scale_bitmap(_px));
    }

    double fscale_bitmap_with_value(const double _px) noexcept
    {
        return fscale_value(fscale_bitmap(_px));
    }

    QSize scale_bitmap_with_value(const QSize& _px) noexcept
    {
        return scale_value(scale_bitmap(_px));
    }

    QSizeF scale_bitmap_with_value(const QSizeF& _px) noexcept
    {
        return scale_value(scale_bitmap(_px));
    }

    QRect scale_bitmap_with_value(const QRect& _px) noexcept
    {
        return QRect(_px.topLeft(), scale_bitmap_with_value(_px.size()));
    }

    QRectF scale_bitmap_with_value(const QRectF& _px) noexcept
    {
        return QRectF(_px.topLeft(), scale_bitmap_with_value(_px.size()));
    }

    int getBottomPanelHeight() { return  Utils::scale_value(48); };
    int getTopPanelHeight() { return  Utils::scale_value(56); };

    template <typename _T>
    void check_pixel_ratio(_T& _image)
    {
        if (is_mac_retina())
            _image.setDevicePixelRatio(2);
    }
    template void check_pixel_ratio<QImage>(QImage& _pixmap);
    template void check_pixel_ratio<QPixmap>(QPixmap& _pixmap);

    QString parse_image_name(const QString& _imageName)
    {
        QString result(_imageName);
        if (is_mac_retina())
        {
            result.replace(ql1s("/100/"), ql1s("/200/"));
            result.replace(ql1s("_100"), ql1s("_200"));
            return result;
        }

        int currentScale = Utils::getScaleCoefficient() * 100;
        if (currentScale > 200)
        {
            if (Ui::GetAppConfig().IsFullLogEnabled())
            {
                std::stringstream logData;
                logData << "INVALID ICON SIZE = " << currentScale << ", " << _imageName.toUtf8().constData();
                Log::write_network_log(logData.str());
            }
            currentScale = 200;
        }

        const QString scaleString = QString::number(currentScale);
        result.replace(ql1s("_100"), ql1c('_') % scaleString);
        result.replace(ql1s("/100/"), ql1c('/') % scaleString %  ql1c('/'));
        return result;
    }

    QPixmap renderSvg(const QString& _filePath, const QSize& _scaledSize, const QColor& _tintColor, const KeepRatio _keepRatio)
    {
        if (_filePath.isEmpty())
            return QPixmap();

        if (Q_UNLIKELY(!QFile::exists(_filePath)))
            return QPixmap();

        QSvgRenderer renderer(_filePath);
        const auto defSize = renderer.defaultSize();

        if (Q_UNLIKELY(_scaledSize.isEmpty() && defSize.isEmpty()))
            return QPixmap();

        QSize resultSize;
        QRect bounds;

        if (!_scaledSize.isEmpty())
        {
            resultSize = scale_bitmap(_scaledSize);
            bounds = QRect(QPoint(), resultSize);

            if (_keepRatio == KeepRatio::yes)
            {
                const double wRatio = defSize.width() / (double)resultSize.width();
                const double hRatio = defSize.height() / (double)resultSize.height();
                constexpr double epsilon = std::numeric_limits<double>::epsilon();

                if (Q_UNLIKELY(std::fabs(wRatio - hRatio) > epsilon))
                {
                    const auto resultCenter = bounds.center();
                    bounds.setSize(defSize.scaled(resultSize, Qt::KeepAspectRatio));
                    bounds.moveCenter(resultCenter);
                }
            }
        }
        else
        {
            resultSize = scale_bitmap_with_value(defSize);
            bounds = QRect(QPoint(), resultSize);
        }

        QPixmap pixmap(resultSize);
        pixmap.fill(Qt::transparent);

        {
            QPainter painter(&pixmap);
            renderer.render(&painter, bounds);

            if (_tintColor.isValid())
            {
                painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                painter.fillRect(bounds, _tintColor);
            }
        }

        check_pixel_ratio(pixmap);

        return pixmap;
    }

    QPixmap renderSvgLayered(const QString& _filePath, const SvgLayers& _layers, const QSize& _scaledSize)
    {
        if (_filePath.isEmpty())
            return QPixmap();

        if (Q_UNLIKELY(!QFile::exists(_filePath)))
            return QPixmap();

        QSvgRenderer renderer(_filePath);
        const auto defSize = scale_bitmap_with_value(renderer.defaultSize());

        if (Q_UNLIKELY(_scaledSize.isEmpty() && defSize.isEmpty()))
            return QPixmap();

        QSize resultSize = _scaledSize.isEmpty() ? defSize : scale_bitmap(_scaledSize);
        QPixmap pixmap(resultSize);
        pixmap.fill(Qt::transparent);

        QMatrix scaleMatrix;
        if (!_scaledSize.isEmpty() && defSize != resultSize)
        {
            const auto s = double(resultSize.width()) / defSize.width();
            scaleMatrix.scale(s, s);
        }

        {
            QPainter painter(&pixmap);

            if (!_layers.empty())
            {
                for (const auto& [layerName, layerColor] : _layers)
                {
                    if (!layerColor.isValid() || layerColor.alpha() == 0)
                        continue;

                    assert(renderer.elementExists(layerName));
                    if (!renderer.elementExists(layerName))
                        continue;

                    QPixmap layer(resultSize);
                    layer.fill(Qt::transparent);

                    const auto elMatrix = renderer.matrixForElement(layerName);
                    const auto elBounds = renderer.boundsOnElement(layerName);
                    const auto mappedRect = scale_bitmap_with_value(elMatrix.mapRect(elBounds));
                    auto layerBounds = scaleMatrix.mapRect(mappedRect);
                    layerBounds.moveTopLeft(QPointF(
                        fscale_bitmap_with_value(layerBounds.topLeft().x()),
                        fscale_bitmap_with_value(layerBounds.topLeft().y())
                    ));

                    {
                        QPainter layerPainter(&layer);
                        renderer.render(&layerPainter, layerName, layerBounds);

                        layerPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                        layerPainter.fillRect(layer.rect(), layerColor);
                    }
                    painter.drawPixmap(QPoint(), layer);
                }
            }
            else
            {
                renderer.render(&painter, QRect(QPoint(), resultSize));
            }
        }

        check_pixel_ratio(pixmap);

        return pixmap;
    }

    void addShadowToWidget(QWidget* _target)
    {
        QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(_target);
        QColor widgetShadowColor(ql1s("#000000"));
        widgetShadowColor.setAlphaF(0.3);
        shadow->setColor(widgetShadowColor);
        shadow->setBlurRadius(scale_value(16));
        shadow->setXOffset(scale_value(0));
        shadow->setYOffset(scale_value(2));
        _target->setGraphicsEffect(shadow);
    }

    void addShadowToWindow(QWidget* _target, bool _enabled)
    {
        int shadowWidth = _enabled ? Ui::get_gui_settings()->get_shadow_width() : 0;
        if (_enabled && !_target->testAttribute(Qt::WA_TranslucentBackground))
            _target->setAttribute(Qt::WA_TranslucentBackground);

        auto oldMargins = _target->contentsMargins();
        _target->setContentsMargins(QMargins(oldMargins.left() + shadowWidth, oldMargins.top() + shadowWidth,
            oldMargins.right() + shadowWidth, oldMargins.bottom() + shadowWidth));

        static QPointer<QObject> eventFilter(new ShadowWidgetEventFilter(Ui::get_gui_settings()->get_shadow_width()));

        if (_enabled)
            _target->installEventFilter(eventFilter);
        else
            _target->removeEventFilter(eventFilter);
    }

    void grabTouchWidget(QWidget* _target, bool _topWidget)
    {
#ifdef _WIN32
        if (_topWidget)
        {
            QScrollerProperties sp;
            sp.setScrollMetric(QScrollerProperties::DragVelocitySmoothingFactor, 0.6);
            sp.setScrollMetric(QScrollerProperties::MinimumVelocity, 0.0);
            sp.setScrollMetric(QScrollerProperties::MaximumVelocity, 0.5);
            sp.setScrollMetric(QScrollerProperties::AcceleratingFlickMaximumTime, 0.4);
            sp.setScrollMetric(QScrollerProperties::AcceleratingFlickSpeedupFactor, 1.2);
            sp.setScrollMetric(QScrollerProperties::SnapPositionRatio, 0.2);
            sp.setScrollMetric(QScrollerProperties::MaximumClickThroughVelocity, 0);
            sp.setScrollMetric(QScrollerProperties::DragStartDistance, 0.01);
            sp.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0);
            QVariant overshootPolicy = QVariant::fromValue<QScrollerProperties::OvershootPolicy>(QScrollerProperties::OvershootAlwaysOff);
            sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, overshootPolicy);
            sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, overshootPolicy);

            QScroller* clScroller = QScroller::scroller(_target);
            clScroller->grabGesture(_target);
            clScroller->setScrollerProperties(sp);
        }
        else
        {
            QScroller::grabGesture(_target);
        }
#else
        Q_UNUSED(_target);
        Q_UNUSED(_topWidget);
#endif //WIN32
    }

    void removeLineBreaks(QString& _source)
    {
        if (_source.isEmpty())
            return;

        bool spaceAtEnd = _source.at(_source.length() - 1) == QChar::Space;
        _source.replace(ql1c('\n'), QChar::Space);
        _source.remove(ql1c('\r'));
        if (!spaceAtEnd && _source.at(_source.length() - 1) == QChar::Space)
            _source.chop(1);
    }

    QString rgbaStringFromColor(const QColor& _color)
    {
        return QString::asprintf("rgba(%d, %d, %d, %d%%)", _color.red(), _color.green(), _color.blue(), _color.alpha() * 100 / 255);
    }

    static bool macRetina = false;

    bool is_mac_retina() noexcept
    {
        return platform::is_apple() && macRetina;
    }

    void set_mac_retina(bool _val) noexcept
    {
        macRetina = _val;
    }

    static double scaleCoefficient = 1.0;

    double getScaleCoefficient() noexcept
    {
        return scaleCoefficient;
    }

    void setScaleCoefficient(double _coefficient) noexcept
    {
        if constexpr (platform::is_apple())
        {
            scaleCoefficient = 1.0;
            return;
        }

        constexpr double epsilon = std::numeric_limits<double>::epsilon();

        if (
            std::fabs(_coefficient - 1.0) < epsilon ||
            std::fabs(_coefficient - 1.25) < epsilon ||
            std::fabs(_coefficient - 1.5) < epsilon ||
            std::fabs(_coefficient - 2.0) < epsilon ||
            std::fabs(_coefficient - 2.5) < epsilon ||
            std::fabs(_coefficient - 3.0) < epsilon
            )
        {
            scaleCoefficient = _coefficient;
            return;
        }

        assert(!"unexpected scale value");
        scaleCoefficient = 1.0;
    }

    namespace { double basicScaleCoefficient = 1.0; }

    double getBasicScaleCoefficient() noexcept
    {
        return basicScaleCoefficient;
    }

    void initBasicScaleCoefficient(double _coefficient) noexcept
    {
        static bool isInitBasicScaleCoefficient = false;
        if (!isInitBasicScaleCoefficient)
            basicScaleCoefficient = _coefficient;
        else
            assert(!"initBasicScaleCoefficient should be called once.");
    }

    void groupTaskbarIcon(bool _enabled)
    {
#ifdef _WIN32
        if (QSysInfo().windowsVersion() >= QSysInfo::WV_WINDOWS7)
        {
            HMODULE libShell32 = ::GetModuleHandle(L"shell32.dll");
            typedef HRESULT  (__stdcall * SetCurrentProcessExplicitAppUserModelID_Type)(PCWSTR);
            SetCurrentProcessExplicitAppUserModelID_Type SetCurrentProcessExplicitAppUserModelID_Func;
            SetCurrentProcessExplicitAppUserModelID_Func = (SetCurrentProcessExplicitAppUserModelID_Type)::GetProcAddress(libShell32,"SetCurrentProcessExplicitAppUserModelID");
            SetCurrentProcessExplicitAppUserModelID_Func(_enabled ? (build::GetProductVariant(application_user_model_id_icq, application_user_model_id_agent, application_user_model_id_biz, application_user_model_id_dit)) : L"");
        }
#endif //_WIN32
    }

    bool isStartOnStartup()
    {
#ifdef _WIN32
        CRegKey keySoftwareRun;
        if (ERROR_SUCCESS != keySoftwareRun.Open(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", KEY_READ))
            return false;

        wchar_t bufferPath[1025];
        ULONG len = 1024;
        auto productName = getProductName();
        if (keySoftwareRun.QueryStringValue((const wchar_t*) productName.utf16(), bufferPath, &len) != ERROR_SUCCESS)
            return false;

#endif //_WIN32
        return true;
    }

    void setStartOnStartup(bool _start)
    {
#ifdef _WIN32

        bool currentState = isStartOnStartup();
        if (currentState == _start)
            return;

        if (_start)
        {
            CRegKey keySoftwareRun;
            if (ERROR_SUCCESS != keySoftwareRun.Open(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", KEY_SET_VALUE))
                return;

            wchar_t buffer[1025];
            if (!::GetModuleFileName(0, buffer, 1024))
                return;

            CAtlString exePath = buffer;
            auto productName = getProductName();
            if (ERROR_SUCCESS != keySoftwareRun.SetStringValue((const wchar_t*) productName.utf16(), CAtlString("\"") + exePath + "\"" + " /startup"))
                return;
        }
        else
        {
            auto productName = getProductName();
            ::SHDeleteValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", (const wchar_t*) productName.utf16());
        }
#endif //_WIN32
    }

#ifdef _WIN32
    HWND createFakeParentWindow()
    {
        HINSTANCE instance = (HINSTANCE) ::GetModuleHandle(0);
        HWND hwnd = 0;

        CAtlString className = L"fake_parent_window";
        WNDCLASSEX wc = {0};

        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = instance;
        wc.lpszClassName = (LPCWSTR)className;
        if (!::RegisterClassEx(&wc))
            return hwnd;

        hwnd = ::CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT, (LPCWSTR)className, 0, WS_POPUP, 0, 0, 0, 0, 0, 0, instance, 0);
        if (!hwnd)
            return hwnd;

        ::SetLayeredWindowAttributes(
            hwnd,
            0,
            0,
            LWA_ALPHA
            );

        return hwnd;
    }
#endif //WIN32

    int calcAge(const QDateTime& _birthdate)
    {
        QDate thisdate = QDateTime::currentDateTime().date();
        QDate birthdate = _birthdate.date();

        int age = thisdate.year() - birthdate.year();
        if (age < 0)
            return 0;

        if ((birthdate.month() > thisdate.month()) || (birthdate.month() == thisdate.month() && birthdate.day() > thisdate.day()))
            return (--age);

        return age;
    }

    QString DefaultDownloadsPath()
    {
        return QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    }

    QString UserDownloadsPath()
    {
        auto path = QDir::toNativeSeparators(Ui::get_gui_settings()->get_value(settings_download_directory, Utils::DefaultDownloadsPath()));
        return QFileInfo(path).canonicalFilePath(); // we need canonical path to follow symlinks
    }

    QString UserSaveAsPath()
    {
        QString result = QDir::toNativeSeparators(Ui::get_gui_settings()->get_value(settings_download_directory_save_as, QString()));
        return (result.isEmpty() ? UserDownloadsPath() : result);
    }

    template<typename T>
    static bool containsCaseInsensitive(T first, T last, const QString& _ext)
    {
        return std::any_of(first, last, [&_ext](const auto& e) { return _ext.compare(e, Qt::CaseInsensitive) == 0; });
    }

    const std::vector<QLatin1String>& getImageExtensions()
    {
        const static std::vector<QLatin1String> ext = { ql1s("jpg"), ql1s("jpeg"), ql1s("png"), ql1s("bmp"), ql1s("tif"), ql1s("tiff") };
        return ext;
    }

    const std::vector<QLatin1String>& getVideoExtensions()
    {
        const static std::vector<QLatin1String> ext = { ql1s("avi"), ql1s("mkv"), ql1s("wmv"), ql1s("flv"), ql1s("3gp"), ql1s("mpeg4"), ql1s("mp4"), ql1s("webm"), ql1s("mov"), ql1s("m4v") };
        return ext;
    }

    bool is_image_extension(const QString& _ext)
    {
        const static auto gifExt = { ql1s("gif") };
        return is_image_extension_not_gif(_ext) || containsCaseInsensitive(gifExt.begin(), gifExt.end(), _ext);
    }

    bool is_image_extension_not_gif(const QString& _ext)
    {
        const auto& imagesExtensions = getImageExtensions();
        return containsCaseInsensitive(imagesExtensions.begin(), imagesExtensions.end(), _ext);
    }

    bool is_video_extension(const QString& _ext)
    {
        const auto& videoExtensions = getVideoExtensions();
        return containsCaseInsensitive(videoExtensions.begin(), videoExtensions.end(), _ext);
    }

    void copyFileToClipboard(const QString& _path)
    {
        QMimeData* mimeData = new QMimeData();
        mimeData->setUrls({ QUrl::fromLocalFile(_path) });
        if (is_image_extension(QFileInfo(_path).suffix()))
            mimeData->setImageData(QImage(_path));
        QApplication::clipboard()->setMimeData(mimeData);
    }

    void saveAs(const QString& _inputFilename, std::function<void (const QString& _filename, const QString& _directory)> _callback, std::function<void ()> _cancel_callback, bool asSheet)
    {
        static const QRegularExpression re(
            qsl("\\/:*?\"<>\\|\""),
            QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::OptimizeOnFirstUsageOption
        );

        auto save_to_settings = [](const QString& _lastDirectory) {
            Ui::setSaveAsPath(QDir::toNativeSeparators(_lastDirectory));
        };

        auto lastDirectory = Ui::getSaveAsPath();
        const int dot = _inputFilename.lastIndexOf(ql1c('.'));
        const auto ext = dot != -1 ? _inputFilename.midRef(dot) : QStringRef();
        QString name = (_inputFilename.size() >= 128 || _inputFilename.contains(re)) ? QT_TRANSLATE_NOOP("chat_page", "File") : _inputFilename;
        QString fullName = QDir::toNativeSeparators(QDir(lastDirectory).filePath(name));

#ifdef __APPLE__
        if (asSheet)
        {
            auto callback = [save_to_settings, _callback](const QString& _filename, const QString& _directory){
                save_to_settings(_directory);

                if (_callback)
                    _callback(_filename, _directory);
            };

            MacSupport::saveFileName(QT_TRANSLATE_NOOP("context_menu", "Save as..."), fullName, ql1c('*') % ext, callback, ext.toString(), _cancel_callback);
            return;
        }
#endif //__APPLE__
        const QString destination = QFileDialog::getSaveFileName(nullptr, QT_TRANSLATE_NOOP("context_menu", "Save as..."), fullName, ql1c('*') % ext);
        if (!destination.isEmpty())
        {
            const QFileInfo info(destination);
            lastDirectory = info.dir().absolutePath();
            const QString directory = info.dir().absolutePath();
            const QStringRef inputExt = !ext.isEmpty() && info.suffix().isEmpty() ? ext : QStringRef();
            const QString filename = info.fileName() % inputExt;
            if (_callback)
                _callback(filename, directory);

            save_to_settings(directory);
        }
        else if (_cancel_callback)
        {
            _cancel_callback();
        }
    }

    const SendKeysIndex& getSendKeysIndex()
    {
        static SendKeysIndex index;

        if (index.empty())
        {
            index.emplace_back(QT_TRANSLATE_NOOP("settings", "Enter"), Ui::KeyToSendMessage::Enter);
            if constexpr (platform::is_apple())
            {
                index.emplace_back(Ui::KeySymbols::Mac::control + QT_TRANSLATE_NOOP("settings", " + Enter"), Ui::KeyToSendMessage::CtrlMac_Enter);
                index.emplace_back(Ui::KeySymbols::Mac::command + QT_TRANSLATE_NOOP("settings", " + Enter"), Ui::KeyToSendMessage::Ctrl_Enter);
                index.emplace_back(Ui::KeySymbols::Mac::shift + QT_TRANSLATE_NOOP("settings", " + Enter"), Ui::KeyToSendMessage::Shift_Enter);
            }
            else
            {
                index.emplace_back(QT_TRANSLATE_NOOP("settings", "Ctrl + Enter"), Ui::KeyToSendMessage::Ctrl_Enter);
                index.emplace_back(QT_TRANSLATE_NOOP("settings", "Shift + Enter"), Ui::KeyToSendMessage::Shift_Enter);
            }
        }

        return index;
    }

    const ShortcutsCloseActionsList& getShortcutsCloseActionsList()
    {
        static const ShortcutsCloseActionsList closeList
        {
            { QT_TRANSLATE_NOOP("settings", "Minimize the window"), Ui::ShortcutsCloseAction::RollUpWindow },
            { QT_TRANSLATE_NOOP("settings", "Minimize the window and minimize the chat"), Ui::ShortcutsCloseAction::RollUpWindowAndChat },
            { QT_TRANSLATE_NOOP("settings", "Close chat"), Ui::ShortcutsCloseAction::CloseChat },
        };

        return closeList;
    }

    QString getShortcutsCloseActionName(Ui::ShortcutsCloseAction _action)
    {
        const auto& actsList = getShortcutsCloseActionsList();
        for (const auto& [name, code] : actsList)
        {
            if (_action == code)
                return name;
        }

        return QString();
    }

    const ShortcutsSearchActionsList& getShortcutsSearchActionsList()
    {
        static const ShortcutsSearchActionsList searchList
        {
            { QT_TRANSLATE_NOOP("settings", "Search in chat"), Ui::ShortcutsSearchAction::SearchInChat },
            { QT_TRANSLATE_NOOP("settings", "Global search"), Ui::ShortcutsSearchAction::GlobalSearch },
        };

        return searchList;
    }

    const PrivacyAllowVariantsList& getPrivacyAllowVariants()
    {
        static const PrivacyAllowVariantsList variants
        {
            { QT_TRANSLATE_NOOP("settings", "Everybody"), core::privacy_access_right::everybody },
            { QT_TRANSLATE_NOOP("settings", "People you have corresponded with and contacts from your phone book"), core::privacy_access_right::my_contacts },
            { QT_TRANSLATE_NOOP("settings", "Nobody"), core::privacy_access_right::nobody },
        };

        return variants;
    }

    void post_stats_with_settings()
    {
        core::stats::event_props_type props;

        //System settings
        const auto geometry = qApp->desktop()->screenGeometry();
        props.emplace_back("Sys_Screen_Size", std::to_string(geometry.width()) + 'x' + std::to_string(geometry.height()));

        const QString processorInfo = ql1s("Architecture:") % QSysInfo::currentCpuArchitecture() % ql1s(" Number of Processors:") % QString::number(QThread::idealThreadCount());
        props.emplace_back("Sys_CPU", processorInfo.toStdString());

        auto processorId = getCpuId().trimmed();
        if (processorId.isEmpty())
            processorId = qsl("Unknown");
        props.emplace_back("Sys_CPUID", processorId.toStdString());

        //General settings
        props.emplace_back("Settings_Startup", std::to_string(Utils::isStartOnStartup()));
        props.emplace_back("Settings_Minimized_Start", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_start_minimazed, true)));
        props.emplace_back("Settings_Taskbar", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_show_in_taskbar, true)));
        props.emplace_back("Settings_Sounds", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_sounds_enabled, true)));

        //Chat settings
        props.emplace_back("Settings_Previews", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_show_video_and_images, true)));
        props.emplace_back("Settings_AutoPlay_Video", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_autoplay_video, true)));

        auto currentDownloadDir = Ui::getDownloadPath();
        props.emplace_back(std::make_pair("Settings_Download_Folder", std::to_string(currentDownloadDir == Ui::getDownloadPath())));

        QString keyToSend;
        int currentKey = Ui::get_gui_settings()->get_value<int>(settings_key1_to_send_message, Ui::KeyToSendMessage::Enter);
        for (const auto& key : Utils::getSendKeysIndex())
        {
            if (key.second == currentKey)
                keyToSend = key.first;
        }
        props.emplace_back("Settings_Send_By", keyToSend.toStdString());

        //Interface settings
        props.emplace_back("Settings_Compact_Recents", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_show_last_message, true)));
        props.emplace_back("Settings_Scale", std::to_string(Utils::getScaleCoefficient()));
        props.emplace_back("Settings_Language", Ui::get_gui_settings()->get_value(settings_language, QString()).toUpper().toStdString());

        if (const auto id = Styling::getThemesContainer().getGlobalWallpaperId(); id.isValid())
            props.emplace_back("Settings_Wallpaper_Global", id.id_.toStdString());
        else
            props.emplace_back("Settings_Wallpaper_Global", std::to_string(-1));

        const auto& themeCounts = Styling::getThemesContainer().getContactWallpaperCounters();
        for (const auto [wallId, count] : themeCounts)
            props.emplace_back("Settings_Wallpaper_Local " + wallId.toStdString(), std::to_string(count));

        //Notifications settings
        props.emplace_back("Settings_Sounds_Outgoing", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_outgoing_message_sound_enabled, false)));
        props.emplace_back("Settings_Alerts", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_notify_new_messages, true)));
        props.emplace_back("Settings_Mail_Alerts", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_notify_new_mail_messages, true)));

        props.emplace_back("SettingsMain_ReadAll_Type", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_partial_read, settings_partial_read_deafult())));

        props.emplace_back("Proxy_Type", ProxySettings::proxyTypeStr(Utils::get_proxy_settings()->type_).toStdString());


        props.emplace_back("Settings_Suggest_Emoji", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_show_suggests_emoji, true)));
        props.emplace_back("Settings_Suggest_Words", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_show_suggests_words, true)));
        props.emplace_back("Settings_Autoreplace_Emoji", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_autoreplace_emoji, settings_autoreplace_emoji_deafult())));
        props.emplace_back("Settings_Allow_Big_Emoji", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_allow_big_emoji, settings_allow_big_emoji_deafult())));
        props.emplace_back("Settings_AutoPlay_Gif", std::to_string(Ui::get_gui_settings()->get_value<bool>(settings_autoplay_gif, true)));

        Ui::GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::client_settings, props);
    }

    QRect GetMainRect()
    {
        assert(!!Utils::InterConnector::instance().getMainWindow() && "Common.cpp (ItemLength)");
        QRect mainRect = GetWindowRect(Utils::InterConnector::instance().getMainWindow());
        assert("Couldn't get rect: Common.cpp (ItemLength)");
        return mainRect;
    }

    QRect GetWindowRect(QWidget* window)
    {
        QRect mainRect(0, 0, 0, 0);
        if (window)
        {
            mainRect = window->geometry();
        }
        else if (auto _window = qApp->activeWindow())
        {
            mainRect = _window->geometry();
        }
        return mainRect;
    }

    QPoint GetMainWindowCenter()
    {
        auto mainRect = Utils::GetMainRect();
        auto mainWidth = mainRect.width();
        auto mainHeight = mainRect.height();

        auto x = mainRect.x() + mainWidth / 2;
        auto y = mainRect.y() + mainHeight / 2;
        return QPoint(x, y);
    }

    void UpdateProfile(const std::vector<std::pair<std::string, QString>>& _fields)
    {
        Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);

        core::ifptr<core::iarray> fieldArray(collection->create_array());
        fieldArray->reserve((int)_fields.size());
        for (const auto& field : _fields)
        {
            Ui::gui_coll_helper coll(Ui::GetDispatcher()->create_collection(), true);
            coll.set_value_as_string("field_name", field.first);
            coll.set_value_as_qstring("field_value", field.second);

            core::ifptr<core::ivalue> valField(coll->create_value());
            valField->set_as_collection(coll.get());
            fieldArray->push_back(valField.get());
        }

        collection.set_value_as_array("fields", fieldArray.get());
        Ui::GetDispatcher()->post_message_to_core("update_profile", collection.get());
    }

    QString getItemSafe(const std::vector<QString>& _values, size_t _selected, const QString& _default)
    {
        return _selected < _values.size() ? _values[_selected] : _default;
    }

    Ui::GeneralDialog *NameEditorDialog(
        QWidget* _parent,
        const QString& _chatName,
        const QString& _buttonText,
        const QString& _headerText,
        Out QString& _resultChatName,
        bool acceptEnter)
    {
        auto mainWidget = new QWidget(_parent);
        mainWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

        auto layout = Utils::emptyVLayout(mainWidget);
        layout->setContentsMargins(Utils::scale_value(16), Utils::scale_value(16), Utils::scale_value(16), 0);

        auto textEdit = new Ui::TextEditEx(mainWidget, Fonts::appFontScaled(18), Ui::MessageStyle::getTextColor(), true, true);
        Utils::ApplyStyle(textEdit, Styling::getParameters().getLineEditCommonQss());
        textEdit->setObjectName(qsl("input_edit_control"));
        textEdit->setPlaceholderText(_chatName);
        textEdit->setPlainText(_chatName);
        textEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        textEdit->setAutoFillBackground(false);
        textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        textEdit->setTextInteractionFlags(Qt::TextEditable | Qt::TextEditorInteraction);
        textEdit->setCatchEnter(acceptEnter);
        textEdit->setMinimumWidth(Utils::scale_value(300));
        Utils::ApplyStyle(textEdit, qsl("padding: 0; margin: 0;"));
        Testing::setAccessibleName(textEdit, qsl("AS utils textEdit"));
        layout->addWidget(textEdit);

        auto horizontalLineWidget = new QWidget(_parent);
        horizontalLineWidget->setFixedHeight(Utils::scale_value(1));
        horizontalLineWidget->setStyleSheet(qsl("background-color: %1;").arg(Styling::getParameters().getColorHex(Styling::StyleVariable::PRIMARY)));
        Testing::setAccessibleName(horizontalLineWidget, qsl("AS utils horizontalLineWidget"));
        layout->addWidget(horizontalLineWidget);

        auto generalDialog = new Ui::GeneralDialog(mainWidget, Utils::InterConnector::instance().getMainWindow());
        generalDialog->addLabel(_headerText);
        generalDialog->addButtonsPair(QT_TRANSLATE_NOOP("popup_window", "CANCEL"), _buttonText, true);

        if (acceptEnter)
            QObject::connect(textEdit, &Ui::TextEditEx::enter, generalDialog, &Ui::GeneralDialog::accept);

        QTextCursor cursor = textEdit->textCursor();
        cursor.select(QTextCursor::Document);
        textEdit->setTextCursor(cursor);
        textEdit->setFrameStyle(QFrame::NoFrame);

        // to disable link/email highlighting
        textEdit->setFontUnderline(false);
        textEdit->setTextColor(Ui::MessageStyle::getTextColor());

        textEdit->setFocus();

        return generalDialog;
    }

    bool NameEditor(
        QWidget* _parent,
        const QString& _chatName,
        const QString& _buttonText,
        const QString& _headerText,
        Out QString& _resultChatName,
        bool acceptEnter)
    {
        auto dialog = std::unique_ptr<Ui::GeneralDialog>(NameEditorDialog(_parent, _chatName, _buttonText, _headerText, _resultChatName, acceptEnter));
        const auto result = dialog->showInCenter();
        auto textEdit = dialog->findChild<Ui::TextEditEx*>(qsl("input_edit_control"));
        if (textEdit)
            _resultChatName = textEdit->getPlainText();
        return result;
    }

    bool GetConfirmationWithTwoButtons(
        const QString& _buttonLeftText,
        const QString& _buttonRightText,
        const QString& _messageText,
        const QString& _labelText,
        QWidget* _parent,
        QWidget* _mainWindow,
        bool _withSemiwindow)
    {
        Ui::GeneralDialog dialog(nullptr, _mainWindow ? _mainWindow : Utils::InterConnector::instance().getMainWindow(), false, true, true, _withSemiwindow);
        dialog.addLabel(_labelText);
        dialog.addText(_messageText, Utils::scale_value(12));
        dialog.addButtonsPair(_buttonLeftText, _buttonRightText, true);

        return dialog.showInCenter();
    }

    bool GetConfirmationWithOneButton(
        const QString& _buttonText,
        const QString& _messageText,
        const QString& _labelText,
        QWidget* _parent,
        QWidget* _mainWindow,
        bool _withSemiwindow)
    {
        Ui::GeneralDialog dialog(nullptr, _mainWindow ? _mainWindow : Utils::InterConnector::instance().getMainWindow(), false, true, true, _withSemiwindow);
        dialog.addLabel(_labelText);
        dialog.addText(_messageText, Utils::scale_value(12));
        dialog.addAcceptButton(_buttonText, true);

        return dialog.showInCenter();
    }

    bool GetErrorWithTwoButtons(
        const QString& _buttonLeftText,
        const QString& _buttonRightText,
        const QString& /*_messageText*/,
        const QString& _labelText,
        const QString& _errorText,
        QWidget* _parent)
    {
        Ui::GeneralDialog dialog(nullptr, Utils::InterConnector::instance().getMainWindow());
        dialog.addLabel(_labelText);
        dialog.addError(_errorText);
        dialog.addButtonsPair(_buttonLeftText, _buttonRightText, true);
        return dialog.showInCenter();
    }

    ProxySettings::ProxySettings(core::proxy_type _type,
                                 core::proxy_auth _authType,
                                 QString _username,
                                 QString _password,
                                 QString _proxyServer,
                                 int _port,
                                 bool _needAuth)
        : type_(_type)
        , authType_(_authType)
        , username_(std::move(_username))
        , needAuth_(_needAuth)
        , password_(std::move(_password))
        , proxyServer_(std::move(_proxyServer))
        , port_(_port)
    {}

    ProxySettings::ProxySettings()
        : type_(core::proxy_type::auto_proxy)
        , authType_(core::proxy_auth::basic)
        , needAuth_(false)
        , port_(invalidPort)
    {}

    void ProxySettings::postToCore()
    {
        Ui::gui_coll_helper coll(Ui::GetDispatcher()->create_collection(), true);

        coll.set_value_as_enum("settings_proxy_type", type_);
        coll.set_value_as_enum("settings_proxy_auth_type", authType_);
        coll.set<QString>("settings_proxy_server", proxyServer_);
        coll.set_value_as_int("settings_proxy_port", port_);
        coll.set<QString>("settings_proxy_username", username_);
        coll.set<QString>("settings_proxy_password", password_);
        coll.set_value_as_bool("settings_proxy_need_auth", needAuth_);

        Ui::GetDispatcher()->post_message_to_core("set_user_proxy_settings", coll.get());
    }

    QString ProxySettings::proxyTypeStr(core::proxy_type _type)
    {
        switch (_type)
        {
            case core::proxy_type::auto_proxy:
                return qsl("Auto");
            case core::proxy_type::http_proxy:
                return qsl("HTTP Proxy");
            case core::proxy_type::socks4:
                return qsl("SOCKS4");
            case core::proxy_type::socks5:
                return qsl("SOCKS5");
            default:
                return QString();
        }
    }

    QString ProxySettings::proxyAuthStr(core::proxy_auth _type)
    {
        switch (_type)
        {
            case core::proxy_auth::basic:
                return qsl("Basic");
            case core::proxy_auth::digest:
                return qsl("Digest");
            case core::proxy_auth::negotiate:
                return qsl("Negotiate");
            case core::proxy_auth::ntlm:
                return qsl("NTLM");
            default:
                return QString();
        }
    }

    ProxySettings* get_proxy_settings()
    {
        static auto proxySetting = std::make_unique<ProxySettings>();
        return proxySetting.get();
    }

    QSize getMaxImageSize()
    {
        static QSize sz = QApplication::desktop()->screenGeometry().size();

        return QSize(std::max(sz.width(), sz.height()), std::max(sz.width(), sz.height()));
    }

    QPixmap loadPixmap(const QString& _resource)
    {
        QPixmap p(parse_image_name(_resource));

        check_pixel_ratio(p);

        return p;
    }

    bool loadPixmap(const QString& _path, Out QPixmap& _pixmap)
    {
        assert(!_path.isEmpty());

        if (!QFile::exists(_path))
        {
            assert(!"file does not exist");
            return false;
        }

        QFile file(_path);
        if (!file.open(QIODevice::ReadOnly))
            return false;

        return loadPixmap(file.readAll(), _pixmap);
    }

    bool loadPixmapScaled(const QString& _path, const QSize& _maxSize,  Out QPixmap& _pixmap, Out QSize& _originalSize, const PanoramicCheck _checkPanoramic)
    {
        QImageReader reader(_path);
        reader.setAutoTransform(true);

        if (!reader.canRead())
        {
            reader.setDecideFormatFromContent(true);
            reader.setFileName(_path);              // needed to reset QImageReader state
            if (!reader.canRead())
                return false;
        }

        QSize imageSize = reader.size();

        _originalSize = imageSize;

        if (const auto tr = reader.transformation(); tr & QImageIOHandler::TransformationRotate90 || tr & QImageIOHandler::TransformationRotate270)
            _originalSize.transpose();

        const auto needRescale = _maxSize.isValid() && (_maxSize.width() < imageSize.width() || _maxSize.height() < imageSize.height());
        const auto isSvg = reader.format() == "svg";

        if (needRescale || isSvg)
        {
            const auto aspectMode = (_checkPanoramic == PanoramicCheck::yes && isPanoramic(_originalSize)) ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio;
            imageSize.scale(_maxSize, aspectMode);

            reader.setScaledSize(imageSize);
        }

        _pixmap = QPixmap::fromImage(reader.read());

        return true;
    }

    bool loadPixmapScaled(QByteArray& _data, const QSize& _maxSize, Out QPixmap& _pixmap, Out QSize& _originalSize)
    {
        QBuffer buffer(&_data, nullptr);

        QImageReader reader;

        reader.setDecideFormatFromContent(true);

        reader.setDevice(&buffer);

        if (!reader.canRead())
            return false;

        QSize imageSize = reader.size();

        _originalSize = imageSize;

        if (_maxSize.isValid() && (_maxSize.width() < imageSize.width() || _maxSize.height() < imageSize.height()))
        {
            imageSize.scale(_maxSize, Qt::KeepAspectRatio);

            reader.setScaledSize(imageSize);
        }

        _pixmap = QPixmap::fromImage(reader.read());

        return true;
    }

    bool loadPixmap(const QByteArray& _data, Out QPixmap& _pixmap)
    {
        assert(!_data.isEmpty());

        static QMimeDatabase db;
        if (db.mimeTypeForData(_data).preferredSuffix() == ql1s("svg"))
        {
            QSvgRenderer renderer;
            if (!renderer.load(_data))
                return false;

            _pixmap = QPixmap(renderer.defaultSize());
            _pixmap.fill(Qt::white);

            QPainter p(&_pixmap);
            renderer.render(&p);
            return !_pixmap.isNull();
        }


        constexpr std::array<const char*, 3> availableFormats = { nullptr, "png", "jpg" };

        for (auto fmt : availableFormats)
        {
            _pixmap.loadFromData(_data, fmt);

            if (!_pixmap.isNull())
            {
                const auto orientation = Exif::getExifOrientation(_data.data(), _data.size());
                Exif::applyExifOrientation(orientation, InOut _pixmap);
                return true;
            }
        }

        return false;
    }

    bool dragUrl(QObject*, const QPixmap& _preview, const QString& _url)
    {
        QDrag *drag = new QDrag(&(Utils::InterConnector::instance()));

        QMimeData *mimeData = new QMimeData();
        mimeData->setProperty("icq", true);

        mimeData->setUrls({ _url });
        drag->setMimeData(mimeData);

        QPixmap p(_preview);
        if (!p.isNull())
        {
            if (p.width() > Utils::scale_value(drag_preview_max_width))
                p = p.scaledToWidth(Utils::scale_value(drag_preview_max_width), Qt::SmoothTransformation);
            if (p.height() > Utils::scale_value(drag_preview_max_height))
                p = p.scaledToHeight(Utils::scale_value(drag_preview_max_height), Qt::SmoothTransformation);

            QPainter painter;
            painter.begin(&p);
            painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            QColor dragPreviewColor(ql1s("#000000"));
            dragPreviewColor.setAlphaF(0.5);
            painter.fillRect(p.rect(), dragPreviewColor);
            painter.end();

            drag->setPixmap(p);
        }

        bool result = drag->exec(Qt::CopyAction) == Qt::CopyAction;
        drag->deleteLater();
        return result;
    }

    StatsSender::StatsSender()
        : guiSettingsReceived_(false)
        , themeSettingsReceived_(false)
    {
        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::guiSettings, this, &StatsSender::recvGuiSettings);
        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::themeSettings, this, &StatsSender::recvThemeSettings);
    }

    StatsSender* getStatsSender()
    {
        static auto statsSender = std::make_shared<StatsSender>();
        return statsSender.get();
    }

    void StatsSender::trySendStats() const
    {
        if ((guiSettingsReceived_ || Ui::get_gui_settings()->getIsLoaded()) &&
            (themeSettingsReceived_ || Styling::getThemesContainer().isLoaded()))
        {
            Utils::post_stats_with_settings();
        }
    }

    bool haveText(const QMimeData * mimedata)
    {
        if (!mimedata)
            return false;

        if (mimedata->hasFormat(qsl("application/x-qt-windows-mime;value=\"Csv\"")))
            return true;

        if (mimedata->hasUrls())
        {
            const QList<QUrl> urlList = mimedata->urls();
            for (const auto& url : urlList)
            {
                if (url.isValid())
                    return false;
            }
        }

        const auto text = mimedata->text();
        QUrl url(text);

        return !text.isEmpty() && (!url.isValid() || url.host().isEmpty());
    }

    QStringRef normalizeLink(const QStringRef& _link)
    {
        return _link.trimmed();
    }

    const wchar_t* get_crossprocess_mutex_name()
    {
        return build::GetProductVariant(crossprocess_mutex_name_icq, crossprocess_mutex_name_agent, crossprocess_mutex_name_biz, crossprocess_mutex_name_dit);
    }

    template<typename T>
    static T initLayout(T layout)
    {
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        return layout;
    }

    QHBoxLayout* emptyHLayout(QWidget* parent)
    {
        return initLayout(new QHBoxLayout(parent));
    }

    QVBoxLayout* emptyVLayout(QWidget* parent)
    {
        return initLayout(new QVBoxLayout(parent));
    }

    QGridLayout* emptyGridLayout(QWidget* parent)
    {
        return initLayout(new QGridLayout(parent));
    }

    void emptyContentsMargins(QWidget* w)
    {
        w->setContentsMargins(0, 0, 0, 0);
    }

    void transparentBackgroundStylesheet(QWidget* w)
    {
        w->setStyleSheet(qsl("background: transparent;"));
    }

    void openMailBox(const QString& email, const QString& mrimKey, const QString& mailId)
    {
        core::tools::version_info infoCurrent;
        const auto buildStr = QString::number(infoCurrent.get_build());
        const auto langStr = Utils::GetTranslator()->getLang();

        QString url;

        if (!mailId.isEmpty())
        {
            url = getMailOpenUrl().arg(email, mrimKey, buildStr, langStr, msgIdFromUidl(mailId));
        }
        else
        {
            url = getMailUrl().arg(email, mrimKey, buildStr, langStr);
        }

        QDesktopServices::openUrl(url);

        emit Utils::InterConnector::instance().mailBoxOpened();
    }

    void openAgentUrl(
        const QString& _url,
        const QString& _fail_url,
        const QString& _email,
        const QString& _mrimKey)
    {
        core::tools::version_info infoCurrent;

        const QString signed_url = getBaseMailUrl() % redirect % ql1s("&page=") % _url % ql1s("&FailPage=") % _fail_url;

        const QString final_url = signed_url.arg(_email, _mrimKey, QString::number(infoCurrent.get_build()), Utils::GetTranslator()->getLang());

        QDesktopServices::openUrl(final_url);
    }


    QString getProductName()
    {
        return build::GetProductVariant(product_name_icq, product_name_agent, product_name_biz, product_name_dit);
    }

    QString getInstallerName()
    {
        return build::GetProductVariant(installer_exe_name_icq, installer_exe_name_agent, installer_exe_name_biz, installer_exe_name_dit);
    }

    QString getUnreadsBadgeStr(const int _unreads)
    {
        QString cnt;
        if (_unreads > 0)
        {
            if (_unreads < 1000)
                cnt = QString::number(_unreads);
            else if (_unreads < 10000)
                cnt = QString::number(_unreads / 1000) % ql1c('k');
            else
                cnt = qsl("9k+");
        }

        return cnt;
    }

    QSize getUnreadsBadgeSize(const int _unreads, const int _height)
    {
        const QString& unreadsString = getUnreadsBadgeStr(_unreads);

        const int symbolsCount = unreadsString.size();

        switch (symbolsCount)
        {
            case 1:
            {
                const QSize baseSize(Utils::scale_bitmap(Utils::scale_value(20)), Utils::scale_bitmap(Utils::scale_value(20)));

                return QSize(_height, _height);
            }
            case 2:
            {
                const QSize baseSize(Utils::scale_bitmap(Utils::scale_value(28)), Utils::scale_bitmap(Utils::scale_value(20)));

                const int badgeWidth = int(double(_height) * (double(baseSize.width()) / double(baseSize.height())));

                return QSize(badgeWidth, _height);
            }
            case 3:
            {
                const QSize baseSize(Utils::scale_bitmap(Utils::scale_value(32)), Utils::scale_bitmap(Utils::scale_value(20)));

                const int badgeWidth = int(double(_height) * (double(baseSize.width()) / double(baseSize.height())));

                return QSize(badgeWidth, _height);

            }
            default:
            {
                assert(false);

                return QSize(Utils::scale_bitmap(Utils::scale_value(20)), Utils::scale_bitmap(Utils::scale_value(20)));
            }
        }
    }

    QPixmap getUnreadsBadge(const int _unreads, const QColor _bgColor, const int _height)
    {
        const QString unreadsString = getUnreadsBadgeStr(_unreads);

        const int symbolsCount = unreadsString.size();

        switch (symbolsCount)
        {
            case 1:
            {
                const QSize baseSize(Utils::scale_bitmap(Utils::scale_value(20)), Utils::scale_bitmap(Utils::scale_value(20)));

                return Utils::renderSvg(qsl(":/controls/unreads_x_counter"), QSize(_height, _height), _bgColor);
            }
            case 2:
            {
                const QSize baseSize(Utils::scale_bitmap(Utils::scale_value(28)), Utils::scale_bitmap(Utils::scale_value(20)));

                const int badgeWidth = int(double(_height) * (double(baseSize.width()) / double(baseSize.height())));

                return Utils::renderSvg(qsl(":/controls/unreads_xx_counter"), QSize(badgeWidth, _height), _bgColor);
            }
            case 3:
            {
                const QSize baseSize(Utils::scale_bitmap(Utils::scale_value(32)), Utils::scale_bitmap(Utils::scale_value(20)));

                const int badgeWidth = int(double(_height) * (double(baseSize.width()) / double(baseSize.height())));

                return Utils::renderSvg(qsl(":/controls/unreads_xxx_counter"), QSize(badgeWidth, _height), _bgColor);

            }
            default:
            {
                assert(false);

                return QPixmap();
            }
        }
    }

    namespace Badge
    {
        namespace
        {
            QSize getSize(int _textLength)
            {
                switch (_textLength)
                {
                case 1:
                    return Utils::scale_value(QSize(18, 18));
                case 2:
                    return Utils::scale_value(QSize(22, 18));
                case 3:
                    return Utils::scale_value(QSize(26, 18));
                default:
                    assert(!"unsupported length");
                    return QSize();
                    break;
                }
            }

            constexpr int minTextSize() noexcept
            {
                return 1;
            }

            constexpr int maxTextSize() noexcept
            {
                return 3;
            }


            std::vector<QPixmap> generatePixmaps(QLatin1String _basePath, Color _color)
            {
                std::vector<QPixmap> result;
                result.reserve(maxTextSize());

                auto bgColor = Styling::getParameters().getColor((_color == Color::Red) ? Styling::StyleVariable::SECONDARY_ATTENTION : Styling::StyleVariable::PRIMARY);
                auto borderColor =  Styling::getParameters().getColor(Styling::StyleVariable::BASE_GLOBALWHITE);


                for (int i = minTextSize(); i <= maxTextSize(); ++i)
                    result.push_back(Utils::renderSvgLayered(_basePath + QString::number(i), {
                                                            { qsl("border"), borderColor },
                                                            { qsl("bg"), bgColor },
                                                        }));
                return result;
            }

            QPixmap getBadgeBackground(int _textLength, Color _color)
            {
                if (_textLength < minTextSize() || _textLength > maxTextSize())
                {
                    assert(!"unsupported length");
                    return QPixmap();
                }
                if (_color == Color::Red)
                {
                    static const std::vector<QPixmap> v = generatePixmaps(ql1s(":/controls/badge_"), Color::Red);
                    return v[_textLength - 1];
                }
                else
                {
                    static const std::vector<QPixmap> v = generatePixmaps(ql1s(":/controls/badge_"), Color::Green);
                    return v[_textLength - 1];
                }
            }
        }

        void drawBadge(const std::unique_ptr<Ui::TextRendering::TextUnit>& _textUnit, QPainter& _p, int _x, int _y, Color _color)
        {
            const auto textLength = _textUnit->getSourceText().size();

            if (textLength < minTextSize() || textLength > maxTextSize())
            {
                assert(!"unsupported length");
                return;
            }

            Utils::PainterSaver saver(_p);
            _textUnit->evaluateDesiredSize();
            const auto desiredWidth = _textUnit->desiredWidth();
            _p.setRenderHint(QPainter::Antialiasing);
            _p.setRenderHint(QPainter::TextAntialiasing);
            _p.setRenderHint(QPainter::SmoothPixmapTransform);
            const auto size = getSize(textLength);

            auto additionalTopOffset = 0; //!HACK until TextUnit fixed
            auto additionalLeftOffset = 0.4; //!HACK
            if (platform::is_linux() || platform::is_windows())
                additionalTopOffset = 1;

            _textUnit->setOffsets(_x + (size.width() - desiredWidth + 1) / 2 + additionalLeftOffset,
                                  _y + (size.height() + additionalTopOffset) / 2);
            _p.drawPixmap(_x, _y, getBadgeBackground(textLength, _color));
            _textUnit->draw(_p, Ui::TextRendering::VerPosition::MIDDLE);
        }
    }


    int drawUnreads(
        QPainter& _p,
        const QFont& _font,
        const QColor& _bgColor,
        const QColor& _textColor,
        const int _unreadsCount,
        const int _badgeHeight,
        const int _x,
        const int _y)
    {
        _p.save();

        QFontMetrics m(_font);

        const auto text = getUnreadsBadgeStr(_unreadsCount);

        const auto unreadsRect = m.tightBoundingRect(text);

        struct cachedBage
        {
            QColor color_;
            QSize size_;
            QPixmap badge_;

            cachedBage(const QColor& _color, const QSize& _size, const QPixmap& _badge)
                : color_(_color), size_(_size), badge_(_badge)
            {
            }
        };

        static std::vector<cachedBage> cachedBages;

        const QSize badgeSize = getUnreadsBadgeSize(_unreadsCount, _badgeHeight);

        const QPixmap* badge = nullptr;

        for (const cachedBage& _badge : cachedBages)
        {
            if (_bgColor == _badge.color_ && badgeSize == _badge.size_)
            {
                badge = &_badge.badge_;
            }
        }

        if (!badge)
        {
            cachedBages.emplace_back(_bgColor, badgeSize, getUnreadsBadge(_unreadsCount, _bgColor, _badgeHeight));

            badge = &(cachedBages[cachedBages.size() - 1].badge_);
        }

        _p.drawPixmap(_x, _y, *badge);

        _p.setFont(_font);
        _p.setPen(_textColor);

        const QChar firstChar = text[0];
        const QChar lastChar = text[text.size() - 1];
        const int unreadsWidth = (unreadsRect.width() + m.leftBearing(firstChar) + m.rightBearing(lastChar));
        const int unreadsHeight = unreadsRect.height();

        if constexpr (platform::is_apple())
        {
            _p.drawText(QRectF(_x, _y, badgeSize.width(), badgeSize.height()), text, QTextOption(Qt::AlignHCenter | Qt::AlignVCenter));
        }
        else
        {
            const float textX = floorf((float)_x + ((float)badgeSize.width() - (float)unreadsWidth) / 2.);
            const float textY = ceilf((float)_y + ((float)badgeSize.height() + (float)unreadsHeight) / 2.);

            _p.drawText(textX, textY, text);
        }

        _p.restore();

        return badgeSize.width();
    }

    QPixmap pixmapWithEllipse(const QPixmap& _original, const QColor& _brushColor, int brushWidth)
    {
        if (!brushWidth)
            return _original;

        QPixmap result(_original.width(),
                       _original.height());
        result.fill({ Qt::transparent });

        QPainter p(&result);
        p.setRenderHint(QPainter::HighQualityAntialiasing);

        p.drawPixmap(0, 0, _original.width(), _original.height(), _original);

        QPen pen(_brushColor);
        pen.setWidth(brushWidth);
        p.setPen(pen);

        auto rect = result.rect();
        rect.setLeft(rect.left() + brushWidth);
        rect.setTop(rect.top() + brushWidth);
        rect.setWidth(_original.width() - 2 * brushWidth);
        rect.setHeight(_original.height() - 2 * brushWidth);

        p.drawEllipse(rect);

        return result;
    }

    int drawUnreads(QPainter *p, const QFont &font, QColor bgColor, QColor textColor, QColor borderColor, int unreads, int balloonSize, int x, int y, QPainter::CompositionMode borderMode)
    {
        if (!p || unreads <= 0)
            return 0;

        QFontMetrics m(font);

        const auto text = getUnreadsBadgeStr(unreads);
        const auto unreadsRect = m.tightBoundingRect(text);
        const auto firstChar = text[0];
        const auto lastChar = text[text.size() - 1];
        const auto unreadsWidth = (unreadsRect.width() + m.leftBearing(firstChar) + m.rightBearing(lastChar));
        const auto unreadsHeight = unreadsRect.height();

        auto balloonWidth = unreadsWidth;
        const auto isLongText = (text.length() > 1);
        if (isLongText)
        {
            balloonWidth += m.height();
        }
        else
        {
            balloonWidth = balloonSize;
        }

        const auto balloonRadius = (balloonSize / 2);

        p->save();
        p->setPen(Qt::NoPen);
        p->setRenderHint(QPainter::HighQualityAntialiasing);

        auto mode = p->compositionMode();
        int borderWidth = 0;
        if (borderColor.isValid())
        {
            p->setCompositionMode(borderMode);
            p->setBrush(borderColor);
            p->setPen(borderColor);
            borderWidth = Utils::scale_value(1);
            p->drawRoundedRect(x - borderWidth, y - borderWidth, balloonWidth + borderWidth * 2, balloonSize + borderWidth * 2, balloonRadius, balloonRadius);
        }

        p->setCompositionMode(mode);
        p->setBrush(bgColor);
        p->drawRoundedRect(x, y, balloonWidth, balloonSize, balloonRadius, balloonRadius);

        p->setFont(font);
        p->setPen(textColor);
        if constexpr (platform::is_apple())
        {
            p->drawText(QRectF(x, y, balloonWidth, balloonSize), text, QTextOption(Qt::AlignCenter));
        }
        else
        {
            const float textX = floorf((float)x + ((float)balloonWidth - (float)unreadsWidth) / 2.);
            const float textY = ceilf((float)y + ((float)balloonSize + (float)unreadsHeight) / 2.);
            p->drawText(textX, textY, text);
        }
        p->restore();

        return balloonWidth;
    }

    QSize getUnreadsSize(const QFont& _font, const bool _withBorder, const int _unreads, const int _balloonSize)
    {
        const auto text = getUnreadsBadgeStr(_unreads);
        const QFontMetrics m(_font);
        const auto unreadsRect = m.tightBoundingRect(text);
        const auto firstChar = text[0];
        const auto lastChar = text[text.size() - 1];
        const auto unreadsWidth = (unreadsRect.width() + m.leftBearing(firstChar) + m.rightBearing(lastChar));

        auto balloonWidth = unreadsWidth;
        if (text.length() > 1)
        {
            balloonWidth += m.height();
        }
        else
        {
            balloonWidth = _balloonSize;
        }

        const auto borderWidth = _withBorder ? Utils::scale_value(2) : 0;
        return QSize(
            balloonWidth + borderWidth,
            _balloonSize + borderWidth
        );
    }

    QImage iconWithCounter(int size, int count, QColor bg, QColor fg, QImage back)
    {
        const QString cnt = getUnreadsBadgeStr(count);

        const auto isBackNull = back.isNull();

        QImage result = isBackNull ? QImage(size, size, QImage::Format_ARGB32) : std::move(back);
        int32_t cntSize = cnt.size();
        if (isBackNull)
            result.fill(Qt::transparent);

        {
            QPainter p(&result);
            p.setBrush(bg);
            p.setPen(Qt::NoPen);
            p.setRenderHint(QPainter::Antialiasing);
            p.setRenderHint(QPainter::TextAntialiasing);
            p.setRenderHint(QPainter::HighQualityAntialiasing);

            //don't try to understand it. just walk away. really.
            int32_t top = 0;
            int32_t radius = cntSize > 2 ? 6 : 8;
            int32_t shiftY = 0;
            int32_t shiftX = count > 10000 ? 1 : 0;
            int32_t left = 0;
            int32_t fontSize = 8;

            if (isBackNull)
            {
                fontSize = (cntSize < 3) ? 11 : 8;

                if (size == 16)
                {
                    shiftY = cntSize > 2 ? 2 : 0;
                    top = cntSize > 2 ? 4 : 0;
                }
                else
                {
                    shiftY = cntSize > 2 ? 4 : 0;
                    top = cntSize > 2 ? 8 : 0;
                }
            }
            else
            {
                if (size == 16)
                {
                    left = cntSize > 2 ? 0 : 4;
                    shiftY = cntSize > 2 ? 2 : 2;
                    top = 4;
                }
                else
                {
                    left = cntSize > 2 ? 0 : 8;
                    shiftY = cntSize > 2 ? 4 : 4;
                    top = 8;
                }
            }

            if (size > 16)
            {
                radius *= 2;
                fontSize *= 2;
                shiftX *= 2;
            }

            auto rect = QRect(left, top, size - left, size - top);
            p.drawRoundedRect(rect, radius, radius);

            auto f = (QSysInfo::WindowsVersion != QSysInfo::WV_WINDOWS10 &&  QSysInfo::WindowsVersion != QSysInfo::WV_XP)
                      ? Fonts::appFont(fontSize, Fonts::FontFamily::ROUNDED_MPLUS)
                      : Fonts::appFont(fontSize, Fonts::FontWeight::Normal);

            f.setStyleStrategy(QFont::PreferQuality);

            QFontMetrics m(f);
            const auto unreadsRect = m.tightBoundingRect(cnt);
            const auto firstChar = cnt[0];
            const auto lastChar = cnt[cnt.size() - 1];
            const auto unreadsWidth = (unreadsRect.width() + m.leftBearing(firstChar) + m.rightBearing(lastChar));
            const auto unreadsHeight = unreadsRect.height();
            const auto textX = floorf(float(size - unreadsWidth) / 2. + shiftX + left / 2.);
            const auto textY = ceilf(float(size + unreadsHeight) / 2. + shiftY);

            p.setFont(f);
            p.setPen(fg);
            p.drawText(textX, textY, cnt);
        }
        return result;
    }

    void openUrl(const QStringRef& _url)
    {
        if (Utils::isMentionLink(_url))
        {
            const auto aimId = _url.mid(_url.indexOf(ql1c('[')) + 1, _url.length() - 3);
            if (!aimId.isEmpty())
            {
                std::string resStr;

                switch (openDialogOrProfile(aimId.toString(), OpenDOPParam::aimid))
                {
                case OpenDOPResult::dialog:
                    resStr = "Chat_Opened";
                    break;
                case OpenDOPResult::profile:
                    resStr = "Profile_Opened";
                    break;
                case OpenDOPResult::chat_popup:
                    resStr = "Chat_Popup_Opened";
                    break;
                default:
                    break;
                }
                assert(!resStr.empty());
                Ui::GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::mentions_click_in_chat, { { "Result", std::move(resStr) } });
                return;
            }
        }

        QString url = _url.toString();

        auto command = UrlCommandBuilder::makeCommand(url, UrlCommandBuilder::Context::Internal);

        if (command->isValid())
            command->execute();
        else
            QDesktopServices::openUrl(url);
    }

    void openFileOrFolder(const QStringRef& _path, const OpenAt _openAt)
    {
        const auto dialog = Utils::InterConnector::instance().getContactDialog();
        const auto chatAimId = dialog ? dialog->currentAimId() : QString();
        const auto isPublic = !chatAimId.isEmpty() ? Logic::getContactListModel()->getContactItem(chatAimId)->is_public() : true;
        const auto isStranger = chatAimId.isEmpty()
                                ? true
                                : (Logic::getRecentsModel()->isStranger(chatAimId)
                                  || Logic::getRecentsModel()->isSuspicious(chatAimId));

        const auto openFile = dialog
                            && Features::opensOnClick()
                            && (_openAt == OpenAt::Launcher)
                            && !isPublic && !isStranger;
#ifdef _WIN32

        auto nativePath = QDir::toNativeSeparators(_path.toString());
        if (openFile)
            ShellExecute(0, L"open", nativePath.toStdWString().c_str(), 0, 0, SW_SHOWNORMAL);
        else
        {
            nativePath = nativePath.replace(ql1c('"'), qsl("\"\""));
            ShellExecute(0, 0, L"explorer", (qsl("/select,") + nativePath).toStdWString().c_str(), 0, SW_SHOWNORMAL);
        }
#else
#ifdef __APPLE__
        if (openFile)
            MacSupport::openFile(_path.toString());
        else
            MacSupport::openFinder(_path.toString());
#else
        if (openFile)
        {
            QDesktopServices::openUrl(qsl("file:///") + _path.toString());
        }
        else
        {
            QDir dir(_path.toString());
            dir.cdUp();
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
        }
#endif // __APPLE__
#endif //_WIN32
    }

    QString convertMentions(const QString& _source, const Data::MentionMap& _mentions)
    {
        if (_mentions.empty())
            return _source;

        QString result;

        static const auto mentionMarker = ql1s("@[");
        auto ndxStart = _source.indexOf(mentionMarker);
        if (ndxStart == -1)
            return _source;

        auto ndxEnd = -1;

        while (ndxStart != -1)
        {
            result += _source.midRef(ndxEnd + 1, ndxStart - (ndxEnd + 1));
            ndxEnd = _source.indexOf(ql1c(']'), ndxStart + 2);
            if (ndxEnd != -1)
            {
                const auto it = _mentions.find(_source.midRef(ndxStart + 2, ndxEnd - ndxStart - 2));
                if (it != _mentions.end())
                    result += it->second;
                else
                    result += _source.midRef(ndxStart, ndxEnd + 1 - ndxStart);

                ndxStart = _source.indexOf(mentionMarker, ndxEnd + 1);
            }
            else
            {
                result += _source.rightRef(_source.size() - ndxStart);
                return result;
            }
        }
        result += _source.rightRef(_source.size() - ndxEnd - 1);

        if (!result.isEmpty())
            return result;

        return _source;
    }

    bool isNick(const QStringRef& _text)
    {
        // match alphanumeric str with @, starting with a-zA-Z
        static const QRegularExpression re(qsl("^@[a-zA-Z0-9][a-zA-Z0-9._]*$"), QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::OptimizeOnFirstUsageOption);
        return re.match(_text).hasMatch();
    }

    QString makeNick(const QString& _text)
    {
        return _text.contains(ql1c('@')) ? _text : (ql1c('@') % _text);
    }

    bool isMentionLink(const QStringRef& _url)
    {
        return _url.startsWith(ql1s("@[")) && _url.endsWith(ql1c(']'));
    }

    bool isContainsMentionLink(const QStringRef& _text)
    {
        if (const auto idxStart = _text.indexOf(ql1s("@[")); idxStart >= 0)
        {
            const auto idxEnd = _text.indexOf(ql1c(']'), idxStart + 2);
            return idxEnd != -1;
        }
        return false;
    }

    OpenDOPResult openDialogOrProfile(const QString& _contact, const OpenDOPParam _paramType)
    {
        assert(!_contact.isEmpty());

        emit Utils::InterConnector::instance().closeAnySemitransparentWindow({ Utils::CloseWindowInfo::Initiator::Unknown, Utils::CloseWindowInfo::Reason::Keep_Sidebar });

        if (_paramType == OpenDOPParam::stamp)
        {
            emit Utils::InterConnector::instance().needJoinLiveChatByStamp(_contact);
            return OpenDOPResult::chat_popup;
        }
        else
        {
            if (!Logic::getContactListModel()->contains(_contact))
            {
                if (isChat(_contact))
                {
                    emit Utils::InterConnector::instance().needJoinLiveChatByAimId(_contact);
                    return OpenDOPResult::chat_popup;
                }
                else
                {
                    emit Utils::InterConnector::instance().profileSettingsShow(_contact);
                    return OpenDOPResult::profile;
                }
            }
            else
            {
                openDialogWithContact(_contact, -1, true);
                return OpenDOPResult::dialog;
            }
        }
    }

    void openDialogWithContact(const QString& _contact, qint64 _id, bool _sel)
    {
        Logic::GetFriendlyContainer()->updateFriendly(_contact);
        emit Utils::InterConnector::instance().addPageToDialogHistory(Logic::getContactListModel()->selectedContact());
        Logic::getContactListModel()->setCurrent(_contact, _id, _sel);
    }

    bool clicked(const QPoint& _prev, const QPoint& _cur, int dragDistance)
    {
        if (dragDistance == 0)
            dragDistance = 5;

        return std::abs(_cur.x() - _prev.x()) < dragDistance && std::abs(_cur.y() - _prev.y()) < dragDistance;
    }

    void drawBubbleRect(QPainter& _p, const QRectF& _rect, const QColor& _color, int _bWidth, int _bRadious)
    {
        Utils::PainterSaver saver(_p);

        auto pen = QPen(_color, _bWidth);
        _p.setPen(pen);
        auto r = _rect;
        auto diff = Utils::getScaleCoefficient() == 2 ? 1 : 0;
        r = QRect(r.x(), r.y() + 1, r.width(), r.height() - _bWidth - 1 + diff);

        _p.setRenderHint(QPainter::Antialiasing, false);
        _p.setRenderHint(QPainter::TextAntialiasing, false);

        auto add = (Utils::getScaleCoefficient() == 2 || Utils::is_mac_retina()) ? 0 : 1;
        _p.drawLine(QPoint(r.x() + _bRadious + _bWidth, r.y() - add), QPoint(r.x() - _bRadious + r.width() - _bWidth, r.y() - add));
        _p.drawLine(QPoint(r.x() + r.width(), r.y() + _bRadious + _bWidth), QPoint(r.x() + r.width(), r.y() + r.height() - _bRadious - _bWidth));
        _p.drawLine(QPoint(r.x() + _bRadious + _bWidth, r.y() + r.height()), QPoint(r.x() - _bRadious + r.width() - _bWidth, r.y() + r.height()));
        _p.drawLine(QPoint(r.x() - add, r.y() + _bRadious + _bWidth), QPoint(r.x() - add, r.y() + r.height() - _bRadious - _bWidth));

        _p.setRenderHint(QPainter::Antialiasing);
        _p.setRenderHint(QPainter::TextAntialiasing);

        QRectF rectangle(r.x(), r.y(), _bRadious * 2, _bRadious * 2);
        int stA = 90 * 16;
        int spA = 90 * 16;
        _p.drawArc(rectangle, stA, spA);

        rectangle = QRectF(r.x() - _bRadious * 2 + r.width(), r.y(), _bRadious * 2, _bRadious * 2);
        stA = 0 * 16;
        spA = 90 * 16;
        _p.drawArc(rectangle, stA, spA);

        rectangle = QRectF(r.x() + r.width() - _bRadious * 2, r.y() + r.height() - _bRadious * 2, _bRadious * 2, _bRadious * 2);
        stA = 0 * 16;
        spA = -90 * 16;
        _p.drawArc(rectangle, stA, spA);

        rectangle = QRectF(r.x(), r.y() + r.height() - _bRadious * 2, _bRadious * 2, _bRadious * 2);
        stA = 180 * 16;
        spA = 90 * 16;
        _p.drawArc(rectangle, stA, spA);
    }

    int getShadowMargin()
    {
        return Utils::scale_value(2);
    }

    void drawBubbleShadow(QPainter& _p, const QPainterPath& _bubble, const int _clipLength)
    {
        Utils::PainterSaver saver(_p);

        _p.setRenderHint(QPainter::Antialiasing);

        auto clip = _bubble.boundingRect();
        const auto clipLen = _clipLength == -1 ? Ui::MessageStyle::getBorderRadius() : _clipLength;
        clip.adjust(0, clip.height() - clipLen, 0, Utils::getShadowMargin());

        _p.setClipRect(clip);

        _p.fillPath(_bubble.translated(0, Utils::getShadowMargin()), QColor(0, 0, 0, 255 * 0.01));
        _p.fillPath(_bubble.translated(0, Utils::getShadowMargin() / 2), QColor(0, 0, 0, 255 * 0.08));
    }

    QSize avatarWithBadgeSize(const int _avatarWidthScaled)
    {
        auto& badges = getBadgesData();

        const auto it = badges.find(Utils::scale_bitmap(_avatarWidthScaled));
        if (it == badges.end())
            return QSize(_avatarWidthScaled, _avatarWidthScaled);

        const auto& badge = it->second;

        const auto w = std::max(_avatarWidthScaled, badge.offset_.x() + badge.size_.width());
        const auto h = std::max(_avatarWidthScaled, badge.offset_.y() + badge.size_.height());

        return QSize(w, h);
    }

    void drawAvatarWithBadge(QPainter& _p, const QPoint& _topLeft, const QPixmap& _avatar, const bool _isOfficial, const bool _isMuted, const bool _isSelected)
    {
        assert(!_avatar.isNull());

        auto& badges = getBadgesData();
        const auto it = badges.find(_avatar.width());
        if ((!_isMuted && !_isOfficial) || it == badges.end())
        {
            if ((_isMuted || _isOfficial) && it == badges.end())
                assert(!"unknown size");

            _p.drawPixmap(_topLeft, _avatar);
            return;
        }

        const auto& badge = it->second;
        const QRect badgeRect(badge.offset_, badge.size_);

        QPixmap cut = _avatar;
        {
            QPainter p(&cut);
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::transparent);
            p.setRenderHint(QPainter::Antialiasing);
            p.setCompositionMode(QPainter::CompositionMode_Source);

            const auto w = badge.cutWidth_;
            p.drawEllipse(badgeRect.adjusted(-w, -w, w, w));
        }
        _p.drawPixmap(_topLeft, cut);

        Utils::PainterSaver ps(_p);
        _p.setRenderHint(QPainter::Antialiasing);
        _p.setPen(Qt::NoPen);

        static const auto muted = Styling::getParameters().getColor(Styling::StyleVariable::BASE_TERTIARY);
        static const auto selMuted = Styling::getParameters().getColor(Styling::StyleVariable::PRIMARY);
        static const auto official = QColor(qsl("#2594ff"));
        const auto& badgeColor = _isMuted ? (_isSelected ? selMuted : muted) : official;
        _p.setBrush(badgeColor);
        _p.drawEllipse(badgeRect.translated(_topLeft));

        const auto& icon = _isMuted ? badge.muteIcon_ : badge.officialIcon_;
        if (!icon.isNull())
        {
            const auto ratio = Utils::scale_bitmap_ratio();
            const auto x = _topLeft.x() + badge.offset_.x() + (badge.size_.width() - icon.width() / ratio) / 2;
            const auto y = _topLeft.y() + badge.offset_.y() + (badge.size_.height() - icon.height() / ratio) / 2;

            _p.drawPixmap(x, y, icon);
        }
    }

    void drawAvatarWithBadge(QPainter& _p, const QPoint& _topLeft, const QPixmap& _avatar, const QString& _aimid, const bool _officialOnly, const bool _isSelected)
    {
        assert(!_avatar.isNull());

        const auto isMuted = _aimid.isEmpty() ? false : !_officialOnly && Logic::getContactListModel()->isMuted(_aimid);
        const auto isOfficial = _aimid.isEmpty() ? false : !isMuted && Logic::GetFriendlyContainer()->getOfficial(_aimid);
        drawAvatarWithBadge(_p, _topLeft, _avatar, isOfficial, isMuted, _isSelected);
    }

    void drawAvatarWithoutBadge(QPainter& _p, const QPoint& _topLeft, const QPixmap& _avatar)
    {
        assert(!_avatar.isNull());

        drawAvatarWithBadge(_p, _topLeft, _avatar, false, false, false);
    }

    QPixmap mirrorPixmap(const QPixmap& _pixmap, const MirrorDirection _direction)
    {
        const auto horScale = _direction & MirrorDirection::horizontal ? -1 : 1;
        const auto verScale = _direction & MirrorDirection::vertical ? -1 : 1;
        return _pixmap.transformed(QTransform().scale(horScale, verScale));
    }

    void drawUpdatePoint(QPainter &_p, const QPoint& _center, int _radius, int _borderRadius)
    {
        Utils::PainterSaver ps(_p);
        _p.setRenderHint(QPainter::HighQualityAntialiasing);
        _p.setPen(Qt::NoPen);

        if (_borderRadius)
        {
            _p.setBrush(Styling::getParameters().getColor(Styling::StyleVariable::BASE_GLOBALWHITE));
            _p.drawEllipse(_center, _borderRadius, _borderRadius);
        }

        _p.setBrush(Styling::getParameters().getColor(Styling::StyleVariable::PRIMARY));
        _p.drawEllipse(_center, _radius, _radius);
    }

    void restartApplication()
    {
#ifdef _WIN32
        auto mainWindow = Utils::InterConnector::instance().getMainWindow();
        if (mainWindow)
            ShowWindow(mainWindow->getParentWindow(), SW_HIDE);
#endif
        qApp->setProperty("restarting", true);
        qApp->quit();
    }

    bool isServiceLink(const QString &_url)
    {
        QUrl url(_url);
        if (url.isEmpty())
            return false;

        const QString scheme = url.scheme();
        const QString host = url.host();
        const QString path = url.path();

        return (scheme.compare(qsl(url_scheme_icq), Qt::CaseInsensitive) == 0 &&
                host.compare(qsl(url_command_service), Qt::CaseInsensitive) == 0 &&
                (path.startsWith(qsl("/").append(qsl(url_commandpath_service_getlogs)), Qt::CaseInsensitive) ||
                 path.startsWith(qsl("/").append(qsl(url_commandpath_service_getlogs_with_rtp)), Qt::CaseInsensitive)));
    }

    PhoneValidator::PhoneValidator(QWidget* _parent, bool _isCode)
        : QValidator(_parent)
        , isCode_(_isCode)
    {
    }

    QValidator::State PhoneValidator::validate(QString& s, int&) const
    {
        auto i = 0;
        for (const auto& ch : std::as_const(s))
        {
            if (!ch.isDigit() && !(isCode_ && i == 0 && ch == ql1c('+') ))
                return QValidator::Invalid;

            ++i;
        }

        return QValidator::Acceptable;
    }

    QString formatFileSize(const int64_t size)
    {
        assert(size >= 0);

        const auto KiB = 1024;
        const auto MiB = 1024 * KiB;
        const auto GiB = 1024 * MiB;

        if (size >= GiB)
        {
            const auto gibSize = ((double)size / (double)GiB);

            return qsl("%1 GB").arg(gibSize, 0, 'f', 1);
        }

        if (size >= MiB)
        {
            const auto mibSize = ((double)size / (double)MiB);

            return qsl("%1 MB").arg(mibSize, 0, 'f', 1);
        }

        if (size >= KiB)
        {
            const auto kibSize = ((double)size / (double)KiB);

            return qsl("%1 KB").arg(kibSize, 0, 'f', 1);
        }

        return qsl("%1 B").arg(size);
    }

    void logMessage(const QString &_message)
    {
        static QString logPath;
        static const QString folderName = qsl("gui_debug");
        static const QString fileName = qsl("logs.txt");
        if (logPath.isEmpty())
        {
            auto loc = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir folder(loc);
            folder.mkdir(folderName);
            folder.cd(folderName);
            logPath = folder.filePath(fileName);
        }

        QFile f(logPath);
        f.open(QIODevice::Append);
        f.write("\n");
        f.write(QDateTime::currentDateTime().toString().toUtf8());
        f.write("\n");
        f.write(_message.toUtf8());
        f.write("\n");
        f.flush();
    }

#ifdef __linux__

    bool runCommand(const QByteArray &command)
    {
        auto result = system(command.constData());
        if (result)
        {
            qCDebug(linuxRunCommand) << QString(qsl("Command failed, code: %1, command (in utf8): %2")).arg(result).arg(QString::fromLatin1(command.constData()));
            return false;
        }
        else
        {
            qCDebug(linuxRunCommand) << QString(qsl("Command succeeded, command (in utf8): %2")).arg(QString::fromLatin1(command.constData()));
            return true;
        }
    }

    QByteArray escapeShell(const QByteArray &content)
    {
        auto result = QByteArray();

        auto b = content.constData(), e = content.constEnd();
        for (auto ch = b; ch != e; ++ch)
        {
            if (*ch == ' ' || *ch == '"' || *ch == '\'' || *ch == '\\')
            {
                if (result.isEmpty())
                    result.reserve(content.size() * 2);
                if (ch > b)
                    result.append(b, ch - b);

                result.append('\\');
                b = ch;
            }
        }
        if (result.isEmpty())
            return content;

        if (e > b)
            result.append(b, e - b);

        return result;
    }

#endif

    void registerCustomScheme()
    {
#ifdef __linux__

        const QString home = QDir::homePath() + ql1c('/');
        const auto executableInfo = QFileInfo(QCoreApplication::applicationFilePath());
        const QString executableName = executableInfo.fileName();
        const QString executableDir = executableInfo.absoluteDir().absolutePath() + ql1c('/');

        QByteArray urlSchemeName = url_scheme_icq;
        if (build::is_biz())
            urlSchemeName = url_scheme_biz;
        else if (build::is_dit())
            urlSchemeName = url_scheme_dit;
        else if (build::is_agent())
            urlSchemeName = url_scheme_agent;

        if (QDir(home + ql1s(".local/")).exists())
        {
            const QString apps = home + ql1s(".local/share/applications/");
            const QString icons = home + ql1s(".local/share/icons/");

            if (!QDir(apps).exists())
                QDir().mkpath(apps);

            if (!QDir(icons).exists())
                QDir().mkpath(icons);

            const QString desktopFileName =  executableName + ql1s("desktop.desktop");

            const auto path = executableDir;
            const QString file = path + desktopFileName;
            QDir().mkpath(path);
            QFile f(file);
            if (f.open(QIODevice::WriteOnly))
            {
                const QString icon = icons % executableName % ql1s(".png");

                QFile(icon).remove();

                const auto iconName = build::GetProductVariant(qsl(":/logo/logo_icq"), qsl(":/logo/logo_agent"), qsl(":/logo/logo_biz"), qsl(":/logo/logo_dit"));
                const auto image = Utils::renderSvg(iconName, (QSize(256, 256))).toImage();
                image.save(icon);

                QFile(iconName).copy(icon);

                const auto appName = getAppTitle();

                QTextStream s(&f);
                s.setCodec("UTF-8");
                s << "[Desktop Entry]\n";
                s << "Version=1.0\n";
                s << "Name=" << appName << '\n';
                s << "Comment=" << QT_TRANSLATE_NOOP("linux_desktop_file", "Official desktop application for the %1 messaging service").arg(appName) << '\n';
                s << "TryExec=" << escapeShell(QFile::encodeName(executableDir + executableName)) << '\n';
                s << "Exec=" << escapeShell(QFile::encodeName(executableDir + executableName)) << " -urlcommand %u\n";
                s << "Icon=" << escapeShell(QFile::encodeName(icon)) << '\n';
                s << "Terminal=false\n";
                s << "Type=Application\n";
                s << "StartupWMClass=" << executableName << '\n';
                s << "Categories=Network;InstantMessaging;Qt;\n";
                s << "MimeType=x-scheme-handler/" << urlSchemeName << ";\n";
                f.close();

                if (runCommand("desktop-file-install --dir=" % escapeShell(QFile::encodeName(home + ql1s(".local/share/applications"))) % " --delete-original " % escapeShell(QFile::encodeName(file))))
                {
                    runCommand("update-desktop-database " % escapeShell(QFile::encodeName(home % ql1s(".local/share/applications"))));
                    runCommand("xdg-mime default " % desktopFileName.toLatin1() % " x-scheme-handler/" % urlSchemeName);
                }
                else // desktop-file-utils not installed, copy by hands
                {
                    if (QFile(file).copy(home % ql1s(".local/share/applications/") % desktopFileName))
                        runCommand("xdg-mime default " % desktopFileName.toLatin1() % " x-scheme-handler/" % urlSchemeName);
                    QFile(file).remove();
                }
            }
        }

        if (runCommand("gconftool-2 -t string -s /desktop/gnome/url-handlers/" % urlSchemeName % "/command " % escapeShell(escapeShell(QFile::encodeName(executableDir + executableName)) % " -urlcommand %s")))
        {
            runCommand("gconftool-2 -t bool -s /desktop/gnome/url-handlers/" % urlSchemeName % "/needs_terminal false");
            runCommand("gconftool-2 -t bool -s /desktop/gnome/url-handlers/" % urlSchemeName % "/enabled true");
        }

        QString services;
        if (QDir(home + ql1s(".kde4/")).exists())
        {
            services = home + ql1s(".kde4/share/kde4/services/");
        }
        else if (QDir(home + ql1s(".kde/")).exists())
        {
            services = home + ql1s(".kde/share/kde4/services/");
        }

        if (!services.isEmpty())
        {
            if (!QDir(services).exists())
                QDir().mkpath(services);

            const auto path = services;
            const QString file = path % executableName % ql1s(".protocol");
            QFile f(file);
            if (f.open(QIODevice::WriteOnly))
            {
                QTextStream s(&f);
                s.setCodec("UTF-8");
                s << "[Protocol]\n";
                s << "exec=" << QFile::decodeName(escapeShell(QFile::encodeName(executableDir + executableName))) << " -urlcommand %u\n";
                s << "protocol=" << urlSchemeName << '\n';
                s << "input=none\n";
                s << "output=none\n";
                s << "helper=true\n";
                s << "listing=false\n";
                s << "reading=false\n";
                s << "writing=false\n";
                s << "makedir=false\n";
                s << "deleting=false\n";
                f.close();
            }
        }

#endif

    }

    void openFileLocation(const QString& _path)
    {
#ifdef _WIN32
        const QString command = ql1s("explorer /select,") + QDir::toNativeSeparators(_path);
        QProcess::startDetached(command);
#else
#ifdef __APPLE__
        MacSupport::openFinder(_path);
#else
        QDir dir(_path);
        dir.cdUp();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
#endif // __APPLE__
#endif //_WIN32
    }

    bool isPanoramic(const QSize& _size)
    {
        if (_size.height() == 0)
            return false;

        const auto ratio = (double)_size.width() / _size.height();
        constexpr double oversizeCoeff = 2.0;
        return ratio >= oversizeCoeff || ratio <= 1. / oversizeCoeff;
    }

    bool isMimeDataWithImageDataURI(const QMimeData* _mimeData)
    {
        return _mimeData->text().startsWith(qsl("data:image/"));
    }

    bool isMimeDataWithImage(const QMimeData* _mimeData)
    {
        return _mimeData->hasImage() || isMimeDataWithImageDataURI(_mimeData);
    }

    QImage getImageFromMimeData(const QMimeData* _mimeData)
    {
        QImage result;

        if (_mimeData->hasImage())
        {
            result = qvariant_cast<QImage>(_mimeData->imageData());
        }
        else if (isMimeDataWithImageDataURI(_mimeData))
        {
            auto plainText = _mimeData->text();

            plainText.remove(0, plainText.indexOf(ql1c(',')) + 1);
            auto imageData = QByteArray::fromBase64(std::move(plainText).toUtf8());

            result = QImage::fromData(imageData);
        }

        return result;
    }

    bool startsCyrillic(const QString& _str)
    {
        return !_str.isEmpty() && _str[0] >= 0x0400 && _str[0] <= 0x04ff;
    }

    bool startsNotLetter(const QString& _str)
    {
        return !_str.isEmpty() && !_str[0].isLetter();
    }

    QPixmap tintImage(const QPixmap& _source, const QColor& _tint)
    {
        QPixmap res;
        if (_tint.isValid() && _tint.alpha())
        {
            constexpr auto fullyOpaque = 255;
            assert(_tint.alpha() > 0 && _tint.alpha() < fullyOpaque);

            QPixmap pm(_source.size());
            QPainter p(&pm);

            if (_tint.alpha() != fullyOpaque)
                p.drawPixmap(QPoint(), _source);

            p.fillRect(pm.rect(), _tint);
            res = std::move(pm);
        }
        else
        {
            res = _source;
        }

        Utils::check_pixel_ratio(res);
        return res;
    }

    bool isChat(const QString& _aimid)
    {
        return _aimid.endsWith(ql1s("@chat.agent"));
    }

    QString getDomainUrl()
    {
        return build::is_icq() ? Features::getProfileDomain() : Features::getProfileDomainAgent();
    }
}

void Logic::updatePlaceholders(const std::vector<Placeholder>& _locations)
{
    const auto recentsModel = Logic::getRecentsModel();
    const auto contactListModel = Logic::getContactListModel();
    const auto unknownsModel = Logic::getUnknownsModel();

    for (auto & location : _locations)
    {
        if (location == Placeholder::Contacts)
        {
            const auto needContactListPlaceholder = contactListModel->contactsCount() == 0;
            if (needContactListPlaceholder)
                emit Utils::InterConnector::instance().showContactListPlaceholder();
            else
                emit Utils::InterConnector::instance().hideContactListPlaceholder();
        }
        else if (location == Placeholder::Recents)
        {
            const auto needRecentsPlaceholder = recentsModel->dialogsCount() == 0 && unknownsModel->itemsCount() == 0;
            if (needRecentsPlaceholder)
                emit Utils::InterConnector::instance().showRecentsPlaceholder();
            else
                emit Utils::InterConnector::instance().hideRecentsPlaceholder();
        }
        else if (location == Placeholder::Dialog)
        {
            const auto needDialogPlaceholder = contactListModel->contactsCount() == 0 && unknownsModel->itemsCount() == 0;
            if (needDialogPlaceholder)
                emit Utils::InterConnector::instance().showDialogPlaceholder();
            else
                emit Utils::InterConnector::instance().hideDialogPlaceholder();
        }
    }
}
