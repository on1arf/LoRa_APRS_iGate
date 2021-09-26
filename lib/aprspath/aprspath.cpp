#include <aprspath.h>

//#define _GLIBCXX_USE_C99

#include <iostream>
//#include <string>
#include <WString.h>
#include <list>
#include <cstring>
#include <vector>

#include <locale>

#include <logger.h>


using std::vector;
//using std::string;

using std::list;


// APRS path parser, based on   



pathnode::pathnode (String callsign_in){
    String thiscall;

    size_t p;
    String p2;
    int p2i;

    // pre-init data
    callsign="";
    ssid=-1;
    digipeat=false;
    valid=false;
    wide=false;
    widetotal=0;
    wideremain=0;


    // special code for empty callsigns
    if (callsign_in.isEmpty()) {
        logPrintlnD("Error pathnode: callsign is empty");
        return;
    }
    
    

    thiscall=callsign_in;
    thiscall.toUpperCase(); // make uppercase

    int l=thiscall.length();

    // check if the call is a digipeat
    if (thiscall.endsWith("*")) {
        digipeat=true;
        thiscall.remove(l-1); // pop last char
        l-=1;
    }


    p=thiscall.indexOf('-');

    // special case: strings ends with a '-' (shouldn't happen, but you never know)
    if (p == l-1) {
        logPrintlnD("Error pathnode: callsign ending with a -");
        return;
    };


    if (p == -1) {
        // no '-' found
        ssid=0;

        // we have a valid callsign
        callsign=thiscall;
    } else {
        // '-' is found
        p2=thiscall.substring(p+1); // get part after the '-'
        p2i=(int)p2.toInt();
        
        if ((p2i==0) && ((p2 != "0") && (p2 != "00")) ) {
            // part after the '-' isn't numeric
            logPrintlnD("Error pathnode: part after - is not numeric");
            return;
        }


        if ((p2i < 0) || (p2i > 15)) {
            // APRS packets over RF should have a SSID between 0 and 15
            logPrintlnD("Error pathnode: SSID should between 0 and 15");
            return;
        }

        // we have a valid callsign and ssid
        ssid=p2i;
        callsign=thiscall.substring(0,p);
        l=p; // reduce length to the part before the '-'
    }


    // at this point, we should have a fully alphanumeric callsign
    if (!strisalphanum(callsign)) {
        logPrintlnD("Error pathnode: callsign is not alphanumeric");
        logPrintlnD(thiscall);
        return;
    }


    // check for 'WIDE'
    // we should have exactly 5 characters
    // 'WIDE' should be at the beginning of the callsign
    if ((l != 5) || callsign.indexOf("WIDE") != 0) {
        // 'wide' not found
        wide=false;

        // done
        valid=true;
        return;
    }
    
    p2=thiscall.substring(4); // get part after the 'WIDE'
    try {
        p2i=p2.toInt();
    }
    catch (const std::invalid_argument& ia) {
        // path after the 'WIDE' isn't numeric
        // (note: callsign is still marked as not valid)
        logPrintlnD("Error pathnode: part after WIDE is not numeric");
        return;
    }

    // some additional checks
    if ((p2i < 1) || (p2i > 2)) {
        // only WIDE1 and WIDE2 are accepted
        // (note: callsign is still marked as not valid)
        logPrintlnD("Error pathnode: WIDE should be WIDE1 or WIDE2");
        return;
    }

    // check: the 'wide total' should be larger or equal to the remaining number of hops
    // Note: remaining number of hops is found in the ssid-part2
    if (p2i < ssid) {
        logPrintlnD("Error pathnode: Wide TOTAL is less then wide REMAIN");
        return; // note, callsign is still marked as not valid
    }

    // we have a valid 'wide' call
    wide=true;

    widetotal=p2i;

    wideremain=ssid;

    // DONE
    valid=true;

    return;
}


bool pathnode::equalcall(pathnode d, bool checkssid){
    if ((callsign==d.callsign) && ((!checkssid) || (ssid==d.ssid))) {
        return true;
    }

    // else
    return false;

}


String pathnode::pathnode2str() {
    // if there is a ssid
    if (ssid) {
        return callsign + "-" + String(ssid);
    };

    // if not, just return the callsign
    return callsign;
};


aprspath::aprspath(int maxhopnw) {
    verified=false;
    path.clear();
    lastnode=NULL;
    wi[0]=NULL;
    wi[1]=NULL;
    wicount=0;
    nodecount=0;
    maxhopnowide=maxhopnw;
}



bool aprspath::appendnodetopath(pathnode p) {
    if (!p.valid) {
        logPrintlnD("Error appendnodetopath: Trying to add non-valid pathnode to aprspath");
        return(false);
    }; 

    // add element to list if it valid
    path.push_back(p);
    nodecount+=1;
    verified=false; // reset "verified"

    return true;

}

bool aprspath::checkpath() {


    // go through all elements of the list, find all WIDE pathnodes
    wicount=0;

    std::list<pathnode>::iterator it;
    for (it = path.begin(); it != path.end(); ++it){        
        if (it->wide) {
            if (wicount < 2) {
                wi[wicount]=&(*it);
            };
            wicount+=1;

            
        } else {
            // set "lastnode" to the last node that is not a "wide"
            lastnode=&(*it);
        };

        
    }

    // maximum two WIDE nodes in the path 
    if (wicount > 2) {
        logPrintlnD("Error: More then 2 WIDE nodes");
        return(false);
    };

    // if there are two WIDE nodes, the first one should be a WIDE1
    if (wicount == 2) {
        if (wi[0]->widetotal != 1) {
            logPrintlnD("Error: First WIDE should be WIDE1 if there are two WIDE nodes in the path");
            return(false);
        }
    }
    // done
    verified=true;

    return(true);

}


bool aprspath::doloopcheck (String mycall) {
    // create pathnode object based on mycall
    pathnode p(mycall);

    if (!p.valid) {
        logPrintD("Error doloopcheck: call ");
        logPrintD(mycall);
        logPrintlnD(" is not non-valid");
        return(false);
    }
    
    std::list<pathnode>::iterator it;
    for (it = path.begin(); it != path.end(); ++it) {   
        if (p.equalcall(*it, true)) return false;
    }
    return true;
}

bool aprspath::adddigi (String mycall, bool fillin){
    pathnode p(mycall);

    if (!p.valid) {
        logPrintlnD("Error adddigi: mycall has an invalid format");
        return(false);
    }

    p.digipeat=true;  // mark current node as a digipeater


    // check if the path has been verified
    if (!verified) {
        logPrintlnD("Error adddigi: Path is not yet verified");
        return false;
    }

    if (wicount==0) {
        // senario 1: no "WIDE" nodes
        // if less then "maxhopnowide" nodes: add pathnode to the end, mark it as digipeat
        // else, refuse
        // note, non-WIDE paths are processed identically, both by wide and fill-in digipeaters
        if (nodecount >= (maxhopnowide)){
            logPrintlnD("Warning adddigi/nowide: Already the maximum number of nodes.");
            // cannot be added, DONE
            return(false);
        }

        path.push_back(p);
        nodecount+=1;

        // DONE
        return(true);
    }; 

    // there are WIDE element,

    // for fill-in digipeaters, stop here if there is only one WIDE node, and it is not a WIDE-1
    if ((fillin) && (wicount == 1) && (wi[0]->widetotal != 1)){
        // a fillin node only should react to a WIDE1-1 path
        // if not, stop
        return(false);
    }


    // check it the node can be added in front of the the 1st WIDE node
    // note: if the first WIDE is marked as digipeater, this should be consider as 'remain=0'  
    if ((wi[0]->wideremain > 0) && (!wi[0]->digipeat)) {

        std::list<pathnode>::iterator it;
        for (it = path.begin(); it != path.end(); ++it){        
            if (&(*it) == wi[0]) {
                // make the WIDE node digipeater for WIDE1, or WIDE2 if remain is  0 at the end (i.e. is currently 1 )
                if ((wi[0]->widetotal == 1) || (wi[0]->wideremain == 1)) {
                    it->digipeat=true;
                }

                // only decrease wideremain for the "WIDE" digipeaters (not for fill-in nodes)
                if (!fillin) {
                    wi[0]->wideremain-=1;
                }
                path.insert(it,p);
                break;
            };
        }

        // DONE
        return(true);
    }

    // stop here if there is only one WIDE node
    if (wicount<2) {
        // cannot be added, DONE
        logPrintlnD("Warning adddigi/1wide: Remain has dropped to 0.");
        return(false);
    }

    // stop here for fill-in digipeaters
    if (fillin) {
        // cannot be added, DONE
        return(false);
    }


    // can we add it in front of the 2nd WIDE element?
    if (wi[1]->wideremain > 0) {
        wi[1]->wideremain-=1;


        std::list<pathnode>::iterator it;
        for (it = path.begin(); it != path.end(); ++it){
            if (&(*it) == wi[1]) {
                path.insert(it,p);

                // make the WIDE node digipeater if remain is 0
                if (wi[1]->wideremain == 0) {
                    it->digipeat=true;
                }
                break;
            };
        }

        // DONE
        return(true);
    }

    // NO other options anymore. STOP here
    logPrintlnD("Warning adddigi/2wide: Remain has dropped to 0.");

    return(false);
}


String aprspath::printfullpath() {
    String ret = "";
    int nodecount=0;

    std::list<pathnode>::iterator it;
    for (it = path.begin(); it != path.end(); ++it) {        
        pathnode p = *it;

        if (nodecount > 0) {ret += ",";}
        nodecount += 1;

        // call
        ret += p.callsign;


        if (p.wide) {
            // "WIDE" call -> check wideremain
            if (p.wideremain != 0) {
                ret += ("-" + String(p.wideremain));
            };
        } else {
            // not a "WIDE" call -> check ssid
            if (p.ssid != 0) {
                ret += ("-" + String(p.ssid));
            };
        }

        if (p.digipeat) {
            ret += "*";
        }
    }

    return(ret);
}



pathnode * aprspath::getlastnode() {
    return lastnode;
}

/////////////////////////
// some support functions



vector<String> splitstr2v(String in, char token) {
    vector<String> ret;

	int oldp=0;
    int p;

    if (in.isEmpty()) return ret; // return and empty vector if input string is empty

    p=in.indexOf(token);

	while (p>=0) {
		ret.push_back(in.substring(oldp,p));
        // skip current char
        oldp=p+1;
        p=in.indexOf(token,oldp);
	};

    // final part to the end
    ret.push_back(in.substring(oldp));

    return ret;
};



bool isinvector(vector <pathnode> pathvector, pathnode element, bool checkssid) {

    for(auto thisnode:pathvector) {
        if (thisnode.equalcall(element, checkssid)) return true;
    }
    return false;

}; 



bool strisalphanum(String s) {
    for (int c=0; c<s.length(); c++) {
        int thischar=s[c];

        // return false if character is not alphanumeric
        if (!isalnum(thischar))  {
            return false;
        }
    }
    //done
    return true;
}
