#ifndef WINAMPSKIN_H
#define WINAMPSKIN_H

#include <QHash>
#include <QImage>
#include <QString>
#include <QTemporaryDir>
#include <memory>

class QWidget;

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

private:
    WinampSkin() = default;
    bool loadArchive(const QString &filePath, QHash<QString, QImage> *sheets,
                     QString *error) const;
    bool loadDirectory(const QString &directoryPath, QHash<QString, QImage> *sheets,
                       QString *error) const;
    void buildAssets();
    QString writeImage(const QString &name, const QImage &image);

    QString m_filePath;
    QHash<QString, QImage> m_sheets;
    QHash<QString, QString> m_assetPaths;
    std::unique_ptr<QTemporaryDir> m_assetDirectory;
};

#endif
