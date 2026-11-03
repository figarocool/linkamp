#include "winampskin.h"
#include "linkampslider.h"

#include <QApplication>
#include <QDirIterator>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QWidget>

#include <archive.h>
#include <archive_entry.h>

namespace {
QString normalizedName(QString name)
{
    name = name.toLower().trimmed();
    const int slash = name.lastIndexOf(QLatin1Char('/'));
    if (slash >= 0)
        name = name.mid(slash + 1);
    const int backslash = name.lastIndexOf(QLatin1Char('\\'));
    if (backslash >= 0)
        name = name.mid(backslash + 1);
    return name;
}

struct SpriteSlice {
    const char *sheet;
    QRect rect;
};

QHash<QString, SpriteSlice> spriteAliases()
{
    return {
        {"prev.png", {"cbuttons", QRect(0, 0, 23, 18)}},
        {"prev_p.png", {"cbuttons", QRect(0, 18, 23, 18)}},
        {"play.png", {"cbuttons", QRect(23, 0, 23, 18)}},
        {"play_p.png", {"cbuttons", QRect(23, 18, 23, 18)}},
        {"pause.png", {"cbuttons", QRect(46, 0, 23, 18)}},
        {"pause_p.png", {"cbuttons", QRect(46, 18, 23, 18)}},
        {"stop.png", {"cbuttons", QRect(69, 0, 23, 18)}},
        {"stop_p.png", {"cbuttons", QRect(69, 18, 23, 18)}},
        {"next.png", {"cbuttons", QRect(92, 0, 23, 18)}},
        {"next_p.png", {"cbuttons", QRect(92, 18, 22, 18)}},
        {"open.png", {"cbuttons", QRect(114, 0, 22, 16)}},
        {"open_p.png", {"cbuttons", QRect(114, 16, 22, 16)}},
        {"posHandle.png", {"posbar", QRect(248, 0, 29, 10)}},
        {"posHandle_p.png", {"posbar", QRect(278, 0, 29, 10)}},
        {"volumeHandle.png", {"volume", QRect(15, 422, 14, 11)}},
        {"volumeHandle_p.png", {"volume", QRect(0, 422, 14, 11)}},
        {"balanceHandle.png", {"balance", QRect(15, 422, 14, 11)}},
        {"balanceHandle_p.png", {"balance", QRect(0, 422, 14, 11)}},
        {"repeat_off.png", {"shufrep", QRect(0, 0, 28, 15)}},
        {"repeat_off_p.png", {"shufrep", QRect(0, 15, 28, 15)}},
        {"repeat_on.png", {"shufrep", QRect(0, 30, 28, 15)}},
        {"repeat_on_p.png", {"shufrep", QRect(0, 45, 28, 15)}},
        {"shuffle_off.png", {"shufrep", QRect(28, 0, 47, 15)}},
        {"shuffle_off_p.png", {"shufrep", QRect(28, 15, 47, 15)}},
        {"shuffle_on.png", {"shufrep", QRect(28, 30, 47, 15)}},
        {"shuffle_on_p.png", {"shufrep", QRect(28, 45, 47, 15)}},
        {"eq_off.png", {"shufrep", QRect(0, 61, 23, 12)}},
        {"eq_off_p.png", {"shufrep", QRect(46, 61, 23, 12)}},
        {"pl_off.png", {"shufrep", QRect(23, 61, 23, 12)}},
        {"pl_off_p.png", {"shufrep", QRect(69, 61, 23, 12)}},
        {"eq_on.png", {"shufrep", QRect(0, 73, 23, 12)}},
        {"eq_on_p.png", {"shufrep", QRect(46, 73, 23, 12)}},
        {"pl_on.png", {"shufrep", QRect(23, 73, 23, 12)}},
        {"pl_on_p.png", {"shufrep", QRect(69, 73, 23, 12)}},
        {"windowClose.png", {"titlebar", QRect(18, 0, 9, 9)}},
        {"windowMinimize.png", {"titlebar", QRect(9, 0, 9, 9)}},
        {"windowMaximize.png", {"titlebar", QRect(0, 18, 9, 9)}},
        {"classic-menu.png", {"titlebar", QRect(0, 0, 9, 9)}},
        {"mono_active.png", {"monoster", QRect(29, 0, 27, 12)}},
        {"mono_inactive.png", {"monoster", QRect(29, 12, 27, 12)}},
        {"stereo_active.png", {"monoster", QRect(0, 0, 29, 12)}},
        {"stereo_inactive.png", {"monoster", QRect(0, 12, 29, 12)}}
    };
}

QString builtInSheetAlias(const QString &alias)
{
    if (alias == QLatin1String("classic-main.png")) return QStringLiteral("classic-main");
    if (alias == QLatin1String("classic-titlebar.png")) return QStringLiteral("classic-titlebar");
    if (alias == QLatin1String("classic-pledit.png")) return QStringLiteral("classic-pledit");
    if (alias == QLatin1String("classic-eqmain.png")) return QStringLiteral("classic-eqmain");
    if (alias == QLatin1String("classic-text.png")) return QStringLiteral("classic-text");
    return {};
}
}

WinampSkin &WinampSkin::instance()
{
    static WinampSkin skin;
    return skin;
}

bool WinampSkin::load(const QString &filePath, QString *error)
{
    QHash<QString, QImage> sheets;
    const QFileInfo info(filePath);
    const bool loaded = info.isDir()
        ? loadDirectory(filePath, &sheets, error)
        : loadArchive(filePath, &sheets, error);
    if (!loaded)
        return false;

    m_sheets = std::move(sheets);
    m_filePath = QFileInfo(filePath).absoluteFilePath();
    buildAssets();
    QSettings().setValue(QStringLiteral("skin/classicWsz"), m_filePath);
    refreshWidgetStyles();
    return true;
}

bool WinampSkin::loadDirectory(const QString &directoryPath, QHash<QString, QImage> *sheets,
                               QString *error) const
{
    static const QSet<QString> supported = {
        "main.bmp", "titlebar.bmp", "cbuttons.bmp", "playpaus.bmp", "monoster.bmp",
        "posbar.bmp", "volume.bmp", "balance.bmp", "shufrep.bmp", "eqmain.bmp",
        "pledit.bmp", "text.bmp", "numbers.bmp", "nums_ex.bmp"
    };
    QHash<QString, QImage> loaded;
    qint64 totalBytes = 0;
    QDirIterator it(directoryPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        const QString filename = normalizedName(info.fileName());
        if (!supported.contains(filename) || info.size() <= 0 || info.size() > 16 * 1024 * 1024)
            continue;
        totalBytes += info.size();
        if (totalBytes > 64 * 1024 * 1024)
            break;
        QImageReader reader(path);
        reader.setAutoTransform(false);
        const QImage image = reader.read();
        if (!image.isNull())
            loaded.insert(filename.left(filename.lastIndexOf(QLatin1Char('.'))), image);
    }

    if (!loaded.contains(QStringLiteral("main"))) {
        if (error) *error = QObject::tr("La cartella non contiene un MAIN.BMP valido.");
        return false;
    }
    *sheets = std::move(loaded);
    return true;
}

void WinampSkin::reset()
{
    m_filePath.clear();
    m_sheets.clear();
    m_assetPaths.clear();
    m_assetDirectory.reset();
    QSettings().remove(QStringLiteral("skin/classicWsz"));
    refreshWidgetStyles();
}

bool WinampSkin::loadArchive(const QString &filePath, QHash<QString, QImage> *sheets,
                             QString *error) const
{
    const QFileInfo info(filePath);
    if (!info.isFile() || info.size() <= 0 || info.size() > 64 * 1024 * 1024) {
        if (error) *error = QObject::tr("Il file skin è vuoto o supera 64 MB.");
        return false;
    }

    static const QSet<QString> supported = {
        "main.bmp", "titlebar.bmp", "cbuttons.bmp", "playpaus.bmp", "monoster.bmp",
        "posbar.bmp", "volume.bmp", "balance.bmp", "shufrep.bmp", "eqmain.bmp",
        "pledit.bmp", "text.bmp", "numbers.bmp", "nums_ex.bmp"
    };
    struct archive *reader = archive_read_new();
    archive_read_support_filter_all(reader);
    archive_read_support_format_zip(reader);
    const QByteArray path = QFile::encodeName(filePath);
    if (archive_read_open_filename(reader, path.constData(), 64 * 1024) != ARCHIVE_OK) {
        if (error) *error = QObject::tr("Non riesco ad aprire il pacchetto .wsz.");
        archive_read_free(reader);
        return false;
    }

    QHash<QString, QImage> loaded;
    qint64 totalBytes = 0;
    archive_entry *entry = nullptr;
    while (archive_read_next_header(reader, &entry) == ARCHIVE_OK) {
        const QString name = normalizedName(QString::fromUtf8(archive_entry_pathname(entry)
                                                                  ? archive_entry_pathname(entry) : ""));
        const qint64 expectedSize = archive_entry_size(entry);
        if (archive_entry_filetype(entry) != AE_IFREG || !supported.contains(name)
            || expectedSize <= 0 || expectedSize > 16 * 1024 * 1024) {
            archive_read_data_skip(reader);
            continue;
        }

        QByteArray bytes;
        bytes.reserve(static_cast<int>(expectedSize));
        char buffer[8192];
        qint64 entryBytes = 0;
        la_ssize_t count = 0;
        while ((count = archive_read_data(reader, buffer, sizeof(buffer))) > 0) {
            entryBytes += count;
            if (entryBytes > 16 * 1024 * 1024 || totalBytes + entryBytes > 64 * 1024 * 1024)
                break;
            bytes.append(buffer, static_cast<qsizetype>(count));
        }
        if (count < 0 || entryBytes > 16 * 1024 * 1024 || totalBytes + entryBytes > 64 * 1024 * 1024)
            continue;
        totalBytes += entryBytes;
        QImage image;
        if (image.loadFromData(bytes))
            loaded.insert(name.left(name.lastIndexOf(QLatin1Char('.'))), image);
    }
    const bool archiveOk = archive_errno(reader) == 0 || archive_errno(reader) == ARCHIVE_EOF;
    archive_read_close(reader);
    archive_read_free(reader);

    if (!archiveOk && loaded.isEmpty()) {
        if (error) *error = QObject::tr("Il pacchetto .wsz non è valido o non è supportato.");
        return false;
    }
    if (!loaded.contains(QStringLiteral("main")) || loaded.value(QStringLiteral("main")).isNull()) {
        if (error) *error = QObject::tr("Il pacchetto non contiene un MAIN.BMP valido.");
        return false;
    }
    *sheets = std::move(loaded);
    return true;
}

QImage WinampSkin::sheet(const QString &name) const
{
    const QString key = normalizedName(name);
    if (m_sheets.contains(key))
        return m_sheets.value(key);

    static const QHash<QString, QString> embeddedSheets = {
        {"main", ":/assets/classic-main.png"},
        {"classic-main", ":/assets/classic-main.png"},
        {"titlebar", ":/assets/classic-titlebar.png"},
        {"classic-titlebar", ":/assets/classic-titlebar.png"},
        {"titlebar-atlas", ":/assets/classic-titlebar-atlas.png"},
        {"pledit", ":/assets/classic-pledit.png"},
        {"classic-pledit", ":/assets/classic-pledit.png"},
        {"eqmain", ":/assets/classic-eqmain.png"},
        {"classic-eqmain", ":/assets/classic-eqmain.png"},
        {"text", ":/assets/classic-text.png"},
        {"classic-text", ":/assets/classic-text.png"},
        {"monoster", ":/assets/classic-monoster.png"},
        {"volume", ":/assets/classic-volume.png"},
        {"balance", ":/assets/classic-balance.png"}
    };
    const QString resource = embeddedSheets.value(key);
    return resource.isEmpty() ? QImage() : QImage(resource);
}

bool WinampSkin::hasSheet(const QString &name) const
{
    return m_sheets.contains(normalizedName(name));
}

QString WinampSkin::writeImage(const QString &name, const QImage &image)
{
    if (image.isNull() || !m_assetDirectory || !m_assetDirectory->isValid())
        return {};
    const QString path = m_assetDirectory->filePath(name);
    if (!image.save(path, "PNG"))
        return {};
    m_assetPaths.insert(name, path);
    return path;
}

void WinampSkin::buildAssets()
{
    const QString tempTemplate = QDir(QDir::tempPath()).filePath(QStringLiteral("linkamp-skin-XXXXXX"));
    m_assetDirectory.reset(new QTemporaryDir(tempTemplate));
    m_assetPaths.clear();

    const QImage titlebar = sheet(QStringLiteral("titlebar"));
    if (!titlebar.isNull() && titlebar.width() >= 302 && titlebar.height() >= 29) {
        QImage normalized(275, 28, QImage::Format_ARGB32);
        QPainter painter(&normalized);
        painter.drawImage(QRect(0, 0, 275, 14), titlebar, QRect(27, 0, 275, 14));
        painter.drawImage(QRect(0, 14, 275, 14), titlebar, QRect(27, 15, 275, 14));
        m_sheets.insert(QStringLiteral("classic-titlebar"), normalized);
    }
    const QList<QPair<QString, QString>> sheetAliases = {
        {"main", "classic-main.png"}, {"pledit", "classic-pledit.png"},
        {"eqmain", "classic-eqmain.png"}, {"text", "classic-text.png"}
    };
    for (const auto &item : sheetAliases) {
        const QImage image = sheet(item.first);
        if (!image.isNull())
            writeImage(item.second, image);
    }
    if (!m_sheets.value(QStringLiteral("classic-titlebar")).isNull())
        writeImage(QStringLiteral("classic-titlebar.png"), m_sheets.value(QStringLiteral("classic-titlebar")));

    const auto aliases = spriteAliases();
    for (auto it = aliases.cbegin(); it != aliases.cend(); ++it) {
        const QImage source = sheet(QString::fromLatin1(it.value().sheet));
        if (!source.isNull() && source.rect().contains(it.value().rect))
            writeImage(it.key(), source.copy(it.value().rect));
    }
    for (int position = 0; position < 28; ++position) {
        const QImage volume = sheet(QStringLiteral("volume"));
        const QImage balance = sheet(QStringLiteral("balance"));
        const QRect volumeFrame(0, position * 15, 68, 13);
        const QRect balanceFrame(9, position * 15, 38, 13);
        if (!volume.isNull() && volume.rect().contains(volumeFrame))
            writeImage(QStringLiteral("volume-track-%1.png").arg(position), volume.copy(volumeFrame));
        if (!balance.isNull() && balance.rect().contains(balanceFrame))
            writeImage(QStringLiteral("balance-track-%1.png").arg(position), balance.copy(balanceFrame));
    }
    const QImage main = sheet(QStringLiteral("main"));
    if (!main.isNull() && main.rect().contains(QRect(12, 23, 91, 41)))
        writeImage(QStringLiteral("visualizationBackground.png"), main.copy(12, 23, 91, 41));
}

QString WinampSkin::assetPath(const QString &alias) const
{
    return m_assetPaths.value(normalizedName(alias));
}

QString WinampSkin::applyToStylesheet(const QString &styleSheet) const
{
    static const QRegularExpression resourcePattern(QStringLiteral(":/?assets/([A-Za-z0-9_.-]+)"));
    QString result = styleSheet;
    auto it = resourcePattern.globalMatch(styleSheet);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString localPath = assetPath(match.captured(1));
        if (!localPath.isEmpty())
            result.replace(match.captured(0), localPath);
    }

    const QStringList sheetNames = {"classic-main.png", "classic-titlebar.png", "classic-pledit.png",
                                    "classic-eqmain.png", "classic-text.png"};
    for (const QString &alias : sheetNames) {
        const QString localPath = assetPath(alias);
        if (!localPath.isEmpty()) {
            result.replace(QStringLiteral(":/assets/") + alias, localPath);
            result.replace(QStringLiteral(":assets/") + alias, localPath);
        }
    }
    return result;
}

void WinampSkin::refreshWidgetStyles()
{
    if (!qApp)
        return;
    for (QWidget *widget : QApplication::allWidgets()) {
        const QByteArray propertyName("_linkampBaseStyleSheet");
        auto *slider = qobject_cast<LinkampSlider *>(widget);
        if (slider && !slider->baseStyleSheet().isEmpty())
            widget->setProperty(propertyName.constData(), slider->baseStyleSheet());
        if (!widget->property(propertyName.constData()).isValid())
            widget->setProperty(propertyName.constData(), widget->styleSheet());
        const QString base = widget->property(propertyName.constData()).toString();
        widget->setStyleSheet(applyToStylesheet(base));
        if (auto *label = qobject_cast<QLabel *>(widget)) {
            const QString objectName = label->objectName();
            if (objectName == QLatin1String("classicVisualModeLabels")) {
                QImage atlas = sheet(QStringLiteral("titlebar"));
                if (atlas.isNull() || atlas.width() < 312 || atlas.height() < 43)
                    atlas = sheet(QStringLiteral("titlebar-atlas"));
                if (!atlas.isNull() && atlas.width() >= 312 && atlas.height() >= 43)
                    label->setPixmap(QPixmap::fromImage(atlas.copy(304, 0, 8, 43)));
            }
        }
        if (slider)
            slider->refreshSkin();
        widget->update();
    }
}
