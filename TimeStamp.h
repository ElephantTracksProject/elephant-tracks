#ifndef TIMESTAMP_H_
#define TIMESTAMP_H_

struct TimeStamp {
public:
    unsigned long time;
    unsigned long thread;
    
    // Default constructor
    TimeStamp()
        : time(0)
        , thread(0) {
    }

    // Constructor with supplied parameters
    TimeStamp( unsigned long newtime,
               unsigned long newthread )
        : time(newtime)
        , thread(newthread) {
    }
};


#endif // TIMESTAMP_H_
