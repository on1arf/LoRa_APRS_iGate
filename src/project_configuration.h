#ifndef PROJECT_CONFIGURATION_H_
#define PROJECT_CONFIGURATION_H_

#include <BoardFinder.h>
#include <configuration.h>

class Configuration {
public:
  class Wifi {
  public:
    class AP {
    public:
      String SSID;
      String password;
    };

    Wifi() {
    }

    std::list<AP> APs;
  };

  class Beacon {
  public:
    Beacon() : message("LoRa iGATE & Digi, Info: github.com/peterus/LoRa_APRS_iGate"), positionLatitude(0.0), positionLongitude(0.0), timeout(15) {
    }

    String message;
    double positionLatitude;
    double positionLongitude;
    int    timeout;
  };

  class APRS_IS {
  public:
    APRS_IS() : active(true), server("euro.aprs2.net"), port(14580) {
    }

    bool   active;
    String passcode;
    String server;
    int    port;
  };

  class Digi {
  public:
    Digi() : active(false), beacon(true), fillin(false) , maxhopnowide(3), invalidcalls("NOCALL,INVALID"), donotdigi("NOCALL,INVALID"),destination("APLG01"){
    }

    bool active;
    bool beacon;
    bool fillin;
    int maxhopnowide;
    String invalidcalls;
    String donotdigi;
    String destination;
  };

  class LoRa {
  public:
    LoRa() : frequencyRx(433775000), frequencyTx(433775000), power(20), spreadingFactor(12), signalBandwidth(125000), codingRate4(5) {
    }

    long frequencyRx;
    long frequencyTx;
    int  power;
    int  spreadingFactor;
    long signalBandwidth;
    int  codingRate4;
  };

  class Display {
  public:
    Display() : alwaysOn(true), timeout(10), overwritePin(0), turn180(true) {
    }

    bool alwaysOn;
    int  timeout;
    int  overwritePin;
    bool turn180;
  };

  class Ftp {
  public:
    class User {
    public:
      String name;
      String password;
    };

    Ftp() : active(false) {
    }

    bool            active;
    std::list<User> users;
  };

  Configuration() : callsign("NOCALL-10"), board(""), ntpServer("pool.ntp.org"){};

  String  callsign;
  Wifi    wifi;
  Beacon  beacon;
  APRS_IS aprs_is;
  Digi    digi;
  LoRa    lora;
  Display display;
  Ftp     ftp;
  String  board;
  String  ntpServer;
};

class ProjectConfigurationManagement : public ConfigurationManagement {
public:
  explicit ProjectConfigurationManagement() : ConfigurationManagement("/is-cfg.json") {
  }
  virtual ~ProjectConfigurationManagement() {
  }

private:
  virtual void readProjectConfiguration(DynamicJsonDocument &data, Configuration &conf) override;
  virtual void writeProjectConfiguration(Configuration &conf, DynamicJsonDocument &data) override;
};

#endif
