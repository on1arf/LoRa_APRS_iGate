#include <WString.h>

#include <aprspath.h>

class aprsmsgmsg {
public:
    pathnode * callsign;

    String body;
    bool hasack;
    bool isack;
    bool isrej;
    String msgno;

    bool valid;


    aprsmsgmsg ();
    aprsmsgmsg (String msgin);
    ~aprsmsgmsg ();

    String fulltxt ();

};
