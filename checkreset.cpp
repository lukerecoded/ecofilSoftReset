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
    QSettings settings("/home/user/ECOFILL/GRD/data/mp.data", QSettings::IniFormat);
    idInputRiavvio = settings.value("GENERALE/IDINPUTRIAVVIO",idInputRiavvio ).toInt();
    idOutputRiavvioHw = settings.value("GENERALE/IDOUTPUTRIAVVIO",idOutputRiavvioHw ).toInt();

    qDebug() << "idInputRiavvio:"<<idInputRiavvio;
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
        if(idOutputRiavvioHw > 0){
            rptr->outCommand[idOutputRiavvioHw]=1;
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
    if(resetRequest==1 && !timerStarted){
        elapsedTimer.start();
        timerStarted = true;
    }
    if(resetRequest==0){
        timerStarted = false;
        riavvioEseguito = false;
    }
    if(onStartup){
        if(resetRequest == 1){
            resetRequest = 0;
            timerStarted = false;
            riavvioEseguito = false;
        }
        onStartup = false;
    }
    else{
        if(timerStarted && elapsedTimer.elapsed()>=ElapsedTimeForSoftReset && !riavvioEseguito){

            if(rptr->binStatus[0] != 1 && rptr->binStatus[2] != 1 && rptr->binStatus[8] != 1){
                    riavviaSistema();
                    riavvioEseguito = true;
             }
        }
        if(timerStarted && elapsedTimer.elapsed()>=ElapsedTimeForHwReset){
             if(idOutputRiavvioHw > 0)
                 rptr->outCommand[idOutputRiavvioHw]=1;

        }
    }
    prevResetRequest = resetRequest;
}

bool checkReset::salvaEvento(QString idevento, QString data){
    qDebug()<<"salva evento: " << idevento;
    bool ret;
    QString directoryEventi="/home/user/ECOFILL/GRD/data/eventi/";
    QSettings settings("/home/user/ECOFILL/GRD/data/mp.data", QSettings::IniFormat);
    int codicestazione= settings.value("GENERALE/ID",codicestazione ).toInt();
    int idComune = settings.value("GENERALE/IDCOMUNE",idComune ).toInt();
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
        mStream << QString::number(codicestazione) << ";";
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
    system("killall grdDataManager");
    salvaEvento("RESET","");
    QThread::msleep(1000);
    system("reboot");
}

int checkReset::getSharedMemory(){
    fd = shm_open("/modbus",O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); //apertura
    if(fd == -1) return -1;

    if(ftruncate(fd,sizeof(struct region)) == -1) return -2;

    rptr = (region*) mmap(NULL,sizeof(struct region),PROT_READ | PROT_WRITE, MAP_SHARED, fd,0);
    if(rptr == MAP_FAILED) return -3;

    return 0;
}
