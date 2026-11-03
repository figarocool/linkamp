#ifndef UTIL_H
#define UTIL_H

#include <QMediaMetaData>
#include <QUrl>

#define DEFAULT_SAMPLE_RATE 44100
#define MAX_AUDIO_STREAM_SAMPLE_SIZE 4096

QMediaMetaData parseMetaData(const QUrl &url);

QString formatDuration(qint64 ms);
QString repairUtf8Mojibake(QString text);

QStringList audioFileFilters();

bool isAudioFile(QString path);

bool isPlaylist(const QUrl &url); // Check for ".m3u" playlists.

bool isYouTubeUrl(const QUrl &url);
QString youtubeVideoId(const QUrl &url);

#endif // UTIL_H
