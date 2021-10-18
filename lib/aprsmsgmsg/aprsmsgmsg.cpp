#include <logger.h>

#include <aprspath.h>
#include <aprsmsgmsg.h>

using std::make_shared;

// aprs "message" messages: "chat" type messages, officially called messagetype "message"
// to avoid confusing with the APRS Messages themself, use the term "aprsmsgmsg"

aprsmsgmsg::aprsmsgmsg (String msgin) {
    int tmp; // tempory variable
    int l; // length
    String callsign_in; // tempory var
    String msgrest; // tempory var;
    String msgtmp; // tempory var;

    // start conditions
    valid=false;
    hasack=false;
    isack=false;
    isrej=false;
    msgno="";
    body="";
    callsign=make_shared<pathnode>();



    // expected format:
    // :callsign-15:message{01
    // the callsign path should be exactly 9 characters
    if (msgin.length() < 11) {
        logPrintlnD("APRSMSGMSG Error: EMPYT");
        return;
    }


    if (msgin.charAt(0) != ':') {
        logPrintlnD("APRSMSGMSG Error: First : missing");
        return;     // first and 11th char should be ":"
    }
    if (msgin.charAt(10) != ':') {
        logPrintlnD("APRSMSGMSG Error: Second : missing");
        return;
    }

    // extract callsign
    callsign_in=msgin.substring(1,10); // callsign is everything between 2dn and 10th char

    tmp=callsign_in.indexOf(" "); // find any trailing spaces

    // callsign begins with a space, should not happen
    if (tmp == 0) {
        logPrintlnD("APRSMSGMSG Error: callsign starts with a space");
        return;
    };

    if (tmp != -1) {
        callsign_in=callsign_in.substring(0,tmp); // use anything before the 1st space
    };

    msgrest=msgin.substring(11); // everything from 12th char


    l=msgrest.length();

    // convert callsign into pathnode object -> easier to check if the callsign is valid
    // the callsign object has already been created above, so just create it
    callsign->configure(callsign_in, normalnode);
    // std::auto_ptr<pathnode> callsign(new pathnode) // Is this correct? 


    // we should have a valid callsign
    // break out if callsign is not valid
    // break out if callsign is "WIDE"
    // break out if the callsign contains a "*"
    if ((!callsign->valid) || (callsign->wide) || (callsign->digipeat)) return;

    // look for "{" 
    tmp=msgrest.indexOf("{");

    if (tmp==-1) {
        // no "{" -> Check if this is an "ack" message


        // check if it is an "ack" message
        if (msgrest.startsWith("ack")) {
            // there should be something after the "ack"
            if (l > 3) {
                isack=true;
                msgno=msgrest.substring(3); // msgno is everything after the "ack" parth
            };
        } else if (msgrest.startsWith("rej")) {
            // check if it is an "rej" message
        
            // there should be something after the "rej"
            if (l > 3) {
                isrej=true;
                msgno=msgrest.substring(3); // msgno is everything after the "rej" parth
            };
        } else {
                body=msgrest; // not an "ack" message

        }


        // maximum length body is 67 characters and can be empty
        // maximum length msgno is 5 characters but cannot be empty
        if (body.length() > 67) {
            logPrintlnD("APRSMSGMSG Error: body to long");
        }
        if (msgno.length() > 5) {
            logPrintlnD("APRSMSGMSG Error: msgno to long");
        }
        if ((body.length() <= 67) && (msgno.length() <= 5)) {
            // OK, we have a valid message!
            valid=true;
        }


        return; // done
    };

    // "{" found
    // extra check: there should be something after the "{"
    if (tmp == l-1) {
        logPrintlnD("APRSMSGMSG Error: Msgno missing");
        return;
    }                       // break out "{" found at the last character of the message
                            // (note: l = length -> starts at 1, "tmp" is indexof -> starts at 0

    // extra check: look for 2nd "{" (should not exist)
    if (msgrest.substring(tmp+1).indexOf("{") != -1) {
        logPrintlnD("APRSMSGMSG Error: multiple { found");
        return; // break out (note: message is still marked as not valid)
    }; 


    // split the "message into the body and the msgno
    body=msgrest.substring(0,tmp); // body is part in front of the "{"
    msgno=msgrest.substring(tmp+1); // msgno of part after the "{"
    hasack=true;

    // maximum length body is 67 characters and can be empty
    // maximum length msgno is 5 characters but cannot be empty

    if (body.length() > 67) {
        logPrintlnD("APRSMSGMSG Error: boddy to long");
    }
    if (msgno.length() > 5) {
        logPrintlnD("APRSMSGMSG Error: msgno to long");
    }


    if ((body.length() <= 67) && (msgno.length() <= 5)) {
        // OK, we have a valid message!
        valid=true;
    }


    // done
    return;

};




aprsmsgmsg::aprsmsgmsg () {
    // start conditions
    valid=false; // note, 'valid' is initialised as 'false', so it is up the higher-level application to set this to 'true' when all data is filled in
    hasack=false;
    isack=false;
    isrej=false;
    msgno="";
    body="";
    callsign=nullptr;
};





String aprsmsgmsg::fulltxt () {
    String out;

    // check if valid
    if (!valid) return "";


    out=callsign->callsign;

    if (callsign->ssid != 0) {
        out += ("-" + String(callsign->ssid));
    }

    // add spaces to make callsign exactly 9 chars
    out=(out+"         ").substring(0,9); 

    // an "ack" or "rej" message
    if (isack) return (":"+out+":ack"+msgno);
    if (isrej) return (":"+out+":rej"+msgno);



    // message is :callsign:body
    out=":"+out+":" + body;


    // is there a ack-marker?
    if (hasack) {
        out += ("{" + msgno);
    };
    
    return out;
}
