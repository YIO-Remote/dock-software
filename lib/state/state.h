#ifndef STATE_H
#define STATE_H

class State
{
public:
    enum States {
	  SETUP                 =   0,     // 0 - needs setup
	  CONNECTING            =   1,     // 1 - connecting to wifi, turning on OTA
	  CONN_SUCCESS          =   2,     // 2 - successful connection
	  NORMAL                =   3,     // 3 - normal operation, LED off, turns on when charging
	  NORMAL_CHARGING       =   4,     // 4 - normal operation, LED breathing to indicate charging
	  ERROR                 =   5,     // 5 - error
	  LED_SETUP             =   6,     // 6 - LED brightness setup
	  NORMAL_FULLYCHARGED   =   7,     // 7 - normal operation, remote fully charged
      NORMAL_LOWBATTERY     =   8      // 8 - normal operation, blinks to indicate remote is low battery
	};

    explicit State();
    virtual ~State(){}

    static State*           getInstance() { return s_instance; }

	// current state
    States                  currentState = SETUP;

	// reboots the ESP
	void 					reboot();
	void					printDockInfo(); 

private:
    static State*           s_instance;
};

#endif