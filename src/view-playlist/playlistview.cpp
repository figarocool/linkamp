#include "playlistview.h"
#include "ui_playlistview.h"
#include "filebrowsericonprovider.h"
#include "util.h"
#include <QFileIconProvider>
#include <QIcon>
#include <QFont>
#include <QMenu>
#include <QStandardPaths>
#include <algorithm>

const QString HOME_PATH = QStandardPaths::standardLocations(QStandardPaths::MusicLocation).first();

PlaylistView::PlaylistView(QWidget *parent, PlaylistModel *playlistModel) :
    QWidget(parent),
    ui(new Ui::PlaylistView)
{
    m_playlistModel = playlistModel;
    m_playlist = m_playlistModel->playlist();

    ui->setupUi(this);
    setupPlayListUi();
    setupFileBrowserUi();
    
    connect(ui->goPlayerButton, &QPushButton::clicked, this, &PlaylistView::showPlayerClicked);
    connect(m_playlist, &QMediaPlaylist::currentIndexChanged, this, &PlaylistView::playlistPositionChanged);

    connect(ui->playList, &QAbstractItemView::clicked, this, &PlaylistView::handleSongSelected);

    connect(ui->clearButton, &QPushButton::clicked, this, [this] {
        if (m_classicMode)
            showListMenu();
        else
            clearPlaylist();
    });
    connect(ui->removeButton, &QPushButton::clicked, this, &PlaylistView::removeItem);

    connect(ui->fbHomeButton, &QPushButton::clicked, this, &PlaylistView::fbGoHome);
    connect(ui->fbUpButton, &QPushButton::clicked, this, &PlaylistView::fbGoUp);
    connect(ui->fbSelectButton, &QPushButton::clicked, this, [this] {
        if (m_classicMode)
            togglePlaylistSelection();
        else
            fbToggleSelect();
    });
    connect(ui->fbAddButton, &QPushButton::clicked, this, [this] {
        if (m_classicMode)
            showAddMenu();
        else
            fbAdd();
    });
    connect(ui->fileBrowserListView, &QAbstractItemView::clicked, this, &PlaylistView::fbItemClicked);

    connect(m_playlist, &QMediaPlaylist::mediaChanged, this, &PlaylistView::updateTotalDuration);
    connect(m_playlist, &QMediaPlaylist::mediaInserted, this, &PlaylistView::updateTotalDuration);
    connect(m_playlist, &QMediaPlaylist::mediaRemoved, this, &PlaylistView::updateTotalDuration);
    connect(m_playlist, &QMediaPlaylist::currentSelectionChanged, this, &PlaylistView::handleSelectionChanged);

    connect(ui->editButton, &QPushButton::clicked, this, &PlaylistView::toggleEditMode);

    updateTotalDuration();
}

PlaylistView::~PlaylistView()
{
    delete ui;
}

void PlaylistView::setClassicMode()
{
    m_classicMode = true;
    ui->sidebarContainer->hide();
    ui->fileBrowserContainer->hide();
    ui->frame->hide();
    ui->plDurationFrame->hide();

    ui->playList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->playList->header()->hide();
    ui->playList->setColumnHidden(2, true);
    ui->playList->setColumnHidden(3, true);
    ui->playList->setColumnHidden(4, true);
    ui->playList->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->playList->header()->resizeSection(0, 22);
    ui->playList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->playList->setFont(QFont(QStringLiteral("DejaVu Sans Mono"), 8));
    ui->playList->setStyleSheet(ui->playList->styleSheet()
        + QStringLiteral("\nQTreeView::item { height: 14px; padding-right: 2px; }"));

    auto buttonStyle = QStringLiteral(
        "QPushButton { color:#00e800; background:#20202b; border:1px solid #707080; padding:0; }"
        "QPushButton:pressed { color:#000; background:#00e800; }");
    auto setButton = [&buttonStyle](QPushButton *button, const QString &text, int width) {
        button->setText(text);
        button->setIcon(QIcon());
        button->setFont(QFont(QStringLiteral("DejaVu Sans Mono"), 6, QFont::Bold));
        button->setFixedSize(width, 18);
        button->setStyleSheet(buttonStyle);
    };

    ui->goPlayerButton->setFixedSize(22, 18);
    ui->goPlayerButton->setIconSize(QSize(14, 12));
    ui->goPlayerButton->setStyleSheet(buttonStyle);
    setButton(ui->fbAddButton, QStringLiteral("ADD"), 34);
    setButton(ui->removeButton, QStringLiteral("REM"), 32);
    setButton(ui->fbSelectButton, QStringLiteral("SEL"), 30);
    setButton(ui->editButton, QStringLiteral("MISC"), 36);
    setButton(ui->clearButton, QStringLiteral("LIST"), 32);
    ui->editButton->setCheckable(true);

    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(2, 1, 2, 1);
    toolbar->setSpacing(1);
    toolbar->addWidget(ui->goPlayerButton);
    toolbar->addWidget(ui->fbAddButton);
    toolbar->addWidget(ui->removeButton);
    toolbar->addWidget(ui->fbSelectButton);
    toolbar->addWidget(ui->editButton);
    toolbar->addWidget(ui->clearButton);
    toolbar->addStretch(1);

    if (auto *layout = qobject_cast<QVBoxLayout *>(ui->playlistContainer->layout()))
        layout->addLayout(toolbar);
}

void PlaylistView::playlistPositionChanged(int currentItem)
{
    if (ui->playList) {
        ui->playList->setCurrentIndex(m_playlistModel->index(currentItem, 0));
        ui->playList->update(); // Force refresh for making sure play icon is updated correctly
    }
}

void PlaylistView::clearPlaylist()
{
    m_playlist->clear();
}

void PlaylistView::removeItem()
{
    QModelIndexList rows = ui->playList->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        const QModelIndex current = ui->playList->selectionModel()->currentIndex();
        if (current.isValid())
            rows.append(current);
    }
    std::sort(rows.begin(), rows.end(), [](const QModelIndex &a, const QModelIndex &b) {
        return a.row() > b.row();
    });
    for (const QModelIndex &row : rows)
        m_playlistModel->removeRow(row.row());
}


void PlaylistView::setupPlayListUi()
{
    // Add touch scroll to playList
    QScrollerProperties sp;
    sp.setScrollMetric(QScrollerProperties::DragVelocitySmoothingFactor, 0.6);
    sp.setScrollMetric(QScrollerProperties::MinimumVelocity, 0.0);
    sp.setScrollMetric(QScrollerProperties::MaximumVelocity, 0.5);
    sp.setScrollMetric(QScrollerProperties::AcceleratingFlickMaximumTime, 0.4);
    sp.setScrollMetric(QScrollerProperties::AcceleratingFlickSpeedupFactor, 1.2);
    sp.setScrollMetric(QScrollerProperties::SnapPositionRatio, 0.2);
    sp.setScrollMetric(QScrollerProperties::MaximumClickThroughVelocity, 0);
    sp.setScrollMetric(QScrollerProperties::DragStartDistance, 0.001);
    sp.setScrollMetric(QScrollerProperties::MousePressEventDelay, 1.0);
    sp.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.3);
    sp.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.1);
    m_playlistViewScroller = QScroller::scroller(ui->playList);
    m_playlistViewScroller->grabGesture(ui->playList, QScroller::LeftMouseButtonGesture);
    m_playlistViewScroller->setScrollerProperties(sp);

    ui->playList->setModel(m_playlistModel);
    ui->playList->setCurrentIndex(m_playlistModel->index(m_playlist->currentIndex(), 0));

    ui->playList->setSortingEnabled(false);
    ui->playList->setIndentation(0);
    ui->playList->setAllColumnsShowFocus(false);
    ui->playList->header()->setSectionResizeMode(0, QHeaderView::ResizeMode::Fixed);
    ui->playList->header()->setSectionResizeMode(1, QHeaderView::ResizeMode::ResizeToContents);
    ui->playList->header()->setSectionResizeMode(2, QHeaderView::ResizeMode::ResizeToContents);
    ui->playList->header()->setSectionResizeMode(3, QHeaderView::ResizeMode::ResizeToContents);
    ui->playList->header()->setSectionResizeMode(4, QHeaderView::ResizeMode::Fixed);
    ui->playList->header()->setDefaultSectionSize(80);

    ui->playList->setDropIndicatorShown(true);
    ui->playList->setDragDropMode(QAbstractItemView::DragDrop);
    ui->playList->setDefaultDropAction(Qt::CopyAction);
    ui->playList->setDragDropOverwriteMode(false);
    ui->playList->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
    // Disable drag and drop until we enable edit mode
    ui->playList->setDragEnabled(false);
    ui->playList->setAcceptDrops(false);
}

void PlaylistView::setupFileBrowserUi()
{
    m_fileSystemModel = new QFileSystemModel(this);
    // filter only folders and audio files
    QStringList filters = audioFileFilters();
    m_fileSystemModel->setNameFilters(filters);
    m_fileSystemModel->setNameFilterDisables(false); // Hide filtered files

    FileBrowserIconProvider *iconProvider = new FileBrowserIconProvider();
    iconProvider->setOptions(QFileIconProvider::DontUseCustomDirectoryIcons);
    m_fileSystemModel->setIconProvider(iconProvider);

    ui->fileBrowserListView->setModel(m_fileSystemModel);
    ui->fileBrowserListView->setSelectionMode(QAbstractItemView::MultiSelection);
    ui->fileBrowserListView->setFocusPolicy(Qt::FocusPolicy::NoFocus);

    fbCd(HOME_PATH);

    // Add touch scroll to playList
    QScrollerProperties sp;
    sp.setScrollMetric(QScrollerProperties::DragVelocitySmoothingFactor, 0.6);
    sp.setScrollMetric(QScrollerProperties::MinimumVelocity, 0.0);
    sp.setScrollMetric(QScrollerProperties::MaximumVelocity, 0.5);
    sp.setScrollMetric(QScrollerProperties::AcceleratingFlickMaximumTime, 0.4);
    sp.setScrollMetric(QScrollerProperties::AcceleratingFlickSpeedupFactor, 1.2);
    sp.setScrollMetric(QScrollerProperties::SnapPositionRatio, 0.2);
    sp.setScrollMetric(QScrollerProperties::MaximumClickThroughVelocity, 0);
    sp.setScrollMetric(QScrollerProperties::DragStartDistance, 0.001);
    sp.setScrollMetric(QScrollerProperties::MousePressEventDelay, 1.0);
    sp.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.3);
    sp.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.1);
    QScroller* fbScroller = QScroller::scroller(ui->fileBrowserListView);
    fbScroller->grabGesture(ui->fileBrowserListView, QScroller::LeftMouseButtonGesture);
    fbScroller->setScrollerProperties(sp);
}


void PlaylistView::fbCd(QString path)
{
    ui->fileBrowserListView->clearSelection();
    m_fileSystemModel->setRootPath(path);
    ui->fileBrowserListView->setRootIndex(m_fileSystemModel->index(path));
}

void PlaylistView::fbGoHome()
{
    fbCd(HOME_PATH);
}

void PlaylistView::fbGoUp()
{
    QDir dir = QDir(m_fileSystemModel->rootDirectory());
    dir.cdUp();
    fbCd(dir.absolutePath());
}

void PlaylistView::fbAdd()
{
    QList<QUrl> files;

    QModelIndexList selIndexes = ui->fileBrowserListView->selectionModel()->selectedIndexes();

    for(QModelIndex index : selIndexes) {
        QUrl url = QUrl::fromLocalFile(m_fileSystemModel->filePath(index));
        // Filter only allowed audio files by extension
        if(isAudioFile(url.fileName())) {
            files.append(url);
        }
    }

    // Clear selection
    ui->fileBrowserListView->clearSelection();

    emit addSelectedFilesClicked(files);
}

void PlaylistView::fbToggleSelect()
{
    qsizetype nSelected = ui->fileBrowserListView->selectionModel()->selectedIndexes().length();
    qsizetype total = m_fileSystemModel->rowCount(m_fileSystemModel->index(m_fileSystemModel->rootPath()));
    if(nSelected == total) {
        ui->fileBrowserListView->clearSelection();
    } else {
        QModelIndex currentIndex = m_fileSystemModel->index(m_fileSystemModel->rootPath());
        int numRows = m_fileSystemModel->rowCount(currentIndex);

        for (int row = 0; row < numRows; ++row) {
            QModelIndex childIndex = m_fileSystemModel->index(row, 0, currentIndex);
            ui->fileBrowserListView->selectionModel()->select(childIndex, QItemSelectionModel::Select);
        }
    }
}

void PlaylistView::togglePlaylistSelection()
{
    const auto selected = ui->playList->selectionModel()->selectedRows();
    if (selected.size() >= m_playlist->mediaCount())
        ui->playList->clearSelection();
    else
        ui->playList->selectAll();
}

void PlaylistView::showAddMenu()
{
    QMenu menu(this);
    QAction *files = menu.addAction(tr("Add files…"));
    QAction *youtube = menu.addAction(tr("Add YouTube URL…"));
    QAction *chosen = menu.exec(ui->fbAddButton->mapToGlobal(QPoint(0, ui->fbAddButton->height())));
    if (chosen == files)
        emit addFilesRequested();
    else if (chosen == youtube)
        emit addYouTubeRequested();
}

void PlaylistView::showListMenu()
{
    QMenu menu(this);
    QAction *clear = menu.addAction(tr("Clear playlist"));
    if (menu.exec(ui->clearButton->mapToGlobal(QPoint(0, ui->clearButton->height()))) == clear)
        clearPlaylist();
}

void PlaylistView::fbItemClicked(const QModelIndex &index)
{
    if(m_fileSystemModel->isDir(index)) {
        fbCd(m_fileSystemModel->filePath(index));
    }
}

void PlaylistView::updateTotalDuration()
{
    qint64 total = m_playlist->totalDuration();
    ui->plDuration->setText(formatDuration(total));
}

void PlaylistView::handleSongSelected(const QModelIndex &index)
{
    bool inEditMode = ui->editButton->isChecked();
    if(!inEditMode) {
        emit songSelected(index);
    }
}

void PlaylistView::toggleEditMode()
{
    bool isEnabled = ui->editButton->isChecked();

    if(isEnabled) {
        ui->playList->setDragEnabled(true);
        ui->playList->setAcceptDrops(true);
        m_playlistViewScroller->ungrabGesture(ui->playList);
    } else {
        ui->playList->setDragEnabled(false);
        ui->playList->setAcceptDrops(false);
        m_playlistViewScroller->grabGesture(ui->playList, QScroller::LeftMouseButtonGesture);
    }
}

void PlaylistView::handleSelectionChanged(int index)
{
    if(index < 0)
        return;
    auto idx = m_playlistModel->index(index, 0);
    ui->playList->selectionModel()->setCurrentIndex(idx,
                                                    QItemSelectionModel::SelectionFlag::Rows |
                                                    QItemSelectionModel::SelectionFlag::SelectCurrent);
}
