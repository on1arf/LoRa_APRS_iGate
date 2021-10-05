#include <WString.h>

#include <aprspath.h>

class aprsmsg {
public:
    pathnode * callsign;

    String body;
    bool hasack;
    bool isack;
    bool isrej;
    String msgno;

    bool valid;


    aprsmsg (String msgin);
    ~aprsmsg ();

    String fulltxt ();

};
