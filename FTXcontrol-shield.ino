#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MemoryFree.h>		// this was for debugging and can be removed (but here are some dependencies futher down)

LiquidCrystal_I2C lcd (0x27, 16, 2);

#define DHTPIN1 4
#define DHTPIN2 5
#define DHTPIN3 6
#define DHTPIN4 7
#define button 2
#define relais 8
#define photo A0
#define photo5V A1


DHT dht[] = {
  {DHTPIN1, DHT22}
  ,
  {DHTPIN2, DHT22}
  ,
  {DHTPIN3, DHT22}
  ,
  {DHTPIN4, DHT22}
  ,
};


const int numsens = 3;		//number of sensors used
float humidity[numsens];
float temperature[numsens];


const int numReadings = 10;	// number of readings of ventstate LED
const unsigned long checkInterval = 3600000;	// how long to wait before checking again after setting ventstate blindly 
const unsigned long readInterval = 1000;	// sensors will be read every x milliseconds
const unsigned long logInterval = 60000;	// send data every x milliseconds
unsigned long lasthistoryTime = 0;	// timer variable
const byte history_ints_per_hour = 6;	// how many intervals measurement intervals (e.g. 6 gives 10 minute history intervals)
const byte history = history_ints_per_hour + 1;	// how many previous values do we save?
byte historyIndex = 0;		// index circulating through the history array 
float thistory[numsens][history];	// keeping this separate from the temperature array with current values
float new_changerate[numsens];	// not used (but could be used to calculate temperature accelleration)
float old_changerate[numsens];
const unsigned long difftriggerInterval = 300000;	// how long t0 has to be lower than t2 to start ventilation
const float requireddiff = 1.0;	// how much colder does it have to be outside for ventilation to kick in?
const float requireddiffhigh = 4.0;	// how much colder for high ventilation to kick in?
const float maxinsidetemp = 25.0;	// temp needed for automatic adjustment to kick in (24 might be bit low, we'll see)
//const int maxhighTime = 10;  not used           // maximum hours on high ventilation per 24h
unsigned long currentTime;
unsigned long previouslogTime = 0;	// timer variable
unsigned long colderoutsideTime = 0;	// timer variable 
unsigned long muchcolderoutsideTime = 0;	// timer variable 
unsigned long warmeroutsideTime = 0;	// timer variable 
unsigned long lastreadTime = 0;	// timer variable 
unsigned long lastfailTime = 0;	// timer variable 
byte ventstate;			// ventilation state arduino believes the ventilation system to be in 
byte desiredventstate;		// ventilation state arduino currently wants
byte lastledventstate;		// ventilation state indicated by the ventilation systems LEDs
volatile byte buttonpress;
byte checkventstate;
int errorsensor[numsens];	// counting sensor read errors (NaN values) 
int used_old_t_value[numsens];	// counting how often sensor yielded NaN values more than 10 times in a row (resulting in previous value being used instead)
int used_old_h_value[numsens];
bool historyarrayfull = false;	// this flag indicates when old changerate can be calculated
bool historyarrayhalffull = false;	// this flag indicates when new (current) changerate can be calculated
bool autohigh = true;		// this flag indicates whether high ventilation state was set automatically (rather than manually)
						  // assuming autohigh at startup to avoid getting stuck in ventstate 2
bool checkneeded = false;	// this flag indicates whether ventstate needs to be synced after failed reading of ledventstate
int error[6];
int lederror = 0;		// counting how often reading the LED (ledventstate) failed
int ledbrightness;		// this is for long term debugging to make sure the ledbrighness reported is the same as the one used to calculate ledventstate
const int mode = 2;		// 0 = manual, 1 = semi-automatic, 2 = automatic, 3 = enforced automatic
						  // semi-automatic only becomes active when threshhold is crossed (no syncing of ventstates)
						  // automatic checks actual state regularly and corrects manual changes (except high ventilation)
						  // enforced automatic will even "correct" when high ventilation is turned on manually
byte printmode = 1;		// 1 = output to influxdb, 2 = output to csv file


// uncomment next line to debug. Note that logging to influxdb will not work while in debugging mode.
//#define DEBUG 
//#define VERBOSE

#ifdef DEBUG
#define DEBUG_PRINT(X) Serial.println(X)
#define DEBUG_SLOWER delay(500)
#else
#define DEBUG_PRINT(X)
#define DEBUG_SLOWER
#endif


#ifdef VERBOSE
#define DEBUG_v_PRINT(X) Serial.println(X)
#else
#define DEBUG_v_PRINT(X)
#endif

void
setup ()
{
  DEBUG_PRINT ((F ("Status: Starting setup...")));
  lcd.init ();
  lcd.backlight ();		//Hintergrundbeleuchtung einschalten (lcd.noBacklight(); schaltet die Beleuchtung aus). 
  Serial.begin (9600);
for (auto & sensor:dht)
    {
      sensor.begin ();
    }
  pinMode (relais, OUTPUT);
  pinMode (photo5V, OUTPUT);
  digitalWrite (relais, LOW);	// just to be sure
  pinMode (button, INPUT);

  pinMode (LED_BUILTIN, OUTPUT);
  buttonpress = 0;
  attachInterrupt (0, pin_ISR, RISING);

  DEBUG_PRINT ((F ("Status: Initializing ventstate...")));
  delay (100);
  ventstate = ledventstate (brightness ());	// check the current state
  if (ventstate < 3)
    {
      desiredventstate = ventstate;
    }				// make sure ventstate wont be changed at every reset, but only if led reading succeeded
  if (ventstate = 2)
    {
      autohigh = true;
    }				// to avoid getting stuck in high ventilation, we assume it was set automatically (giving permission to change it)
  for (int i = 0; i < 20; i++)
    {				// initializing sensor readings for exponential smoothing
      read_sensors ();		// initialization should rather be with raw readings. but does it really matter?
      delay (10);
    }
  send_data ();			// send first reading so that we don't have to wait a minute (note, however, that it is inaccurate)
  DEBUG_PRINT ((F ("Status: Setup done.")));
}



void
loop ()
{
  DEBUG_SLOWER;
  currentTime = millis ();


  if (currentTime - lastreadTime >= readInterval)
    {				// the sensors are not so fast, so we'll take it easy...
      DEBUG_v_PRINT ((F ("Status: reading sensors...")));
      read_sensors ();
    }

  update_display ();


  if (currentTime - lasthistoryTime > 3600000 / history_ints_per_hour)
    {
      DEBUG_PRINT ((F ("Status: Time to save to history array...")));
      save_to_history ();
      DEBUG_PRINT ((F
		    ("Status: Calculating change rates (if any) at index...")));
      DEBUG_PRINT ((historyIndex));
      DEBUG_PRINT ((F ("for sensor 0")));
      calculate_change_rates (0);	// calculate changerates for outside sensor
      DEBUG_PRINT ((F ("for sensor 2")));
      calculate_change_rates (2);	// calculate changerates for inside sensor
      DEBUG_PRINT ((F ("Status: Increasing historyIndex from ")));
      DEBUG_PRINT ((historyIndex));
      historyIndex = (historyIndex + 1) % history;	// increase index and wrap around 
      DEBUG_PRINT ((F ("to")));
      DEBUG_PRINT ((historyIndex));
      lasthistoryTime = currentTime;
    }


  // Basic idea: its either warmer or colder ouside 
  // and we measure for how long we have been on either side of the threshold (colder or warmer)
  // by resetting the timer for "the other side".
  // If we've been on one side for long enough, we take it seriously
  // This could be rewritten much nicer by using state machines. Maybe later.
  if (temperature[2] - temperature[0] >= requireddiff)
    {				//if outside temp is at least x lower than inside
      warmeroutsideTime = currentTime;	//reset the timer for it being  warmer outside
      DEBUG_v_PRINT ((F ("Status: It's colder outside than inside")));
    }
  else if (temperature[2] - temperature[0] < requireddiff)
    {				//if outside temp is not at least x lower than inside
      colderoutsideTime = currentTime;	//reset the timer for it being  colder outside
      colderoutsideTime = currentTime;
      DEBUG_v_PRINT ((F ("Status: It's warmer outside than inside")));
    }



  if (currentTime - colderoutsideTime > difftriggerInterval)
    {				//has it been cold outside for long enough?
      whichventstate ();
    }
  else if (currentTime - warmeroutsideTime > difftriggerInterval
	   && temperature[2] > maxinsidetemp && desiredventstate != 0)
    {				//is it getting too warm outside? (but it's only a problem when inside is already quite warm. Otherwise, let the spring and autumn sun warm up the house)
      desiredventstate = 0;
      DEBUG_PRINT ((F ("Status: Changed desired ventstate to 0")));
    }



  if (desiredventstate != ventstate && mode > 0)
    {				// unless we're in manual mode ...
      DEBUG_PRINT ((F ("Status: Changing ventstate...")));
      setventstate (desiredventstate);	// ... we adjust the ventilation 
    }



  if (currentTime - previouslogTime >= logInterval)
    {
      DEBUG_PRINT ((F ("Status: Sending data...")));
      send_data ();
      DEBUG_PRINT ((F ("Status: Data sent.")));
      previouslogTime = currentTime;	// reset the logg timer

      // this is here just because it's enough to check once per logintervall. It's not related to logging
      if (mode > 1)
	{			// when in automatic mode ...
	  DEBUG_PRINT ((F
			("Status: Syncing ventstate with ledventstate...")));
	  syncventstate ();	// ... make sure ventstate and actual ventstate are in sync
	}
    }



  if (currentTime - lastfailTime >= checkInterval && checkneeded)
    {
      DEBUG_PRINT ((F
		    ("Status: Syncing ventstate after blindsetventstate...")));
      checkneeded = false;
      syncventstate ();		// check again after error
    }
}

void
pin_ISR ()
{				// note that the interrupt pin has to be pin 2 or 3
  buttonpress = 1;
  DEBUG_PRINT ((F ("buttonpress registered...")));
  DEBUG_PRINT ((F ("Status: Pressing button on ventilation...")));
  digitalWrite (relais, HIGH);
  delay (300);
  digitalWrite (relais, LOW);
}

void
read_sensors ()
{
  for (int i = 0; i < numsens; i++)
    {
      DEBUG_v_PRINT ((F ("Status: Reading temperature sensor ")));
      DEBUG_v_PRINT ((i));
      float old_value = temperature[i];
      float new_value = dht[i].readTemperature ();
      byte count = 0;
      while (isnan (new_value) && count < 10)
	{			// retrying up to 10 times if reading fails
	  errorsensor[i]++;
	  DEBUG_PRINT ((F ("Error: NaN value for temp")));
	  DEBUG_PRINT ((i));
	  DEBUG_PRINT ((F ("Trying again...")));
	  delay (500);		// waiting 100 ms was too short here. would still produce NaNs
	  new_value = dht[i].readTemperature ();	// the isnan function is in the DHT library
	  DEBUG_PRINT ((new_value));
	  count++;
	}
      if (isnan (new_value))
	{
	  temperature[i] = old_value;	// keeping the old value for another cycle seems the best way to handle this
	  used_old_t_value[i]++;	// keep a record of the failure
	  DEBUG_PRINT ((F ("Status: Used old value for temp")));
	  DEBUG_PRINT ((i));
	}
      else if (!isnan (new_value))
	{
	  temperature[i] = 0.1 * new_value + 0.9 * old_value;	// apply exponential smoothing
	  DEBUG_v_PRINT ((F
			  ("Status: Calculated temp based on old value and new value")));
	}
      else
	{
	  DEBUG_PRINT ((F ("Status: This should not happen (temp).")));
	  DEBUG_PRINT ((i));
	  DEBUG_PRINT ((old_value));
	  DEBUG_PRINT ((new_value));
	}

      DEBUG_v_PRINT ((F ("Status: Reading humidity sensor ")));
      DEBUG_v_PRINT ((i));
      old_value = humidity[i];
      new_value = dht[i].readHumidity ();
      count = 0;
      while (isnan (new_value) && count < 10)
	{
	  errorsensor[i]++;
	  DEBUG_PRINT ((F ("Error: NaN value for hum")));
	  DEBUG_PRINT ((i));
	  DEBUG_PRINT ((F ("Trying again...")));
	  delay (500);
	  new_value = dht[i].readHumidity ();
	  DEBUG_PRINT ((new_value));
	  count++;
	}
      if (isnan (new_value))
	{
	  humidity[i] = old_value;	// keeping the old value for another cycle seems the best way to handle this
	  used_old_h_value[i]++;	// keep a record of the failure
	  DEBUG_v_PRINT ((F ("Status: Used old value for humidity")));
	  DEBUG_v_PRINT ((i));
	}
      else if (!isnan (new_value))
	{
	  humidity[i] = 0.1 * new_value + 0.9 * old_value;
	  DEBUG_v_PRINT ((F
			  ("Status: Calculated humindity based on old value and new value")));
	}
      else
	{
	  DEBUG_PRINT ((F ("Status: This should not happen. (hum)")));
	  DEBUG_PRINT ((i));
	  DEBUG_PRINT ((old_value));
	  DEBUG_PRINT ((new_value));
	  DEBUG_PRINT ((humidity[i]));
	}
    }
  DEBUG_v_PRINT ((F ("Status: Done reading sensors.")));
}



void
save_to_history ()
{				// this saves the current value for outside and inside temperature (for the time being)
  DEBUG_PRINT ((F ("Status: Adding value to history array at index... ")));
  DEBUG_PRINT ((historyIndex));
  byte halfhistory = history / 2;
  DEBUG_PRINT ((F ("temp 0 is")));
  DEBUG_PRINT ((temperature[0]));
  byte s = 0;			//  outside sensor
  thistory[s][historyIndex] = temperature[s];
  DEBUG_PRINT ((F ("Status: These are the temp0 values we currently have:")));
  for (int i = 0; i < history; i++)
    {
      DEBUG_PRINT ((thistory[s][i]));
    }

  DEBUG_PRINT ((F ("temp 2 is")));
  DEBUG_PRINT ((temperature[2]));
  s = 2;			//  inside sensor
  thistory[s][historyIndex] = temperature[s];
  DEBUG_PRINT ((F ("Status: These are the temp2 values we currently have:")));
  for (int i = 0; i < history; i++)
    {
      DEBUG_PRINT ((thistory[s][i]));
    }

  if (historyIndex == history - 1 && !historyarrayfull)
    {
      historyarrayfull = true;
      DEBUG_PRINT ((F ("Status: History array is now filled.")));
    }
  else if (historyIndex > halfhistory && !historyarrayhalffull)
    {
      historyarrayhalffull = true;
      DEBUG_PRINT ((F ("Status: History array is now half filled.")));
    }
  DEBUG_v_PRINT ((F ("Status: Done writing values to history array.")));
}

void
calculate_change_rates (byte s)
{
  byte steps = (history - 1) / 2;	// need an extra variable for this so that I have the integer value even in the equations with floats below (its only relevant if history is set to even value, which it shouldnt be)
  byte v1 = (historyIndex + 1) % history;	// this is the index of th oldest value we have (using modulo to wrap around
  byte v2 = (historyIndex + 1 + steps) % history;
  byte v3 = (historyIndex + history - steps) % history;	// v2 and v3 are identical if history is set to an uneven value (which is preferred)
  byte v4 = (historyIndex);

  DEBUG_PRINT ((F ("Status: v1-v4 are")));
  DEBUG_PRINT ((v1));
  DEBUG_PRINT ((v2));
  DEBUG_PRINT ((v3));
  DEBUG_PRINT ((v4));
  DEBUG_PRINT ((F ("Status: contents of v1-v4 are")));
  DEBUG_PRINT ((thistory[s][v1]));
  DEBUG_PRINT ((thistory[s][v2]));
  DEBUG_PRINT ((thistory[s][v3]));
  DEBUG_PRINT ((thistory[s][v4]));

  if (historyarrayfull)
    {				// prevent nonsense changerates while there is insufficient data
      old_changerate[s] = (thistory[s][v2] - thistory[s][v1]) / steps * history_ints_per_hour;	// the unit here is degrees per hour
      DEBUG_PRINT ((F ("Status: old changerate is...")));
      DEBUG_PRINT ((old_changerate[s]));
    }
  if (historyarrayhalffull)
    {				// prevent nonsense changerates while there is insufficient data
      new_changerate[s] = (thistory[s][v4] - thistory[s][v3]) / steps * history_ints_per_hour;	// the unit here is degrees per hour
      DEBUG_PRINT ((F ("Status: current changerate is...")));
      DEBUG_PRINT ((new_changerate[s]));
    }
}

void
whichventstate ()
{
  DEBUG_v_PRINT ((F ("Status: Checking if we want ventstate 1 or 2")));
  if (temperature[2] - temperature[0] >= requireddiffhigh &&	// we only want ventstate 2 if (i) temp difference is high enough 
      temperature[2] > maxinsidetemp &&	// its too warm inside
      currentTime - muchcolderoutsideTime > difftriggerInterval)
    {				// this has been so for long enough (to avoid jumping back and forth between states)     
      desiredventstate = 2;
      autohigh = true;
      DEBUG_v_PRINT ((F ("Status: Set desired ventstate to 2")));
    }
  else if (temperature[2] - temperature[0] >= requireddiffhigh
	   && temperature[2] > maxinsidetemp)
    {
      desiredventstate = 1;
      autohigh = true;
      DEBUG_v_PRINT ((F
		      ("Status: Conditions for ventstate 2 are met but not yet for long enough so ventstate is still 1")));
      return;			// wait another wile to see if conditions are stable
    }
  else
    {
      muchcolderoutsideTime = currentTime;	// reset timer vor high ventstate
      if (desiredventstate != 1)
	{
	  desiredventstate = 1;
	  DEBUG_PRINT ((F ("Status: Changed desired ventstate to 1")));
	}
    }
}

float
dew (float t, float rh)
// dewpoint formula by Peter Mander (https://carnotcycle.wordpress.com/2017/08/01/compute-dewpoint-temperature-from-rh-t/)
// According to Peter, this formula is accurate to within 0.1% over the temperature range b
