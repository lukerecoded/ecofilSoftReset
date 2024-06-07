#ifndef CHECKRESET_H
#define CHECKRESET_H

#include <QObject>
#include <QTimer>
#include <QSettings>
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QDateTime>
#include <QDir>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <QElapsedTimer>

#include <linux/version.h>
#include <linux/input.h>

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <ctype.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "../ecofil/structSm.h"
#include "../ecofil/path.h"

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"

class checkReset : public QObject
{
    Q_OBJECT

public:
    explicit checkReset(QObject *parent = nullptr);
    int start();

private:
    QTimer *mTimerSW;
    QTimer *mTimerHW;
    QElapsedTimer elapsedTimer;
    void readConfigFile();
    int getSharedMemory();

    struct regionIO *rptr;
    int fd;

    int idInputRiavvio;
    bool resetRequest;
    bool prevResetRequest;
    bool salvaEvento(QString idevento, QString data);
    void riavviaSistema();
    bool onStartup;
    void CheckResetHardware();

    int idOutputAlimentazionePannello;
    bool eventoInviato;

    int ElapsedTimeForSoftReset = 2000;
    int ElapsedTimeForHwReset = 10000;
    bool timerStarted = false;
    bool riavvioEseguito = false;

    int codiceStazione;
    int idComune;

private slots:
    void tickSW();
    void tickHW();


};

#endif // CHECKRESET_H
