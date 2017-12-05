#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define OutChip1 0
#define OutChip2 16
#define OutChip3 15
#define OutCycleRegisters 4
#define OutFlushOutputs 5

#define InNextSpot 12
#define InAcceptMove 14

#define PLAYERONE 1
#define PLAYERTWO -1

int Board[9]; //Holds the state of the board
int Score[3]; //Holds the three scores
int SelectedLED; //holds the number of the selected LED (always one that belongs to neither player)
int Player; //Holds the number of the current player
int Inputs[2]; //holds the value of the inputs

void FlushOutputs()
{ //flush the values from the shift registers to the outputs of their chips
  digitalWrite(OutFlushOutputs, HIGH);
  delay(10);
  digitalWrite(OutFlushOutputs, LOW);
}

void CycleRegisters()
{ //Read the next value into the shift registers (pushing everything one slot down)
  digitalWrite(OutCycleRegisters, HIGH);
  delay(10);
  digitalWrite(OutCycleRegisters, LOW);
}

void SetPin(int Pin, bool Value)
{ //Set a given nodeMCU pin to a given value
  /*Serial.print("Pin: ");
  Serial.println(Pin);
  Serial.print("Value");
  Serial.println(Value);*/
  if (Value)
    digitalWrite(Pin, HIGH);
  else
    digitalWrite(Pin, LOW);
}

void SetThreePins(int Pin1Value, int Pin2Value, int Pin3Value, int Player)
{ //Set the next input value on all three shift registers at once
  //and cycle the registers
  SetPin(OutChip1, (Pin1Value==Player));
  SetPin(OutChip2, (Pin2Value==Player));
  SetPin(OutChip3, (Pin3Value==Player));
  delay(10);
  CycleRegisters();
}

void SetTwoLines(int Pin1Value, int Pin2Value, int Pin3Value)
{ //handle one row of output for each player
  SetThreePins(Pin1Value, Pin2Value, Pin3Value, PLAYERONE);
  SetThreePins(Pin1Value, Pin2Value, Pin3Value, PLAYERTWO);
}

void SendOutputs()
{ //Output the entire board and all three score LEDs
  SetTwoLines(Score[0], Score[1], Score[2]);
  SetTwoLines(Board[0], Board[3], Board[6]);
  SetTwoLines(Board[1], Board[4], Board[7]);
  SetTwoLines(Board[2], Board[5], Board[8]);
  delay(10);
  FlushOutputs();
}

void SetBoard(int Value)
{ //Set the entire board to a single value (used for startup and win animations)
  //Note: Deletes current game-in-progress and resets to player one first
  int Count;
  for(Count=0;Count<9;Count++)
     Board[Count]=Value;
  SelectedLED=0;
  Player=PLAYERONE;
}

void ClearBoard()
{ //Clear board and make ready for next game
  SetBoard(0);
}

void setup() {
  // initialise variables and pins
  int Count;
  Serial.begin(74880);
  Serial.println("test");
  for (Count=0;Count<9;Count++)
    Board[Count]=0;
  for (Count=0;Count<3;Count++)
    Score[Count]=0;
  Inputs[0]=0;
  Inputs[1]=0;
  SelectedLED=0;
  Player=PLAYERONE;


  pinMode(OutChip1, OUTPUT);
  pinMode(OutChip2, OUTPUT);
  pinMode(OutChip3, OUTPUT);
  pinMode(OutCycleRegisters, OUTPUT);
  pinMode(OutFlushOutputs, OUTPUT);
  pinMode(InNextSpot, INPUT);
  pinMode(InAcceptMove, INPUT);

//Startup animation to check LEDs
  SetBoard(PLAYERONE);
  SendOutputs();
  delay(250);
  ClearBoard();
  SendOutputs();
  delay(250);
  SetBoard(PLAYERTWO);
  SendOutputs();
  delay(250);
  ClearBoard();
  SendOutputs();
  
}

int CheckLine(int One, int Two, int Three)
{ //See whether or not the given set of three pins constitute a winning row
  if ((One==Two) && (Two==Three))
    return One;
  else
    return 0;
}

void CheckVictory()
{ //Check whether or not we have a winner; update score and play win animation if needed
  int WhichScore, Count, Winner;
  for (Count=2;Count>=0;Count--)
  {
    if (Score[Count]==0)
       WhichScore=Count;
  }

  Winner=0;

  for (Count=0;(Count<3) && (Winner==0);Count++)
  { //Check rows and columns
    Winner = CheckLine(Board[Count], Board[Count+3], Board[Count+6]);
    if (Winner == 0)
      Winner = CheckLine(Board[Count*3], Board[(Count*3)+1], Board[(Count*3)+2]);
  }

  //Now check the diagonals
  if (Winner==0)
    Winner = CheckLine(Board[0], Board[4], Board[8]);
  if (Winner==0)
    Winner = CheckLine(Board[2], Board[4], Board[6]);

  if (Winner!=0)
  {
    Score[WhichScore]=Winner;
    ClearBoard();
    if (WhichScore==2) //All three games won
    { //Identify final winner
      SendOutputs();
      if ((Score[0]+Score[1]+Score[2])>0)
         Winner=PLAYERONE;
      else
         Winner=PLAYERTWO;
      delay(250);
      //Play victory animation
      SetBoard(Winner);
      SendOutputs();
      delay(500);
      ClearBoard();
      SendOutputs();
      delay(500);
      SetBoard(Winner);
      SendOutputs();
      delay(500);
      ClearBoard();
      //Reset scores
      Score[0]=0;
      Score[1]=0;
      Score[2]=0;
      SendOutputs();
    }
  }
}

void ShiftSelected()
{ //Select next available LED
  int LastSelected;
  LastSelected=SelectedLED;
  do
  {
    SelectedLED++;
    if (SelectedLED==9)
      SelectedLED=0;
  } while ((Board[SelectedLED]!=0) && (SelectedLED!=LastSelected));
}

void NextSpot()
{ //Player wishes to move to next available LED
  Board[SelectedLED]=0;
  ShiftSelected();
}

void AcceptMove()
{ //Player wishes to play on currently selected LED
  Board[SelectedLED]=Player;
  if (Player==PLAYERTWO)
    Player=PLAYERONE;
  else
    Player=PLAYERTWO;
  ShiftSelected();
  CheckVictory();
  if (Board[SelectedLED]!=0) //No empty spaces left, yet no victory, therefore draw
    ClearBoard();
}

void ProcessInputs()
{ //Check and respond to inputs
  int NewInputs[2];
  NewInputs[0] = digitalRead(InNextSpot);
  NewInputs[1] = digitalRead(InAcceptMove);

  if ((NewInputs[0]!=Inputs[0]) && (NewInputs[0]==1))
     NextSpot();
  if ((NewInputs[1]!=Inputs[1]) && (NewInputs[1]==1))
     AcceptMove();

  Inputs[0]=NewInputs[0];
  Inputs[1]=NewInputs[1];
}

void SerialBoard()
{ //Output board on serial interface
  int Count;
  for (Count=0;Count<9;Count++)
  {
    Serial.print(Board[Count]);
    Serial.print("x  ");
    if (Count%3==2)
      Serial.println();
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  Serial.println("loop");
  
  if (Board[SelectedLED]!=0)
     Board[SelectedLED]=0;
  else
     Board[SelectedLED]=Player;

  SerialBoard();

  SendOutputs();

  ProcessInputs();

   delay(250);
}
