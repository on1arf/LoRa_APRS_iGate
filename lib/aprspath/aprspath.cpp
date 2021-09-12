//#define _GLIBCXX_USE_C99

#include <iostream>
//#include <string>
#include <WString.h>
#include <list>
#include <cstring>
#include <vector>


#define DEFAULTMAXPATH 3


using std::endl;
using std::cout;
using std::vector;
//using std::string;

using std::list;


// APRS path parser, based on http://wa8lmf.net/DigiPaths/





class pathnode {
public:
    String callsign;
    int ssid;
    bool digipeat;
    bool valid;
    bool wide;
    int widetotal;
    int wideremain;

    pathnode (String callsign_in){
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
        if (callsign_in.isEmpty()) return;
        
        

        thiscall=callsign_in;

        int l=thiscall.length();

        // check if the call is a digipeat
        if (thiscall.end()[0] == '*') {
            digipeat=true;
            thiscall.remove(l-2); // pop last char .. (note: 'remove' index starts at 0, hence the '-2')
            l-=1;
        }



        p=thiscall.indexOf('-');

        // special case: strings ends with a '-' (shouldn't happen, but you never know)
        if (p == l-1) return;


        if (p == -1) {
            // no '-' found
            ssid=-1;

            // we have a valid callsign
            callsign=thiscall;
        } else {
            // '-' is found
            p2=thiscall.substring(p+1); // get part after the '-'
            try {
                p2i=(int)p2.toInt();
                //p2i=stoi(p2);
            }
            catch (const std::invalid_argument& ia) {
                // part after the '-' isn't numeric
                return;
            }

            // we have a valid callsign and ssid
            ssid=p2i;
            callsign=thiscall.substring(0,p);
            l=p; // reduce length to the part before the '-'
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
            return;
        }

        // some additional checks
        if ((p2i < 1) || (p2i > 2)) {
            // only WIDE1 and WIDE2 are accepted
            // (note: callsign is still marked as not valid)
            return;
        }

        // check: the 'wide total' should be larger or equal to the remaining number of hops
        // Note: remaining number of hops is found in the ssid-part2
        if (p2i < ssid) return; // note, callsign is still marked as not valid

        // we have a valid 'wide' call
        wide=true;

        widetotal=p2i;

        if (ssid == -1) {
            // no ssid found: remaining is 0
            wideremain=0;
        } else {
            wideremain=ssid;
        }

        // DONE
        valid=true;

        return;
    }


    bool equalcall(pathnode d){
        if ((callsign==d.callsign) && (ssid=d.ssid)) {
            return true;
        }

        // else
        return false;

    }

};

/*
int invalidcallcheck (char * invalidcalllist, string call2check) {
    bool ret=true;

    pathnode call2check_pn = pathnode(call2check);

	char * p;
	p=strtok(invalidcalllist,",");
	
	while (p) {
		if (!call2check_pn.equalcall(string(p))) {
            ret=false;
            break;
        };
		p=strtok(NULL, ",");
	};

    return ret;

};
*/


class aprspath {
    bool verified;
    list<pathnode> path;
    pathnode *wi[2];
    int wicount;
    int nodecount;
    int maxhopnowide;

    public:
    aprspath(){
        aprspath(DEFAULTMAXPATH);
    }

    aprspath(int maxhopnw) {
        verified=false;
        path.clear();
        wi[0]=NULL;
        wi[1]=NULL;
        wicount=0;
        nodecount=0;
        maxhopnowide=maxhopnw;
    }

    void appendnodetopath(pathnode p) {
        if (!p.valid) {
            cout << "Error: Trying to add incorrect node" << endl;
            throw("Error: invalid node added");
        }; 

        // add element to list if it valid
        path.push_back(p);
        nodecount+=1;
        verified=false; // reset "verified"

    }

    bool checkpath() {
        // the list should not be empty
        if (path.empty()) {
            cout << "Error: list is empty" << endl;
            return(false);
        };

        // the first node of the list should be a non-'WIDE' call
        std::list<pathnode>::iterator firstnode=path.begin();
        if (firstnode->wide) {
            cout << "Error. first element is wide" << endl;
            return(false);
        };




        // go through all elements of the list, find all WIDE pathnodes
        wicount=0;

        std::list<pathnode>::iterator it;
        for (it = firstnode; it != path.end(); ++it){
            if (it->wide) {
                if (wicount < 2) {
                    wi[wicount]=&(*it);
                };
                wicount+=1;
            };
        }

        // maximum two WIDE nodes in the path 
        if (wicount > 2) {
            cout << "Error: More then 2 WIDE nodes" << endl;
            return(false);
        };

        // if there are two WIDE nodes, the first one should be a WIDE1
        if (wicount == 2) {
            if (wi[0]->widetotal != 1) {
                cout << "Error: First WIDE should be WIDE1 if there are two WIDE nodes in the path" << endl;
                return(false);
            }
        }
        // done
        verified=true;

        return(true);

    }


    int doloopcheck (String mycall) {
        // create pathnode object based on mycall
        pathnode p(mycall);

        if (!p.valid) {
            return(-1);
        }
        
        std::list<pathnode>::iterator it;
        for (it = path.begin(); it != path.end(); ++it){
            if (p.equalcall(*it)) return 1;
//            if ((it->callsign == p.callsign) && (it->ssid == p.ssid)){
//               return(1);
//            };
        }
        return(0);
    }

    bool adddigi (String mycall, bool fillin){
        pathnode p(mycall);

        if (!p.valid) {
            return(false);
        }

        p.digipeat=true;  // mark current node as a digipeater


        // check if the path has been verified
        if (!verified) {
            cout << "Error: Path is not yet verified" << endl;
            throw("Error: path is not verified");
        }

        if (wicount==0) {
            // senario 1: no "WIDE" nodes
            // if less then 4 nodes: add pathnode to the end, mark it as digipeat
            // (in reality, these are 3 "real" nodes, as node0 in the path is the "A" callsign)
            // else, refuse
            // note, non-WIDE paths are processed in the same way by both wide and fill-in digipeaters
            if (nodecount >= (maxhopnowide + 1)){
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
        if ((fillin) && (wi[0]->widetotal != 1)){
            // a fillin node only should react to a WIDE1-1 path
            // if not, stop
            return(false);
        }


        // check it the node can be added in front of the the 1st WIDE node
        // note: if the first WIDE is marked as digipeater, this should be consider as 'remain=0'  
        if ((wi[0]->wideremain > 0) && (!wi[0]->digipeat)) {
//            wi[0]->wideremain-=1;


            std::list<pathnode>::iterator it;
            for (it = path.begin(); it != path.end(); ++it){
                if (&(*it) == wi[0]) {
                    // make the WIDE node digipeater for WIDE1, or WIDE2 if remain is  0 at the end (i.e. is currently 1 )
                    if ((wi[0]->widetotal == 1) || (wi[0]->wideremain == 1)) {
                        wi[0]->digipeat=true;
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

                    // make the WIDE node digipeater is remain is  0
                    if (wi[1]->wideremain == 0) {
                        wi[1]->digipeat=true;
                    }
                    break;
                };
            }

            // DONE
            return(true);
        }

        // NO other options anymore. STOP here
        return(false);
    }


    String printfullpath() {
        String ret = "";
        int nodecount=0;

        std::list<pathnode>::iterator it;
        for (it = path.begin(); it != path.end(); ++it){
            pathnode p = *it;

            if (nodecount > 0) {ret += ",";}
            nodecount += 1;

            // call
            ret += p.callsign;


            if (p.wide) {
                // "WIDE" call -> check widremain
                if (p.wideremain != 0) {
                    ret += ("-" + String(p.wideremain));
                };
            } else {
                // not a "WIDE" call -> check ssid
                if (p.ssid != -1) {
                    ret += ("-" + String(p.ssid));
                };
            }

            if (p.digipeat) {
                ret += "*";
            }
        }

        return(ret);
    }


};



/////////////////////////
// some support functions

vector<String> splitchrp2v(char * in, char token) {
    vector<String> ret;

	char * p;
	p=strtok(in,&token);
	
	while (p) {
		ret.push_back(String(p));
		p=strtok(NULL, ",");
	};

    return ret;
};


bool isinvector(vector <pathnode> pathvector, pathnode element) {
    for(auto thisnode:pathvector) {
        if (thisnode.equalcall(element)) {
            return true;
        };
    }
    return false;

}; 

