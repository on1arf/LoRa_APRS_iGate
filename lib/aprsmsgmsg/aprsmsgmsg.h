#include <WString.h>

#include <aprspath.h>

#include <memory>

using std::shared_ptr;

class aprsmsgmsg {
public:
    shared_ptr<pathnode> callsign;

    String body;
    bool hasackreq;
    bool isack;
    bool isrej;
    String msgno;
    String msgno2;

    bool valid;


    aprsmsgmsg ();
    aprsmsgmsg (String msgin);


    String fulltxt ();

};
