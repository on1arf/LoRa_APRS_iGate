#ifndef APRSPATH_H
#define APRSPATH_H


#include <iostream>
#include <WString.h>
#include <list>
#include <vector>

#include <memory>

using std::vector;
using std::list;
using std::shared_ptr;


#define normalnode false
#define specialnode true


#define docheckssid true
#define nocheckssid false

// APRS message parser, based on http://wa8lmf.net/DigiPaths/
// header file


class pathnode {
public:
    bool special;
    String callsign;
    int ssid;
    bool digipeat;
    bool valid;
    bool wide;
    int widetotal;
    int wideremain;
    bool configured;

    pathnode ();
    pathnode (String callsign_in, bool isspecial);
    void configure (String callsign_in, bool isspecial);
    bool equalcall(shared_ptr<pathnode> d, bool checkssid);
    String pathnode2str();
};


//int invalidcallcheck (char * invalidcalllist, string call2check);


class aprspath {
private:
    bool verified;
    list<shared_ptr<pathnode>> path;
    shared_ptr<pathnode> *lastnode;
    shared_ptr<pathnode> wi[2];
    int wicount;
    int nodecount;
    int maxhopnowide;
    int pathlength; // number of non-wide hops in a path
    bool configured; // if object is configured (via 'config' or constructor)


public:
    aprspath();
    aprspath(int maxhopnw);
    void configure(int maxhopnw);


    shared_ptr<pathnode> * getlastnode();

    bool appendnodetopath(shared_ptr<pathnode> p);
    bool appendpathtxttopath(String p, vector<shared_ptr<pathnode>>  invalidcalllist); 
    bool checkpath();

    bool doloopcheck (shared_ptr<pathnode> call_pn);

    bool adddigi (shared_ptr<pathnode> p, bool fillin);

    String printfullpath();

    int getpathlength();

    list<shared_ptr<pathnode>> getpath();
};


/////////////////////////
// some support functions

vector<String> splitstr2v(String in, char token);

bool isinvector(vector <shared_ptr<pathnode>> pathvector, shared_ptr<pathnode> element, bool checkssid); 
bool strisalphanum(String s);

#endif
