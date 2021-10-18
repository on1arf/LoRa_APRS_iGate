// MESSAGE HISTORY
// version 0.0: Kristoff Bonne (on1arf) 12/10/2021

#include <APRSMessage.h>

#include <list>
#include <memory>


using std::list;
using std::shared_ptr;


#define found_notfound 0
#define found_shortdist 1
#define found_longdist 2

class msghist_elem {
public:
    msghist_elem(shared_ptr<APRSMessage> aprsmsg, int distance);
    bool checkexist_sender(String dest, int lastts);
    bool checkexist_dup(shared_ptr<APRSMessage>  aprsmsg, int lastts);

    shared_ptr<APRSMessage>  msg;
    int lastseen;
    int distance;
};

class msghist {
public:
    msghist(); // empty creator
    void configure (int keeptime, int keeptime_shortdist, int checktimedup);

    int checkexist_sender(String dest);
    bool checkexist_dup(shared_ptr<APRSMessage>  aprsmsg);
    void cleanup();

    void receivemsg(shared_ptr<APRSMessage>  aprsmsg, int distance);


private:
    list<msghist_elem>lhlist;

    int conf_keeptime;
    int conf_keeptime_shortdist;
    int conf_checktimedup;
};

