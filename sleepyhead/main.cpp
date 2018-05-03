﻿/* SleepyHead Main
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the Linux
 * distribution for more details. */

//#include <QtPlugin>
#include <QApplication>
#include <QMessageBox>
#include <QFontDatabase>
#include <QStringList>
#include <QDebug>
#include <QPushButton>
#include <QWebFrame>
#include <QWebView>
#include <QTranslator>
#include <QDir>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QFileDialog>
#include <QSysInfo>
#include <QXmlSimpleReader>

#include "version.h"
#include "logger.h"
#include "SleepLib/schema.h"
#include "mainwindow.h"
#include "SleepLib/profiles.h"
#include "profileselect.h"
#include "newprofile.h"
#include "translation.h"
#include "common_gui.h"
#include "SleepLib/machine_loader.h"


// Gah! I must add the real darn plugin system one day.
#include "SleepLib/loader_plugins/prs1_loader.h"
#include "SleepLib/loader_plugins/cms50_loader.h"
#include "SleepLib/loader_plugins/cms50f37_loader.h"
#include "SleepLib/loader_plugins/md300w1_loader.h"
#include "SleepLib/loader_plugins/zeo_loader.h"
#include "SleepLib/loader_plugins/somnopose_loader.h"
#include "SleepLib/loader_plugins/resmed_loader.h"
#include "SleepLib/loader_plugins/intellipap_loader.h"
#include "SleepLib/loader_plugins/icon_loader.h"
#include "SleepLib/loader_plugins/weinmann_loader.h"


#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

MainWindow *mainwin = nullptr;

void initialize()
{
    schema::init();
}

void release_notes()
{
    QString str = QObject::tr("SleepyHead Release Notes");
    QDialog relnotes;
    relnotes.setWindowTitle(str);
    relnotes.setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    relnotes.setStyleSheet("QDialog { background:white; }");
    relnotes.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    QHBoxLayout * hlayout = new QHBoxLayout();
    QLabel * title = new QLabel("<html><body><div align=top>"
    "<font size=+4>"+STR_TR_SleepyHead+"</font> &nbsp; "
    "<font size=+1>"+STR_TR_AppVersion+"</font> &nbsp; "
    "<font size=+2><i>"+QObject::tr("Release Notes")+"</i></font>"
    "<hr/>"
    "</div></body></html>", &relnotes);
    QPixmap img=QPixmap(":/docs/sheep.png").scaled(100,100);
    QLabel * logo= new QLabel(&relnotes);//, * logo2 = new QLabel();
    logo->setPixmap(img);
    hlayout->insertWidget(0, title, 1);
    hlayout->insertWidget(1, logo, 0);

    QVBoxLayout * layout = new QVBoxLayout(&relnotes);
    QWebView * web = new QWebView(&relnotes);

//    QString welcomeMessage = "<font size=+1>"
//            "<p>"+QObject::tr("After four years in the making, this build brings SleepyHead into the final beta phase.")+"</p>"
//            "<p>"+QObject::tr("Things are not perfect yet, but the focus from now is putting on the finishing touches. ")+
//            QObject::tr("This version brings support for the new Philips Respironics DreamStation, and older PRS1 1060P models.")+
//            "</p></font>";
;
    QFile clfile(":/docs/release_notes.html");
    QString changeLog = QObject::tr("Sorry, could not locate changelog.");
    if (clfile.open(QIODevice::ReadOnly)) {
        QTextStream ts(&clfile);
        //Todo, write XML parser and only show the latest..
        changeLog = ts.readAll();
    }


    QString html = "<html>"
    //"<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"></head>"
    "<body>";
    //"<h2><p>"+QObject::tr("Greetings!")+"</p></h2>";

//    html += welcomeMessage;


    if (ReleaseStatus != "r") {
        html += "<p><font color='red' size=+1><b>"+QObject::tr("Important:")+"</b></font> "
        "<font size=+1><i>"+QObject::tr("As this is a pre-release version, it is recommended that you back up your data folder manually before proceding, because attempting to roll back later may break things.")+"</i></font></p>";
    }

//    html += "<p><b>"+QObject::tr("Sleep Well, and good luck!")+"</b></p>"
//            "<p><b><i>"+"JediMark"+"</i></b></p><br/><b><i>"+QObject::tr("Change log")+"</i></b><hr/><br/><br/>";

    html += changeLog;
    html += "</body></html>";

    //QUrl("qrc:/docs/release_notes.html")

    // Should read these from online!!! with language code
    web->setHtml(html);

    //web.page()->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOn);
    relnotes.setLayout(layout);
    layout->insertLayout(0, hlayout, 0);
    layout->insertWidget(1, web, 1);
    QPushButton * okbtn = new QPushButton(QObject::tr("&Ok, get on with it.."), &relnotes);
    relnotes.connect(okbtn, SIGNAL(clicked()), SLOT(accept()));
    layout->insertWidget(2, okbtn, 0);
    QApplication::processEvents(); // MW: Needed on Mac, as the html has to finish loading

    relnotes.exec();
}

void sDelay(int s)
{
    // QThread::msleep() is exposed in Qt5
#ifdef Q_OS_WIN32
    Sleep(s * 1000);
#else
    sleep(s);
#endif
}

int compareVersion(QString version);


void MigrateSettings()
{
    QSettings settings(getDeveloperName(), getAppName());
    if (settings.contains("Version")) { return; } // done, we are new

    QSettings oldcopy(getDeveloperName(), getAppName()+"-Testing");
    if (oldcopy.contains("Migrated")) { return; }

    QStringList keys = oldcopy.allKeys();


    for (int i=0; i<keys.size(); ++i) {
        const QString & key = keys[i];
        settings.setValue(key, oldcopy.value(key));
    }

    oldcopy.setValue("Migrated", true);
    settings.setValue("Version", VersionString);

    qDebug() << keys;

}


int main(int argc, char *argv[])
{
#ifdef Q_WS_X11
    XInitThreads();
#endif

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    QGL::setPreferredPaintEngine(QPaintEngine::OpenGL);
#endif

    bool dont_load_profile = false;
    bool force_data_dir = false;
    bool changing_language = false;
    QString load_profile = "";

    QApplication a(argc, argv);
    QStringList args = QCoreApplication::arguments();

    // Ok, first things first... Migrate the -Testing QSettings over
    MigrateSettings();

    QSettings settings(getDeveloperName(), getAppName());

    QString lastlanguage = settings.value(LangSetting, "").toString();
    if (lastlanguage.isEmpty())
        changing_language = true;

    for (int i = 1; i < args.size(); i++) {
        if (args[i] == "-l") { dont_load_profile = true; }
        else if (args[i] == "-d") { force_data_dir = true; }
        else if (args[i] == "-language") {
            changing_language = true;

            // reset to force language dialog
            settings.setValue(LangSetting,"");
        } else if (args[i] == "-p") {
            sDelay(1);
        } else if (args[i] == "--profile") {
            if ((i+1) < args.size()) {
                load_profile = args[++i];
            } else {
                fprintf(stderr, "Missing argument to --profile\n");
                exit(1);
            }
        } else if (args[i] == "--datadir") { // mltam's idea
            QString datadir ;
            if ((i+1) < args.size()) {
              datadir = args[++i];
              settings.setValue("Settings/AppRoot", datadir);
            } else {
              fprintf(stderr, "Missing argument to --datadir\n");
              exit(1);
            }
          }
    }

    initializeLogger();


    ////////////////////////////////////////////////////////////////////////////////////////////
    // Language Selection
    ////////////////////////////////////////////////////////////////////////////////////////////
    initTranslations(settings);

    initializeStrings(); // Important, call this AFTER translator is installed.
    a.setApplicationName(STR_TR_SleepyHead);


    ////////////////////////////////////////////////////////////////////////////////////////////
    // OpenGL Detection
    ////////////////////////////////////////////////////////////////////////////////////////////
    float glversion = 0.0;
#ifndef NO_OPENGL_BUILD
    glversion = getOpenGLVersion();
#endif

    bool opengl2supported = glversion >= 2.0;
    bool bad_graphics = !opengl2supported;
    bool intel_graphics = false;
#ifndef NO_OPENGL_BUILD
    getOpenGLVersionString().contains("INTEL", Qt::CaseInsensitive);
#endif

//#if defined(Q_OS_WIN)
//    bool angle_supported = getGraphicsEngine().contains(CSTR_GFX_ANGLE, Qt::CaseInsensitive) && (QSysInfo::windowsVersion() >= QSysInfo::WV_VISTA);
//    if (bad_graphics) {
//        bad_graphics = !angle_supported;
//    }
//#endif

#ifdef BROKEN_OPENGL_BUILD
    Q_UNUSED(bad_graphics)
    Q_UNUSED(intel_graphics)

    const QString BetterBuild = "Settings/BetterBuild";

    if (opengl2supported) {
        if (!settings.value(BetterBuild, false).toBool()) {
            QMessageBox::information(nullptr, QObject::tr("A faster build of SleepyHead may be available"),
                             QObject::tr("This build of SleepyHead is a compatability version that also works on computers lacking OpenGL 2.0 support.")+"<br/><br/>"+
                             QObject::tr("However it looks like your computer has full support for OpenGL 2.0!") + "<br/><br/>"+
                             QObject::tr("This version will run fine, but a \"<b>%1</b>\" tagged build of SleepyHead will likely run a bit faster on your computer.").arg("-OpenGL")+"<br/><br/>"+
                             QObject::tr("You will not be bothered with this message again."), QMessageBox::Ok, QMessageBox::Ok);
            settings.setValue(BetterBuild, true);
        }
    } /*else {
#if defined(Q_OS_WIN)
        if (angle_supported) {
            if (!settings.value(betterbuild, false).toBool()) {
                QMessageBox::information(nullptr, fasterbuildavailable,
                             QObject::tr("This build of SleepyHead was designed to work with older computers lacking OpenGL 2.0 support, which yours doesn't have, but there may still be a better version available for your computer.") + "<br/><br/>"+
                             betterresults.arg("-ANGLE")+"<br/><br/>"+
                             QObject::tr("If you are running this in a virtual machine like VirtualBox or VMware, please disregard this message, as no better build is available.")+"<br/><br/>"+
                             lookfor + "<br/><br/>"+
                             notbotheragain, QMessageBox::Ok, QMessageBox::Ok);
                settings.setValue(betterbuild, true);
            }
        }
#endif
    } */
#else
    if (bad_graphics) {
        QMessageBox::warning(nullptr, QObject::tr("Incompatible Graphics Hardware"),
                             QObject::tr("This build of SleepyHead requires OpenGL 2.0 support to function correctly, and unfortunately your computer lacks this capability.") + "<br/><br/>"+
                             QObject::tr("You may need to update your computers graphics drivers from the GPU makers website. %1").
                                arg(intel_graphics ? QObject::tr("(<a href='http://intel.com/support'>Intel's support site</a>)") : "")+"<br/><br/>"+
                             QObject::tr("Because graphs will not render correctly, and it may cause crashes, this build will now exit.")+"<br/><br/>"+
                             QObject::tr("There is another build available tagged \"<b>-BrokenGL</b>\" that should work on your computer.")
                             ,QMessageBox::Ok, QMessageBox::Ok);
        exit(1);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Datafolder location Selection
    ////////////////////////////////////////////////////////////////////////////////////////////
    bool change_data_dir = force_data_dir;

    bool havefolder = false;

    if (!settings.contains("Settings/AppRoot")) {
        change_data_dir = true;
    } else {
        QDir dir(GetAppRoot());

        if (!dir.exists()) {
            change_data_dir = true;
        } else { havefolder = true; }
    }

    if (!havefolder && !force_data_dir) {
        if (QMessageBox::question(nullptr, STR_MessageBox_Question,
                QObject::tr("Would you like SleepyHead to use this location for storing its data?")+"\n\n"+
                QDir::toNativeSeparators(GetAppRoot())+"\n\n"+
                QObject::tr("If you are upgrading, don't panic, you just need to make sure this is pointed at your old SleepyHead data folder.")+"\n\n"+
                QObject::tr("(If you have no idea what to do here, just click yes.)"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
            settings.setValue("Settings/AppRoot", GetAppRoot());
            change_data_dir = false;
        }
    }

retry_directory:

    if (change_data_dir) {
        QString datadir = QFileDialog::getExistingDirectory(nullptr,
                          QObject::tr("Choose or create new folder for SleepyHead data"), GetAppRoot(),
                          QFileDialog::ShowDirsOnly);

        if (datadir.isEmpty()) {
            if (!havefolder) {
                QMessageBox::information(nullptr, QObject::tr("Exiting"),
                                         QObject::tr("As you did not select a data folder, SleepyHead will exit.")+"\n\n"+QObject::tr("Next time you run, you will be asked again."));
                return 0;
            } else {
                QMessageBox::information(nullptr, STR_MessageBox_Warning,
                                         QObject::tr("You did not select a directory.")+"\n\n"+QObject::tr("SleepyHead will now start with your old one.")+"\n\n"+
                                         QDir::toNativeSeparators(GetAppRoot()), QMessageBox::Ok);
            }
        } else {
            QDir dir(datadir);
            QFile file(datadir + "/Preferences.xml");

            if (!file.exists()) {
                if (dir.count() > 2) {
                    // Not a new directory.. nag the user.
                    if (QMessageBox::question(nullptr, STR_MessageBox_Warning,
                                              QObject::tr("The folder you chose is not empty, nor does it already contain valid SleepyHead data.")
                                              + "\n\n"+QObject::tr("Are you sure you want to use this folder?")+"\n\n"
                                              + datadir, QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
                        goto retry_directory;
                    }
                }
            }

            settings.setValue("Settings/AppRoot", datadir);
            qDebug() << "Changing data folder to" << datadir;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Register Importer Modules for autoscanner
    ////////////////////////////////////////////////////////////////////////////////////////////
    initialize();
    PRS1Loader::Register();
    ResmedLoader::Register();
    IntellipapLoader::Register();
    FPIconLoader::Register();
    WeinmannLoader::Register();
    CMS50Loader::Register();
    CMS50F37Loader::Register();
    MD300W1Loader::Register();
    //ZEOLoader::Register(); // Use outside of directory importer..

    schema::setOrders();


    ///////////////////////////////////////////////////////////////////////////////////////////
    // Initialize preferences system (Don't use PREF before this point)
    ///////////////////////////////////////////////////////////////////////////////////////////
    p_pref = new Preferences("Preferences");
    PREF.Open();
    AppSetting = new AppWideSetting(p_pref);

    // Clean up some legacy crap
    QFile lf(PREF.Get("{home}/Layout.xml"));
    if (lf.exists()) {
        lf.remove();
    }

    PREF.Erase(STR_AppName);
    PREF.Erase(STR_GEN_SkipLogin);

    // Todo: Make a wrapper for Preference settings, like Profile settings have..
    QDateTime lastchecked, today = QDateTime::currentDateTime();


    ////////////////////////////////////////////////////////////////////////////////////////////
    // Check when last checked for updates..
    ////////////////////////////////////////////////////////////////////////////////////////////
    bool check_updates = false;

    if (AppSetting->updatesAutoCheck()) {
        int update_frequency = AppSetting->updateCheckFrequency();
        int days = 1000;
        lastchecked = AppSetting->updatesLastChecked();

        if (lastchecked.isValid()) {
            days = lastchecked.secsTo(today);
            days /= 86400;
        }

        if (days > update_frequency) {
            check_updates = true;
        }
    }

    int vc = compareVersion(AppSetting->versionString());
    if (vc < 0) {
        release_notes();

        check_updates = false;
    } else if (vc > 0) {
        if (QMessageBox::warning(nullptr, STR_MessageBox_Error,
            QObject::tr("The version of SleepyHead you just ran is OLDER than the one used to create this data (%1).").
                        arg(AppSetting->versionString()) +"\n\n"+
            QObject::tr("It is likely that doing this will cause data corruption, are you sure you want to do this?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No) {

            return 0;
        }

    }

    AppSetting->setVersionString(VersionString);

    //    int id=QFontDatabase::addApplicationFont(":/fonts/FreeSans.ttf");
    //    QFontDatabase fdb;
    //    QStringList ffam=fdb.families();
    //    for (QStringList::iterator i=ffam.begin();i!=ffam.end();i++) {
    //        qDebug() << "Loaded Font: " << (*i);
    //    }


    if (!PREF.contains("Fonts_Application_Name")) {
#ifdef Q_OS_WIN
        // Windows default Sans Serif interpretation sucks
        // Segoe UI is better, but that requires OS/font detection
        PREF["Fonts_Application_Name"] = "Arial";
#else
        PREF["Fonts_Application_Name"] = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
#endif
        PREF["Fonts_Application_Size"] = 10;
        PREF["Fonts_Application_Bold"] = false;
        PREF["Fonts_Application_Italic"] = false;
    }


    QApplication::setFont(QFont(PREF["Fonts_Application_Name"].toString(),
                                PREF["Fonts_Application_Size"].toInt(),
                                PREF["Fonts_Application_Bold"].toBool() ? QFont::Bold : QFont::Normal,
                                PREF["Fonts_Application_Italic"].toBool()));

    qDebug() << "Selected Font" << QApplication::font().family();

    // Scan for user profiles
    Profiles::Scan();

    if (!dont_load_profile) {
        // TODO: set the don't automatically load profile AppSetting
    }

    Q_UNUSED(changing_language)

    MainWindow w;
    mainwin = &w;

    if (check_updates) { mainwin->CheckForUpdates(); }

    w.show();

    return a.exec();
}
