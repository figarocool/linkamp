#ifndef WINAMPSKIN_H
#define WINAMPSKIN_H

#include <QColor>
#include <QHash>
#include <QImage>
#include <QList>
#include <QString>
#include <QTemporaryDir>
#include <memory>

class QWidget;

struct PlaylistSkinTheme {
    QColor normalText = QColor(255, 255, 255);
    QColor currentText = QColor(255, 255, 255);
    QColor normalBackground = QColor(0, 0, 0);
    QColor selectedBackground = QColor(0, 0, 198);
    QString fontFamily = QStringLiteral("Arial");
};

// Loader and sprite resolver for Winamp 2.x classic .wsz skins.
class WinampSkin
{
public:
    static WinampSkin &instance();

    bool load(const QString &filePath, QString *error = nullptr);
    void reset();
    QString filePath() const { return m_filePath; }
    bool isLoaded() const { return !m_filePath.isEmpty(); }
    QImage sheet(const QString &name) const;
    bool hasSheet(const QString &name) const;
    QString assetPath(const QString &alias) const;
    QString applyToStylesheet(const QString &styleSheet) const;
    void refreshWidgetStyles();
    const PlaylistSkinTheme &playlistTheme() const { return m_playlistTheme; }
    const QList<QColor> &visColors() const { return m_visColors; }

private:
    WinampSkin() = default;
    bool loadArchive(const QString &filePath, QHash<QString, QImage> *sheets,
                     PlaylistSkinTheme *playlistTheme, QList<QColor> *visColors,
                     QString *error) const;
    bool loadDirectory(const QString &directoryPath, QHash<QString, QImage> *sheets,
                       PlaylistSkinTheme *playlistTheme, QList<QColor> *visColors,
                       QString *error) const;
    void buildAssets();
    QString writeImage(const QString &name, const QImage &image);

    QString m_filePath;
    QHash<QString, QImage> m_sheets;
    QHash<QString, QString> m_assetPaths;
    std::unique_ptr<QTemporaryDir> m_assetDirectory;
    PlaylistSkinTheme m_playlistTheme;
    QList<QColor> m_visColors;
};

#endif
