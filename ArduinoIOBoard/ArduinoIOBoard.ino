/* 
 * Arduino Serial IOBoard (Version v0.23)
 * by Julian ( http://www.waprogramms.com/ )
 * 
 * Inspiriert von der Arduino I2C Wire Slave version 0.21 by Racer993 <http://raspberrypi4dummies.wordpress.com/>
 * 
 * Durch dieses Programm wird der ATmega328p mit Arduino Bootloader zu einem IO-Board, ansteuerbar durch z.B. einen Computer.
 * Die Dokumentation der Instruktionen befindet sich in der Datei "Instruktionen.txt"
 * 
 * Lizenz: Public Domain
 */

// ##### Konfigurationsbereich: #####
const int tastenEntprellungTimeout = 50; // Millisekunden zur Tastenentprellung
// ##################################

/////////// Finger weg von sämtlichen Code unter dieser Zeile, außer man weiß genau, was man tut. ///////////
char pwmPinsStatus[15]  = "00000000000000";
char flankenConfigP[19] = "000000000000000000";
char flankenConfigN[19] = "000000000000000000";
char flankenWertCn[19]  = "XXXXXXXXXXXXXXXXXX";
int flankenAnalogGrenzwert[6] = { 511, 511, 511, 511, 511, 511 };

String commandInput = "";
bool readInprogress = false;
bool stringReady = false;

void serialWrite(String s)
{
	Serial.println("     :" + s + ";     ");
}
void setup()
{
	Serial.begin(19200);
	serialWrite("SLAVEALIVE");
}
void loop()
{
	if(!readInprogress)
	{
		while(Serial.available() > 0)
		{
			readInprogress = true;
			char in = (char)Serial.read();
			if(in == ';')
				stringReady = true;
			else if(in != " " && in != ":")
				commandInput += in;
		}
		readInprogress = false;
	}
	if(stringReady)
	{
		process(commandInput);
		commandInput = "";
		stringReady = false;
	}
	for(int digitalPin = 0; digitalPin <= 13; digitalPin++)
	{
		char pnP = flankenConfigP[digitalPin];
		char pnN = flankenConfigN[digitalPin];
		char cn = flankenWertCn[digitalPin];
		if(pnP == '1' || pnN == '1')
		{ // Flanke gewünscht
			flankenWertCn[digitalPin] = (digitalRead(digitalPin) == HIGH ? '1' : '0');
			if(pnP == '1' && cn == '0' && flankenWertCn[digitalPin] == '1') { // positive
				serialWrite("FLANKE_P" + String(digitalPin) + ";"); // getriggert
				delay(tastenEntprellungTimeout);
			}
			if(pnN == '1' && cn == '1' && flankenWertCn[digitalPin] == '0') { // negative
				serialWrite("FLANKE_N" + String(digitalPin) + ";"); // getriggert
				delay(tastenEntprellungTimeout);
			}
		}
	}
	for(int analogPin = 0; analogPin <= 5; analogPin++)
	{
		int digitalPin = analogPin + 14;
		char pnP = flankenConfigP[digitalPin];
		char pnN = flankenConfigN[digitalPin];
		char cn = flankenWertCn[digitalPin];
		if(pnP == '1' || pnN == '1')
		{ // Flanke gewünscht
			flankenWertCn[digitalPin] = (analogRead(digitalPin) >= flankenAnalogGrenzwert[analogPin] ? '1' : '0');
			if(pnP == '1' && cn == '0' && flankenWertCn[digitalPin] == '1') { // positive
				serialWrite("FLANKE_PA" + String(analogPin)); // getriggert
				delay(tastenEntprellungTimeout);
			}
			if(pnN == '1' && cn == '1' && flankenWertCn[digitalPin] == '0') { // negative
				serialWrite("FLANKE_NA" + String(analogPin)); // getriggert
				delay(tastenEntprellungTimeout);
			}
		}
	}
	//delay(1); // damit ma uns ned verhaspeln
}

void process(String command)
{
	if(command == "READ")
		serialWrite(getPinStatus());
	else {
		bool success = obey(command);
		serialWrite((success ? "SUCCESS" : "FAILED") + "|" + command);
	}
}

String getPinStatus()
{
	//Pattern: D00 D01 D02 D03 D04 D05 D06 D07 D08 D09 D10 D11 D12 D13 | A0 | A1 | A2 | A3 | A4 | A5
	String pinStatus = "";
	for(int digitalPin = 0; digitalPin <= 13; digitalPin++)
	{
		if (pwmPinsStatus[digitalPin] == 0)
			pinStatus += String (digitalRead(digitalPin));
		else
			pinStatus += "P";
	}
	for(int analogPin = 0; analogPin <= 5; analogPin++)
	{
		pinStatus += "|" + String (analogRead(analogPin));
	}
	return pinStatus;
}

bool obey(String command)
{
	int pinVal;
	int analogVal = (String(command[4]) + String(command[5]) + String(command[6])).toInt();
	// Parse characters 2 and 3
	if (String(command[1]) == "A")
	{ // Analog pin number on character 2
		switch(String(command[2]).toInt())
		{
			case 0: pinVal = A0; break;
			case 1: pinVal = A1; break;
			case 2: pinVal = A2; break;
			case 3: pinVal = A3; break;
			case 4: pinVal = A4; break;
			case 5: pinVal = A5; break;
			default: return false;
		}
	} else { // Digital pin numbers on characters 1 and 2
		pinVal = (String(command[1]) + String(command[2])).toInt();
		pwmPinsStatus[pinVal] = 0;
	}
	// Parse character 1
	if (String(command[0]) == "S") { // Set pinMode
		switch(command[3]) {
			case 'I': pinMode(pinVal, INPUT); break;
			case 'O': pinMode(pinVal, OUTPUT); flankenConfigP[pinVal] = '0'; flankenConfigN[pinVal] = '0'; break;
			case 'P': pinMode(pinVal, INPUT_PULLUP); break;
			default: return false;
		}
	} else if (String(command[0]) == "W") { // digitalWrite
		digitalWrite(pinVal, (String(command[3]) == "H" ? HIGH : LOW));
	} else if (String(command[0]) == "A") { // analogWrite (PWM write)
		if (!(pinVal == 3 || pinVal == 5 || pinVal == 6 || pinVal == 9 || pinVal == 10 || pinVal == 11))
			return false;
		pwmPinsStatus[pinVal] = 1;
		analogWrite(pinVal, analogVal);
	} else if (String(command[0]) == "P" || String(command[0]) == "N") {
		char pn = command[0]; // Positive/Negative Flanke
		char sr = command[3]; // Set/Reset
		flankenWertCn[pinVal] = 'X';
		if(pn == 'P') flankenConfigP[pinVal] = (sr == 'S' ? '1' : '0');
		if(pn == 'N') flankenConfigN[pinVal] = (sr == 'S' ? '1' : '0');
		if(pinVal > 13)
			flankenAnalogGrenzwert[pinVal - 14] = analogVal;
	} else
		return false;
	return true;
}
