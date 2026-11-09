#include "util.h"
#include "qdatetime.h"
#include "qfileinfo.h"

#include <QRegularExpression>
#include <QStringDecoder>
#include <QUrlQuery>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>

QMediaMetaData parseMetaData(const QUrl &url)
{
    QMediaMetaData metadata;
    TagLib::FileRef f(url.toLocalFile().toLocal8Bit().data());

    if(!f.isNull() && f.tag()) {
        TagLib::Tag *tag = f.tag();

        QString title = QString::fromStdString(tag->title().toCString(true));

        // If title is not set, use the file name
        if(!title.length()) {
            title = url.fileName();
        }

        QString albumTitle = QString::fromStdString(tag->album().toCString(true));
        QString artist = QString::fromStdString(tag->artist().toCString(true));
        QString comment = QString::fromStdString(tag->comment().toCString(true));
        QString genre = QString::fromStdString(tag->genre().toCString(true));
        qint64 track = tag->track();
        qint64 year = tag->year();

        metadata = QMediaMetaData{};
        metadata.insert(QMediaMetaData::Title, title);
        metadata.insert(QMediaMetaData::AlbumTitle, albumTitle);
        metadata.insert(QMediaMetaData::AlbumArtist, artist);
        metadata.insert(QMediaMetaData::Comment, comment);
        metadata.insert(QMediaMetaData::Genre, genre);
        metadata.insert(QMediaMetaData::TrackNumber, track);
        metadata.insert(QMediaMetaData::Url, url);
        metadata.insert(QMediaMetaData::Date, year);
    }

    if(!f.isNull() && f.audioProperties()) {
        TagLib::AudioProperties *properties = f.audioProperties();

        qint64 duration = properties->lengthInMilliseconds();
        qint64 bitrate = properties->bitrate() * 1000;
        qint64 sampleRate = properties->sampleRate();

        metadata.insert(QMediaMetaData::AudioBitRate, bitrate);
        metadata.insert(QMediaMetaData::Comment, QString::number(sampleRate)); // Using Comment as sample rate
        metadata.insert(QMediaMetaData::Duration, duration);
    }

    return metadata;
}

static int mojibakeMarkerCount(const QString &text)
{
    return text.count(QChar(0x00c3)) + text.count(QChar(0x00c2))
        + text.count(QStringLiteral("â€"));
}

QString repairUtf8Mojibake(QString text)
{
    for (int pass = 0; pass < 12; ++pass) {
        const int currentMarkers = mojibakeMarkerCount(text);
        if (currentMarkers == 0)
            break;

        const QByteArray bytes = text.toLatin1();
        QStringDecoder decoder(QStringConverter::Utf8);
        const QString candidate = decoder(bytes);
        if (decoder.hasError() || mojibakeMarkerCount(candidate) >= currentMarkers)
            break;
        text = candidate;
    }
    return text;
}

QString formatDuration(qint64 ms)
{
    //  Calculate duration
    qint64 duration = ms/1000;
    QTime totalTime((duration / 3600) % 60, (duration / 60) % 60, duration % 60,
                    (duration * 1000) % 1000);
    QString format = "mm:ss";
    if (duration > 3600)
        format = "hh:mm:ss";
    QString durationStr = totalTime.toString(format);
    return durationStr;
}

QStringList audioFileFilters()
{
    QStringList filters;
    filters << "*.mp3" << "*.flac" << "*.m4a" << "*.ogg" << "*.wma" << "*.wav" << "*.mp4" << "*.mkv" << "*.webm" << "*.m3u";
    return filters;
}

bool isAudioFile(QString path)
{
    QStringList filters = audioFileFilters();

    for(const QString &filter : filters) {
        QRegularExpression rx = QRegularExpression::fromWildcard(filter,
                                                                 Qt::CaseInsensitive,
                                                                 QRegularExpression::UnanchoredWildcardConversion);
        if(rx.match(path).hasMatch()) {
            return true;
        }
    }

    return false;
}

bool isPlaylist(const QUrl &url)
{
    if (!url.isLocalFile())
        return false;
    const QFileInfo fileInfo(url.toLocalFile());
    return fileInfo.exists()
           && !fileInfo.suffix().compare(QLatin1String("m3u"), Qt::CaseInsensitive);
}

bool isYouTubeUrl(const QUrl &url)
{
    if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
        return false;

    const QString host = url.host().toLower();
    return host == QLatin1String("youtu.be")
           || host == QLatin1String("youtube.com")
           || host.endsWith(QLatin1String(".youtube.com"))
           || host == QLatin1String("youtube-nocookie.com")
           || host.endsWith(QLatin1String(".youtube-nocookie.com"));
}

QString youtubeVideoId(const QUrl &url)
{
    if (!isYouTubeUrl(url))
        return {};

    const QString host = url.host().toLower();
    const QStringList pathParts = url.path(QUrl::FullyDecoded).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (host == QLatin1String("youtu.be"))
        return pathParts.value(0);

    const QUrlQuery query(url);
    const QString queryId = query.queryItemValue(QLatin1String("v"), QUrl::FullyDecoded).trimmed();
    if (!queryId.isEmpty())
        return queryId;

    if (pathParts.size() >= 2
        && (pathParts.at(0) == QLatin1String("shorts")
            || pathParts.at(0) == QLatin1String("embed")
            || pathParts.at(0) == QLatin1String("live")
            || pathParts.at(0) == QLatin1String("v"))) {
        return pathParts.at(1);
    }
    return {};
}
