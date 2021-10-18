// MESSAGE HISTORY
// version 0.0: Kristoff Bonne (on1arf) 12/10/2021


#include <list>
#include <memory>

using std::list;
using std::shared_ptr;

#include <msghist.h>
#include <APRSMessage.h>




msghist_elem::msghist_elem(shared_ptr<APRSMessage>aprsmsg, int distance_in) {
    msg=aprsmsg;
    distance=distance_in;
    lastseen=millis();
}


bool msghist_elem::checkexist_dup(shared_ptr<APRSMessage> aprsmsg, int lastts) {
    // returns true to source, destination and body is the same
    // NOTE: do not check the path
    return (
            (lastseen >= lastts)
        &&  (msg->getSource() == aprsmsg->getSource())
        &&  (msg->getDestination() == aprsmsg->getDestination())
        &&  (msg->getRawBody() == aprsmsg->getRawBody())
        );
}


bool msghist_elem::checkexist_sender(String dest, int lastts) {
    // returns true to source, destination and body is the same
    // NOTE: do not check the path
    return (
            (lastseen >= lastts)
        &&  (msg->getSource() == dest)
        );
}




msghist::msghist (){
    lhlist.clear(); // init list
};

void msghist::configure (int keeptime, int keeptime_shortdist, int checktimedup){
    // configuration parameters
    conf_keeptime=keeptime;
    conf_keeptime_shortdist=keeptime_shortdist;
    conf_checktimedup=checktimedup;
};





void msghist::cleanup(){
    std::list<msghist_elem>::iterator it;

    int lastts;
    int now=millis();

    // stop if the list is empty (nothing to check)
    if (lhlist.empty()) return;

    // first check the the current timestamp is not less then the timestamp of all elements
    // if yes, this means that 'millis' has wrapped around
    bool milliswrapped=true;

    for (it = lhlist.begin(); it != lhlist.end(); ++it) { 
        if (it->lastseen < now) {
            milliswrapped=false;
            break;
        };
    }; // end for

    // millis has wrapped, reset all timers to 0
    if (milliswrapped) {
        for (it = lhlist.begin(); it != lhlist.end(); ++it) {
            it->lastseen=0;
        }; // end for
    }


    // erase element from list:
    // shameless stolen from https://stackoverflow.com/questions/596162/can-you-remove-elements-from-a-stdlist-while-iterating-through-it
    lastts=millis()-conf_keeptime;

    for (it = lhlist.begin(); it != lhlist.end(); ++it){ 
        if (it->lastseen < lastts){
            it=lhlist.erase(it);
            --it;  // as it will be add again in for, so we go back one step
        }
    }
}

//    bool checkexist_sender(APRSMessage aprsmsg);
bool msghist::checkexist_dup(shared_ptr<APRSMessage>aprsmsg) {
    int lastts=millis()-conf_checktimedup;


    std::list<msghist_elem>::iterator it;
    for (it = lhlist.begin(); it != lhlist.end(); ++it){ 
        if (it->checkexist_dup(aprsmsg, lastts)) {
            return true;
        }
    }

    return false;
};



int msghist::checkexist_sender(String dest) {
    int found = 0;
    int now=millis();

    int lastts=now-conf_keeptime;


    // check if the sender exists, compair to "keeptime"
    std::list<msghist_elem>::iterator it;
    for (it = lhlist.begin(); it != lhlist.end(); ++it){ 
        if (it->checkexist_sender(dest, lastts)) {
            found=1;
            break;
        }
    }

    // if not found, exit here
    if (not found) return found_notfound;

    // if found, check if more or less then the "short distance" timer
    lastts=now-conf_keeptime_shortdist;

    if (it->checkexist_sender(dest, lastts)) {
        // found 
        return found_longdist;
    }


    return found_shortdist;
};

void msghist::receivemsg(shared_ptr<APRSMessage> aprsmsg, int distance) {
    int now = millis();

    int lastts=now-conf_keeptime;

    // check if the sender exists
    std::list<msghist_elem>::iterator it;
    for (it = lhlist.begin(); it != lhlist.end(); ++it){ 
        if (it->checkexist_sender(aprsmsg->getSource(), lastts)) {
            // found .. update timestamp
            it->lastseen=now;
        }
    };  // end for

    // message does not yet exist -> add a copy to the list
    lhlist.push_back(msghist_elem(aprsmsg, distance));


};
