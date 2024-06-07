#include "checkreset.h"

checkReset::checkReset(QObject *parent) : QObject(parent)
{
    mTimerSW = new QTimer(this);
    connect(mTimerSW,SIGNAL(timeout()),this,SLOT(tickSW()));

    mTimerHW = new QTimer(this);
    connect(mTimerHW,SIGNAL(timeout()),this,SLOT(tickHW()));
}

void checkReset::readConfigFile(){
    idInputRiavvio= -1;
    QSettings settings(path::mp(), QSettings::IniFormat);
    idInputRiavvio = settings.value("DEVICE_D_INPUT/IDINPUTRIAVVIO",idInputRiavvio ).toInt();
    idOutputAlimentazionePannello = settings.value("DEVICE_D_OUTPUT/RELEALIMENTAZIONEPANNELLO",idOutputAlimentazionePannello ).toInt();

    codiceStazione= settings.value("GENERALE/ID",-1 ).toInt();
    idComune = settings.value("GENERALE/IDCOMUNE",-1 ).toInt();
}

int checkReset::start(){
    readConfigFile();
    if(idInputRiavvio < 0) {    //qui è necessario il controllo
        return -1;
    }
    onStartup = true;
    int retsh = getSharedMemory();
    if(retsh==0){
        mTimerSW->start(500);
        mTimerHW->start(1800000); //30 minuti
    }
    return retsh;
}

static int is_event_device(const struct dirent *dir) {
    return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}


static bool scan_devices()
{
    bool touchOK = false;

    struct dirent **namelist;
    int i, ndev, devnum;
    int max_device = 0;

    ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, versionsort);
    if (ndev <= 0)
        return -1;

    for (i = 0; i < ndev; i++)
    {
        char fname[64];
        int fd = -1;
        char name[256] = "???";

        snprintf(fname, sizeof(fname),"%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);
        fd = open(fname, O_RDONLY);
        if (fd < 0)
            continue;
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);

        close(fd);

        if(strstr(name, "eGalaxTouch") != NULL) {
            touchOK = true;
        }

        sscanf(namelist[i]->d_name, "event%d", &devnum);
        if (devnum > max_device)
            max_device = devnum;

        free(namelist[i]);
    }

    return touchOK;
}


void checkReset::CheckResetHardware(){
    if(!scan_devices()){
        if(!eventoInviato){
            salvaEvento("TF","TOUCH NON RILEVATO");
            eventoInviato = true;
        }
        if(idOutputAlimentazionePannello > 0){
            rptr->outCommand[idOutputAlimentazionePannello]=false;
        }
    }
    else{
        eventoInviato = false;
    }
}

void  checkReset::tickHW(){
    CheckResetHardware();
}


void  checkReset::tickSW(){

    //fronte di discesa
    resetRequest = rptr->binStatus[idInputRiavvio];
    if(resetRequest && !timerStarted){
        elapsedTimer.start();
        timerStarted = true;
    }
    if(!resetRequest){
        timerStarted = false;
        riavvioEseguito = false;
    }
    if(onStartup){
        if(resetRequest){
            resetRequest = false;
            timerStarted = false;
            riavvioEseguito = false;
        }
        onStartup = false;
    }
    else{
        if(timerStarted && elapsedTimer.elapsed()>=ElapsedTimeForSoftReset && !riavvioEseguito){
            riavviaSistema();
            riavvioEseguito = true;
        }
        if(timerStarted && elapsedTimer.elapsed()>=ElapsedTimeForHwReset){
             if(idOutputAlimentazionePannello > 0)
                 rptr->outCommand[idOutputAlimentazionePannello]=false;

        }
    }
    prevResetRequest = resetRequest;
}

bool checkReset::salvaEvento(QString idevento, QString data){
    bool ret;
    QString directoryEventi=path::directoryEventi();
    QString dataOra = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString fileName = QDateTime::currentDateTime().toString("yyyyMMddHHmmss").append(".dat");
    //crea la cartella locale se non esiste
    if(!QDir(directoryEventi).exists()){
        qDebug()<<"creo dir eventi";
        QDir().mkpath(directoryEventi);
    }

    fileName = QString(directoryEventi).append(fileName);

    //crea il file locale
    QFile file(fileName);
    if(file.open(QIODevice::WriteOnly | QIODevice::Text)){
        QTextStream mStream(&file);
        mStream << dataOra << ";" ;
        mStream << QString::number(idComune) << ";";
        mStream << QString::number(codiceStazione) << ";";
        mStream << idevento << ";";
        mStream << data << endl;
        file.close();

    }
    else
        ret = false;

    return ret;
}

void checkReset::riavviaSistema(){
    qDebug()<<"RICHIESTO RIAVVIO";
    system("killall ecofilDataManager");
    salvaEvento("RESET","");
    QThread::msleep(1000);
    system("reboot");
}

int checkReset::getSharedMemory(){
    fd = shm_open(smNames::sharedNameIO,O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); //apertura
    if(fd == -1) return -1;

    if(ftruncate(fd,sizeof(struct regionIO)) == -1) return -2;

    rptr = (regionIO*) mmap(NULL,sizeof(struct regionIO),PROT_READ | PROT_WRITE, MAP_SHARED, fd,0);
    if(rptr == MAP_FAILED) return -3;

    return 0;
}
