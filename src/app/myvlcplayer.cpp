#include "myvlcplayer.h"

#include <QDebug>
#include <QFileDialog>
#include <QSettings>
#include <QInputDialog>
#include <stdexcept>



MyVlcPlayer::~MyVlcPlayer()
{

    qDebug() << "Destructor VLCPlayer";

    resetActionMenu();

    delete subMenuOpen;
    delete _video;
    delete _audio;
    delete _controlVideo;
    delete _controlAudio;
    delete _player;
    //    if (_media != nullptr)
    delete _media;
    delete _instance;

}


MyVlcPlayer::MyVlcPlayer(QWidget *parent) : QWidget(parent), _media(0)
{

    initUI();
    connect(video, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(videoRightClick()) );
    connect(label, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(videoRightClick()) );



    isRadio = false;


    setMouseTracking(true);

    seek->setVisible(false);
    widget_volume->setVisible(false);
    label_volume->setVolumeLabel(true);
    label->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    widgetControlhidden = true;


    _instance = new VlcInstance(VlcCommon::args(), this);
    _instance->setLogLevel(Vlc::ErrorLevel);
//        _instance->setUserAgent(qApp->applicationName(), qApp->applicationVersion());

    // Guard against libvlc_new() returning NULL (e.g. missing VLC plugin
    // directory).  VlcMediaPlayer::setVideoWidget() passes its internal
    // libvlc_media_player_t* directly to libvlc without a NULL check, so
    // proceeding with a failed instance causes a SIGSEGV.  Throwing here
    // lets WorkoutDialog's try-catch disable the radio player gracefully.
    // _instance is a Qt child of `this`; QObject's destructor cleans it up.
    if (!_instance->core())
        throw std::runtime_error("VLC: libvlc_new() failed - VLC plugins unavailable");

    _player = new VlcMediaPlayer(_instance);
    _player->setVideoWidget(video);
    _video= new VlcVideo(_player);
    _audio = new VlcAudio(_player);


    _controlVideo = new VlcControlVideo(_player);
    _controlAudio = new VlcControlAudio(_player);


    //        connect(_controlVideo, SIGNAL(subtitleTracks(QList<QAction*>)), this, SLOT(subtitlesTrackChanged(QList<QAction*>)) );
    connect(_controlVideo, SIGNAL(subtitleTracks(QList<QAction*>)), this, SLOT(checkForSubtitle(QList<QAction*>)) );
    //    connect(_controlVideo, SIGNAL(videoTracks(QList<QAction*>)), this, SLOT(videoTracksChanged(QList<QAction*>)) );
    connect(_controlAudio, SIGNAL(audioTracks(QList<QAction*>)), this, SLOT(audioTracksChanged(QList<QAction*>)) );

    videoAlreadySet = false;
    audioAlreadySet = false;
    subtitlesAlreadySet = false;


    connect(horizontalSlider_volume, SIGNAL(valueChanged(int)), this, SLOT(changeVolume(int)));
    connect(label_volume, SIGNAL(clicked(bool)), this, SLOT(muteVolume(bool)));



    //INSTALL EVEN FILTER ON SEEK BAR, VOLUME LABEL, VOLUME SLIDER, To show widgets +5secs
    label_volume->setMouseTracking(true);
    horizontalSlider_volume->setMouseTracking(true);
    seek->setMouseTracking(true);
    widget_volume->setMouseTracking(true);
    label->setMouseTracking(true);
    video->setMouseTracking(true);

    label_volume->installEventFilter(this);
    horizontalSlider_volume->installEventFilter(this);
    seek->installEventFilter(this);
    widget_volume->installEventFilter(this);
    label->installEventFilter(this);
    video->installEventFilter(this);


    video->setMediaPlayer(_player);
    seek->setMediaPlayer(_player);
    video->setContextMenuPolicy(Qt::CustomContextMenu);


    isStarted = false;
    videoLoaded = false;
    isMuted = false;
    timer_hideWidgets = new QTimer(this);



    //------  Menu --------------------------------
    menu = new QMenu(tr("Menu"), this);

    action_playOrPause = menu->addAction(QIcon(":/image/icon/play"), tr("Play"));
    action_stop = menu->addAction(QIcon(":/image/icon/stop"), tr("Stop"));
    action_stop->setDisabled(true);
    menu->addSeparator();

    subMenuSubtitleTrack = menu->addMenu(tr("Subtitle Track"));
    subMenuSubtitleTrack->setDisabled(true);
    subtitleTrackActionGroup = nullptr;


    menu->addSeparator();



    subMenuOpen = menu->addMenu(tr("Open media"));
    action_openLocalMedia = subMenuOpen->addAction(QIcon(":/image/icon/folder"), tr("Local media") );
    action_openURLMedia = subMenuOpen->addAction(QIcon(), tr("URL") );

    //----- Fin menu -------------------------


    connect(action_playOrPause, SIGNAL(triggered()), this, SLOT(playOrPause()));
    connect(action_stop, SIGNAL(triggered()), this, SLOT(stop()));

    connect(action_openLocalMedia, SIGNAL(triggered()), this, SLOT(openLocal()));
    connect(action_openURLMedia, SIGNAL(triggered()), this, SLOT(openUrl()));

    connect(timer_hideWidgets, SIGNAL(timeout()), this, SLOT(hideWidgets()));



    //Foward signals
    connect(_player, SIGNAL(playing()), this, SIGNAL(playing()) );
    connect(_player, SIGNAL(paused()), this, SIGNAL(paused()) );
    connect(_player, SIGNAL(stopped()), this, SIGNAL(stopped()) );

}




//-------------------------------------------------------------------------------------------
void MyVlcPlayer::videoRightClick() {
    menu->popup(QCursor::pos());
}





//-------------------------------------------------------------------------------------------
void MyVlcPlayer::setMovieTime(int msec) {

    _player->setTime(msec);

}




//-------------------------------------------------------------------------------------------
void MyVlcPlayer::audioTracksChanged(QList<QAction*> lstAction) {

    if (audioAlreadySet)
        return;

    if (lstAction.size() > 0) {

        audioAlreadySet = true;
        int volume;
        if (isRadio)
            volume = 100;
        else
            volume = loadSoundVolume();


        widget_volume->setVisible(true);
        horizontalSlider_volume->setValue(volume);
        label_currentVolume->setText(QString::number(volume)+"%");
    }

}


//----------------------------------------------------------------------------------------------
void MyVlcPlayer::subtitleChanged() {

    qDebug() << " SUBTITLE CHANGED";

    QAction *activeAction =  subtitleTrackActionGroup->checkedAction();
    int value = activeAction->data().toInt();

    _video->setSubtitle(value);
}
//----------------------------------------------------------------------------------------------------
void MyVlcPlayer::checkForSubtitle(QList<QAction*> lstAction) {

    //    qDebug() << "checking for sub.." << subtitlesAlreadySet;
    if (subtitlesAlreadySet)
        return;

    if (lstAction.size() > 0) {
        subtitlesAlreadySet = true;
        updateSubtitle();
    }
}

//----------------------------------------------------------------------------------------------
void MyVlcPlayer::updateSubtitle() {


    qDebug() << "updateSubtitle...";

    subMenuSubtitleTrack->setEnabled(true);

    //---- Construct subtitle menu
    if (subtitleTrackActionGroup == nullptr) {

        qDebug() << "nullptr...";

        subtitleTrackActionGroup = new QActionGroup(this);
        subtitleTrackActionGroup->setExclusive(true);
        QAction *actionSubtitleChoice;


        int nbSub = _video->subtitleCount();


        if (nbSub>0) {
            subMenuSubtitleTrack->setEnabled(true);

            int currentSubtitleChecked = _video->subtitle();
            qDebug() << "Current subtitle checked : " << currentSubtitleChecked;
            QStringList lstSub = _video->subtitleDescription();

            QList<int> lstID = _video->subtitleIds();
            int y=0;
            foreach(int i, lstID) {

                qDebug() << "ADDING Sub :" << lstSub.at(y) << " ID : " << i;

                actionSubtitleChoice = subMenuSubtitleTrack->addAction(lstSub.at(y));
                actionSubtitleChoice->setCheckable(true);
                actionSubtitleChoice->setData(i);
                subtitleTrackActionGroup->addAction(actionSubtitleChoice);
                lstSubAction.append(actionSubtitleChoice);

                connect(actionSubtitleChoice, SIGNAL(triggered()), this, SLOT(subtitleChanged()));

                if (currentSubtitleChecked == i)
                    actionSubtitleChoice->setChecked(true);
                y++;
            }
        }
        else {
            subMenuSubtitleTrack->setEnabled(false);
        }

    }

}



//----------------------------------------------------------------------------------------------
void MyVlcPlayer::resetActionMenu() {


    //    subMenuAudioTrack->clear();
    //    subMenuVideoTrack->clear();
    //    subMenuSubtitleTrack->clear();


    ///------------
    foreach (QAction *myAction, lstSubAction) {
        disconnect(myAction, SIGNAL(triggered()), this, SLOT(subtitleChanged()));
        subtitleTrackActionGroup->removeAction(myAction);
        subMenuSubtitleTrack->removeAction(myAction);

        delete myAction;
    }
    lstSubAction.clear();
    subMenuSubtitleTrack->clear();
    subtitleTrackActionGroup = nullptr;
    ///------

}



//------------------------------------------------------------------------------------------------
bool MyVlcPlayer::eventFilter(QObject *watched, QEvent *event) {


    Q_UNUSED(watched);

    if(videoLoaded && event->type() == QEvent::MouseMove)  ///Add some cooldown 50ms?
    {
        //        qDebug() << "watched object" << watched << "event:" << event << "eventType" << event->type();
        pushTimerHideWidgets();
        if (widgetControlhidden) {
            seek->setVisible(true);
            widget_volume->setVisible(true);
            widgetControlhidden = false;
        }
    }
    video->setAspectRatio(Vlc::Original);
    return false;

}
//------------------------------------------------------------------------------------------------
void MyVlcPlayer::hideWidgets() {

    timer_hideWidgets->stop();
    seek->setVisible(false);
    widget_volume->setVisible(false);
    widgetControlhidden = true;

    video->setAspectRatio(Vlc::Original);
    this->parentWidget()->setFocus();
}
//------------------------------------------------------------------------------------------------
void MyVlcPlayer::pushTimerHideWidgets() {
    timer_hideWidgets->start(3000);
}




//------------------------------------------------------------------------------------------------
void MyVlcPlayer::setPauseInterface() {
    action_playOrPause->setIcon(QIcon(":/image/icon/pause"));
    action_playOrPause->setText(tr("Pause"));
}

void MyVlcPlayer::setPlayInterface() {
    action_playOrPause->setIcon(QIcon(":/image/icon/play"));
    action_playOrPause->setText(tr("Play"));
}

void MyVlcPlayer::interfaceVideoLoaded(bool loaded) {
    action_stop->setEnabled(loaded);
    //    action_prev->setEnabled(loaded);
    //    action_next->setEnabled(loaded);

    label->setVisible(!loaded);
    seek->setVisible(true);
    widgetControlhidden = false;


}



//----------------------------------------------------------------------------------------------
void MyVlcPlayer::stop() {

    qDebug() << "vlcplayer.. stop!";

    if (!isRadio) {
        interfaceVideoLoaded(false);
        setPlayInterface();
        videoLoaded = false;

        subMenuSubtitleTrack->setEnabled(false);
    }

    isStarted = false;
    _player->stop();
    //    _controlVideo->reset();
    //    _controlAudio->reset();

    //    subMenuAudioTrack->setEnabled(false);
    //    subMenuVideoTrack->setEnabled(false);


    qDebug() << "vlcplayer.. stop done!";
}

//----------------------------------------------------------------------------------------------
void MyVlcPlayer::playOrPause() {

    //if no file open, show file openLocal
    if (!videoLoaded) {
        openLocal();
        return;
    }

    if (isStarted) {
        pause();
    }
    else {
        resume();
    }
}
//---------------------------------
void MyVlcPlayer::pause() {


    if (videoLoaded && isStarted) {

        qDebug() << "Pause Player";
        isStarted = false;
        _player->pause();
        setPlayInterface();
    }

}
void MyVlcPlayer::resume() {

    if (!videoLoaded)
        return;
    qDebug() << "Resume";
    isStarted = true;
    _player->resume();
    setPauseInterface();

}

//----------------------------------------------------------------------------------------------
void MyVlcPlayer::openLocal() {



    QString file = QFileDialog::getOpenFileName(this, tr("Open file"),
                                                loadPath(),
                                                tr("Multimedia Files(*)"));
    if (file.isEmpty())
        return;

    ////reset media if playing one
    //    subMenuAudioTrack->setEnabled(false);
    //    subMenuVideoTrack->setEnabled(false);
    subMenuSubtitleTrack->setEnabled(false);
    resetActionMenu();
    _controlVideo->reset();
    _controlAudio->reset();
    videoAlreadySet = false;
    audioAlreadySet = false;
    subtitlesAlreadySet = false;
    /////------------------------

    qDebug() << "FILE TO OPEN IS:" << file;

    _media = new VlcMedia(file, true, _instance);
    _player->open(_media);

    isStarted = true;
    videoLoaded = true;
    setPauseInterface();
    interfaceVideoLoaded(true);

    savePath(file);
}

//----------------------------------------------------------------------------------------------
void MyVlcPlayer::openUrl() {


    if (videoLoaded)
        _player->stop();



    //---- reset current media
    //    subMenuAudioTrack->setEnabled(false);
    //    subMenuVideoTrack->setEnabled(false);
    subMenuSubtitleTrack->setEnabled(false);
    resetActionMenu();
    _controlVideo->reset();
    _controlAudio->reset();
    videoAlreadySet = false;
    audioAlreadySet = false;
    subtitlesAlreadySet = false;
    //-----------------------------

    QString url = QInputDialog::getText(this, tr("Open Url"), tr("<font color='black'>Enter the URL you want to play</font>"));

    if (url.isEmpty())
        return;

    _media = new VlcMedia(url, _instance);
    _player->open(_media);

    isStarted = true;
    videoLoaded = true;
    setPauseInterface();
    interfaceVideoLoaded(true);




}

//----------------------------------------------------------------------------------------------
void MyVlcPlayer::openUrlRadio(QString url) {

    //cursor bug here..

//    QHostInfo::lookupHost("qt-project.org", this, SLOT(openUrlRadioFromIp(QHostInfo)));

    qDebug() << "OPEN URL";
    _media = new VlcMedia(url, _instance);
    _player->open(_media);

    //1- change DNS to ip address
    //2 load new URL
    //http://drumstep-high.rautemusik.fm

    qDebug() << "Find openURl!";

}

//----------------------------------------------------------------------------------------------
void MyVlcPlayer::openUrlRadioFromIp(QHostInfo hostInfo) {

    //cursor bug here..

    qDebug() << "openUrlRadioFromIp!" << hostInfo.hostName() << " tt" << hostInfo.localHostName();

    QList<QHostAddress>	listAddress = hostInfo.addresses();
    qDebug() << listAddress.size();
//    qDebug() << listAddress.at(0);
}


//----------------------------------------------------------------------------------------------
void MyVlcPlayer::muteVolume(bool mute) {

    Q_UNUSED(mute);

    _audio->toggleMute();
}

//----------------------------------------------------------------------------------------------
void MyVlcPlayer::changeVolume(int volume) {


    label_currentVolume->setText(QString::number(volume)+"%");
    _audio->setVolume(volume);

    if (!isRadio)
        saveSoundVolume(volume);
}


//----------------------------------------------------------------------------------------------
void MyVlcPlayer::savePath(QString path) {

    QSettings settings;
    settings.beginGroup("videoPlayer");
    settings.setValue("loadPath", path);
    settings.endGroup();
}


//----------------------------------------------------------------------------------------------
QString MyVlcPlayer::loadPath() {


    QSettings settings;
    settings.beginGroup("videoPlayer");
    QString path = settings.value("loadPath", QDir::homePath() ).toString();
    settings.endGroup();
    return path;
}

//----------------------------------------------------------------------------------------
void MyVlcPlayer::saveSoundVolume(int vol) {

    QSettings settings;

    settings.beginGroup("videoPlayer");
    settings.setValue("soundVolume", vol);
    settings.endGroup();
}


//----------------------------------------------------------------------------------------------
int MyVlcPlayer::loadSoundVolume() {


    QSettings settings;

    settings.beginGroup("videoPlayer");
    int volume = settings.value("soundVolume", 100 ).toInt();
    settings.endGroup();

    return volume;
}












//-------------------------------------------------------------------------
void MyVlcPlayer::initUI() {


    if (this->objectName().isEmpty())
        this->setObjectName(QStringLiteral("MyVlcPlayer"));


    this->resize(1088, 704);
    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(this->sizePolicy().hasHeightForWidth());
    this->setSizePolicy(sizePolicy);
    this->setMinimumSize(QSize(0, 0));


    this->setStyleSheet(QLatin1String("#MyVlcPlayer {\n"
                                      "	 background-color: rgb(25, 25, 25); \n"
                                      "}\n"
                                      "\n"
                                      "\n"
                                      "QLabel {\n"
                                      "	color: white;\n"
                                      "}\n"
                                      "\n"
                                      "QInputDialog {\n"
                                      "	color: black;\n"
                                      "}\n"
                                      "\n"
                                      "QLabel#label_volume {\n"
                                      "	image: url(:/image/icon/volume);\n"
                                      "}"));


    //    " /*QProgressBar {\n"
    //    "	background-color: rgb(75, 75, 75);\n"
    //    "	border: 2px solid grey;\n"
    //    "    border-radius: 5px;\n"
    //    "	text-align: center;\n"
    //    "	color: rgb(255,255,255)\n"
    //    "\n"
    //    " }\n"
    //    "\n"
    //    "QProgressBar::chunk {\n"
    //    "	border-bottom-left-radius: 4px;\n"
    //    "	border-top-left-radius: 4px;\n"
    //    "	border-bottom-right-radius: 4px;\n"
    //    "	border-top-right-radius: 4px;\n"
    //    "	background-color: orange;\n"
    //    "}\n"
    //    "*/\n"
    //    "\n"
    //    "QInputDialog {\n"
    //    "	color: black;\n"
    //    "}\n"
    //    "\n"



    gridLayout = new QGridLayout(this);
    gridLayout->setSpacing(0);
    gridLayout->setObjectName(QStringLiteral("gridLayout"));
    gridLayout->setContentsMargins(0, 0, 0, 0);
    widget_volume = new QWidget(this);
    widget_volume->setObjectName(QStringLiteral("widget_volume"));
    horizontalLayout = new QHBoxLayout(widget_volume);
    horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
    horizontalLayout->setContentsMargins(-1, 0, -1, 0);
    horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    horizontalLayout->addItem(horizontalSpacer);

    label_volume = new ClickableLabel(widget_volume);
    label_volume->setObjectName(QStringLiteral("label_volume"));
    QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Preferred);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(label_volume->sizePolicy().hasHeightForWidth());
    label_volume->setSizePolicy(sizePolicy1);
    label_volume->setMinimumSize(QSize(20, 0));
    label_volume->setMaximumSize(QSize(200, 16777215));
    label_volume->setCursor(QCursor(Qt::PointingHandCursor));

    horizontalLayout->addWidget(label_volume);

    horizontalSlider_volume = new QSlider(widget_volume);
    horizontalSlider_volume->setObjectName(QStringLiteral("horizontalSlider_volume"));
    QSizePolicy sizePolicy2(QSizePolicy::Maximum, QSizePolicy::Fixed);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    sizePolicy2.setHeightForWidth(horizontalSlider_volume->sizePolicy().hasHeightForWidth());
    horizontalSlider_volume->setSizePolicy(sizePolicy2);
    horizontalSlider_volume->setMinimumSize(QSize(100, 0));
    horizontalSlider_volume->setMaximumSize(QSize(100, 16777215));
    horizontalSlider_volume->setMaximum(200);
    horizontalSlider_volume->setValue(100);
    horizontalSlider_volume->setOrientation(Qt::Horizontal);

    horizontalLayout->addWidget(horizontalSlider_volume);

    label_currentVolume = new QLabel(widget_volume);
    label_currentVolume->setObjectName(QStringLiteral("label_currentVolume"));
    QSizePolicy sizePolicy3(QSizePolicy::Fixed, QSizePolicy::Preferred);
    sizePolicy3.setHorizontalStretch(0);
    sizePolicy3.setVerticalStretch(0);
    sizePolicy3.setHeightForWidth(label_currentVolume->sizePolicy().hasHeightForWidth());
    label_currentVolume->setSizePolicy(sizePolicy3);
    label_currentVolume->setMinimumSize(QSize(45, 0));
    label_currentVolume->setMaximumSize(QSize(30, 16777215));

    horizontalLayout->addWidget(label_currentVolume);


    gridLayout->addWidget(widget_volume, 4, 0, 1, 1);

    label = new QLabel(this);
    label->setObjectName(QStringLiteral("label"));
    QFont font;
    font.setPointSize(13);
    label->setFont(font);
    label->setContextMenuPolicy(Qt::CustomContextMenu);
    label->setAlignment(Qt::AlignCenter);
    label->setText(tr("Right click to open media"));

    gridLayout->addWidget(label, 0, 0, 1, 1);

    video = new VlcWidgetVideo(this);
    video->setObjectName(QStringLiteral("video"));
    QSizePolicy sizePolicy4(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    sizePolicy4.setHorizontalStretch(0);
    sizePolicy4.setVerticalStretch(0);
    sizePolicy4.setWidthForHeight(true);
    video->setSizePolicy(sizePolicy4);
    video->setAspectRatio(Vlc::Original);

    gridLayout->addWidget(video, 0, 0, 1, 1);


    seek = new VlcWidgetSeek(this);
    seek->setObjectName(QStringLiteral("seek"));
    QSizePolicy sizePolicy5(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    sizePolicy5.setHorizontalStretch(0);
    sizePolicy5.setVerticalStretch(0);
    sizePolicy5.setHeightForWidth(seek->sizePolicy().hasHeightForWidth());
    seek->setSizePolicy(sizePolicy5);

    gridLayout->addWidget(seek, 1, 0, 1, 1);


}
