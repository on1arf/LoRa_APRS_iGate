#ifndef APRSPATH_H
#define APRSPATH_H


#include <iostream>
//#include <string>
#include <WString.h>
#include <list>
#include <vector>

using std::vector;
//using std::string;
using std::list;


// APRS message parser, based on http://wa8lmf.net/DigiPaths/
// header file


class pathnode {
public:
    String callsign;
    int ssid;
    bool digipeat;
    bool valid;
    bool wide;
    int widetotal;
    int wideremain;

    pathnode (String callsign_in);
    bool equalcall(pathnode d, bool checkssid);
    String pathnode2str();
};


//int invalidcallcheck (char * invalidcalllist, string call2check);


class aprspath {
private:
    bool verified;
    list<pathnode> path;
    pathnode * lastnode;
    pathnode *wi[2];
    int wicount;
    int nodecount;
    int maxhopnowide;

public:
    aprspath(int maxhopnw);

    pathnode * getlastnode();

    bool appendnodetopath(pathnode p);
    bool checkpath();

    bool doloopcheck (String mycall);

    bool adddigi (String mycall, bool fillin);

    String printfullpath();
};


/////////////////////////
// some support functions

vector<String> splitstr2v(String in, char token);

bool isinvector(vector <pathnode> pathvector, pathnode element, bool checkssid); 
bool strisalphanum(String s);

#endif
