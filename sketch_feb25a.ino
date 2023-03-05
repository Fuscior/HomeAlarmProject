//local headunit hardware working
//26/02/2023
//fixed local logs

#include <WiFi.h>   //
#include "time.h"   //NTP 
#include <LiquidCrystal.h>  //LCD remove
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>   //RTC
#include <Keypad.h>   //Keypad
#include "FS.h"       //SD Card
#include "SD.h"
#include "SPI.h"
#include <esp_now.h>  //module coms
#include <WiFiClientSecure.h> //comes with telegram bot
#include <UniversalTelegramBot.h> //telegram
#include <MFRC522.h>  //RFID

//RTC pins
#define I2C_SDA 38  //pins for RTC
#define I2C_SCL 48
//RFID pins
#define SS_PIN_RFID 41
#define RST_PIN_RFID 42
// Telegram BOT Token
#define BOT_TOKEN "5832604102:AAFt5WHcdB-bh3H6Am-0EVcu6Yao5jcO11Q"
#define CHAT_IDTEST "5682801801"  //@Y3Project_bot

//WiFi creds
const char* ssid     = "eir99400545-2.4G";
const char* password = "rbkw5dyt";

//NTP settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;
int NTPcount=0;

//LCD remove old data
//const int rs = 4, en = 5, d4 = 6, d5 = 7, d6 = 15, d7 = 16;
//LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
int Row_LCD= 4;
int Cols_LCD= 20;
LiquidCrystal_I2C lcd(0x20, Cols_LCD, Row_LCD);   //I2C address, size

//Keypad
const int Row_Keypad= 4;  //kepad
const int Col_Keypad= 3;
//String Password= "1234";  //startingpassword
String input_Password;
char keys[Row_Keypad][Col_Keypad] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte pin_rows[Row_Keypad] = {17, 18, 8, 3};     //row pinouts of the keypad
byte pin_column[Col_Keypad] = {46, 9, 14};  //column pinouts of the keypad

Keypad keypad= Keypad(makeKeymap(keys), pin_rows, pin_column, Row_Keypad, Col_Keypad);

//RTC
RTC_DS3231 rtc;

typedef struct struct_message{
  char Device[32];
  char Location[32];
  int ID;
  int EventCount;
  float BatteryPercent;
  bool isAlarm=false;
} struct_message;
struct_message moduleData; //moduleData stores info

bool RTC_reajust_overide=true;

//Time&date Timer
unsigned long previousMillis=0; //used to cycle row1LCD
bool row1LCD=false; //toggle for row1LCD

//password input
int x_locate;
//system state
bool isSet;   
bool is_Alarm;     
int fail_count=0; //runing total of failed password inputs 

char Saved_Password[10];

//logs
unsigned int LogCount=0;  //running total of alarm logs
bool isLogSaved=false;   //has log been writen to SD

//TelegramBot
const unsigned long BOT_MTBS = 10000; // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime; // last time messages' scan has been done

//RFID testing 16/02/23
const int ipaddress[4] = {103, 97, 67, 25};
const int Passs[8]= {99,99,36,33};
bool isRIGHTrfid=false;

byte nuidPICC[4] = {0, 0, 0, 0};
MFRC522::MIFARE_Key key;
MFRC522 rfid = MFRC522(SS_PIN_RFID, RST_PIN_RFID);

//testing area
int armDelay=500;

//Buzzer
const int buzzer=47;

bool alarmAlirtsent=false;

//testing





//--------------------------------------------------
//-----Start-of-Setup-------------------------------
void setup() {
  Serial.begin(115200);
  lcd.begin(Cols_LCD, Row_LCD);   //LCD Size
  Wire.begin(I2C_SDA, I2C_SCL);  //setting SDA & SCL pins RTC
  input_Password.reserve(4);     // maximum input characters is 5(0 to 4) //update to remove hard code

  initilize_LCD();
/*
  lcd.init();
  lcd.backlight();
  lcd.noBlink();
  lcd.noCursor();
  */

  start_WiFi();
  struct tm init_NTP=F_get_NTP();   //retrive time from NTP server

  initilize_telegram_Bot();

  //end_WiFi(); //commented out due to testing Telegram Bot

  initilize_RTC();   //check RTC&Seed
    //Time RTC on Boot
  struct DateTime RTC_TIME= F_Get_RTC_Time();   //get RTC Time
  if(RTC_reajust_overide){
    Serial.println("RTC check on start up");
    F_Check_RTC_accurcy(init_NTP, RTC_TIME);
    RTC_reajust_overide=false;
  }

  initilize_SD();
  F_Retreve_SD_Password();  //currently saved as goable need to UPDATE just get check then clear security!!!!!

  initilize_RFID();

  //ESP now
  WiFi.mode(WIFI_STA);  //Set device as a Wi-Fi Station
  if (esp_now_init() != ESP_OK) { // Init ESP-NOW
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  //Once ESPNow is successfully Init, we will register for recv CB to
  //get recv packer info
  esp_now_register_recv_cb(OnDataRecv);
  
  delay(2500);
  lcd.clear();
  //0 inital state
  NTPcount=0; // reset count to get NTP
  isSet=false;  //system us unarmed on boot
  is_Alarm=false; //alarm is off on boot

  /*  //reset password 
  char* testx="4567";
  deleteFile(SD, "/PasswordFile.txt");
  writeFile(SD, "/PasswordFile.txt", testx);
  */
  //Serial.println(WiFi.macAddress()); get MAC of device

  //buzzer testing
  pinMode(buzzer, OUTPUT);

}
//------END-Of-Setup--------------------------------
//==================================================
//======Start-of-Main-Loop=========================
void loop() {
  struct tm NTP_Time;
  for(NTPcount; NTPcount<1; NTPcount++){
    //struct tm NTP_Time= F_get_NTP();    //get NTP time
    NTP_Time= F_get_NTP(); 
    //F_Display_NTP_time(NTP_Time);
  }
  //F_Display_NTP_time(NTP_Time);        //display ntp time TESTING purpuses
  struct DateTime RTC_Time= F_Get_RTC_Time();   //get RTC Time
  //F_Display_RTC_Time(RTC_Time);          //display RTC time TESTING
  previousMillis= F_Time_Date_Cycle_Display(RTC_Time, previousMillis);

  F_Check_RTC_accurcy(NTP_Time, RTC_Time);

  F_Password_input();   //keystrocks
  
  F_Display_State();  //display system state to LCD

  //testing below
  //to be removed
  //quick testing of alarm logic NEEDS to be built out

  if(moduleData.isAlarm == true && isSet == true){
    is_Alarm=true;
    lcd.setCursor(5, 1);
    lcd.print("ALARM!!!");
    lcd.setCursor(5, 2);
    lcd.print(moduleData.Location);

    if(!alarmAlirtsent){
      bot.sendMessage(CHAT_IDTEST, "Alarm: ", "");
      bot.sendMessage(CHAT_IDTEST,moduleData.Location, "");

      alarmAlirtsent=true;
    }
  }
  if(moduleData.isAlarm == false && isSet == false){
    is_Alarm=false;
    lcd.setCursor(5, 1);
    lcd.print("        ");
    lcd.setCursor(5, 2);
    lcd.print("          ");
  }



  //display state for testing remove later
  if(isSet){
    pinMode(1,OUTPUT);
    digitalWrite(1,HIGH);
  }
  else{
    pinMode(1,OUTPUT);
    digitalWrite(1,LOW);
  }

  if(!isSet){
    //system unset reset log 
    isLogSaved=false;
  }
  
  //testing of encrpyt
  //F_EncryptPassword();

  //telegram testing working
  //------------------------------
  if (millis() - bot_lasttime > BOT_MTBS){  //if over the time past since last check
    Serial.println("Checking for Telegram message");
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1); 

    while (numNewMessages){
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
  //-----------------------------------------

  //rfid Testing working
  readRFID(); //RFID Logic

  //buzzer testing working emprove on
 /// tone(buzzer, 1000);

  F_Save_Local_Log();
}
//======END-of-Main-Loop============================
//==================================================
//------Initilize-WiFi------------------------------
void start_WiFi(void){
  // Connect to Wi-Fi
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  //lcd.println(ssid);  //display SSID
  WiFi.begin(ssid, password); //start WiFi send SSID & Password
  while (WiFi.status() != WL_CONNECTED) {
    for(int i=0; i<4; i++){ //protray a sense of progress to the user
      delay(500);
      lcd.print(".");
    }
  }
  lcd.setCursor(0, 1);
  lcd.print("WiFi connected.");
}
//-----End-of-initilize-WiFi-----------------------
//-----Stop-WiFi-----------------------------------
void end_WiFi(void){
  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  lcd.setCursor(0, 2);
  lcd.print("WiFi Stopped");
  delay(2000);
}
//-------End-of-Stop-WiFi----------------------------
//-------Get/update-NTP-Time------------------------------
struct tm F_get_NTP(){
  struct tm NTP_time;

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  if(!getLocalTime(&NTP_time)){ //is NTP reached?
    lcd.setCursor(0, 1);
    lcd.print("Failed to obtain time");
    Serial.println("Failed to obtain time");
  //return;
  }
  //F_Display_NTP_time(NTP_time); //debuging display

  return NTP_time;
}
//---------END-Of-Get-NTP-Time-------------------------
//---------Display-NTP-Time-----------------------------
void F_Display_NTP_time(struct tm NTP){ 
  //display NTP time HR:Min:sec
  //Serial
  Serial.println(&NTP, "%A, %B %d %Y %H:%M:%S");
  Serial.print("NTP TIME:");
  Serial.print(NTP.tm_hour);
  Serial.print(":");
  Serial.print(NTP.tm_min);
  Serial.print(":");
  Serial.println(NTP.tm_sec);
  //LCD
  /*
  lcd.setCursor(1, 3);
  lcd.print(NTP.tm_hour);
  lcd.print(":");
  lcd.print(NTP.tm_min);
  lcd.print(":");
  lcd.print(NTP.tm_sec);
  */
}
//--------END-of-Display-NTP-Time---------------------------
//--------Get-Time-from-RTC---------------------------------
struct DateTime F_Get_RTC_Time(){
  DateTime now= rtc.now();

  return now;  
}
//-------END-of-Get-Time-from-RTC----------------------------
//-------Display-RTC-Time------------------------------------
void F_Display_RTC_Time(struct DateTime Now){
  lcd.setCursor(2, 0);

  if(Now.hour()<10){    //padd a 0 to display if needed
    lcd.print("0");
  }
  lcd.print(Now.hour());
  lcd.print(":");
    if(Now.minute()<10){
    lcd.print("0");
  }
  lcd.print(Now.minute());
  lcd.print(":");
    if(Now.second()<10){
    lcd.print("0");
  }
  lcd.print(Now.second());
}
//-------END-of-Display-RTC-Time-----------------------------
//-------Check&Seed-RTC-if-Nessassery-----------------------
//complete
void F_Check_RTC_accurcy(struct tm ntp_time_check, struct DateTime rtc_time_check){
   
  if((rtc_time_check.hour() == 0 && rtc_time_check.minute() == 25 && rtc_time_check.second() == 0) || (rtc_time_check.hour() == 13 && rtc_time_check.minute() == 00 && rtc_time_check.second() == 0 || RTC_reajust_overide==true) ){
    ntp_time_check=F_get_NTP();

    if((rtc_time_check.minute() - ntp_time_check.tm_min) > 15 || (rtc_time_check.minute() - ntp_time_check.tm_min) < -15){
      /* //debug 
      Serial.print("NTP time in Accucary Check: ");
      F_Display_RTC_Time(rtc_time_check);
      F_Display_NTP_time(ntp_time_check);
      */
      
      int savedHour= ntp_time_check.tm_hour;
      int savedMin= ntp_time_check.tm_min;
      int savedSec= ntp_time_check.tm_sec;
      int savedMonth= ntp_time_check.tm_mon+1;  //off set of 1hour
      int savedYear= ntp_time_check.tm_year+1900; //offset relative to 1900
      int savedDay= ntp_time_check.tm_mday;

      rtc.adjust(DateTime(savedYear, savedMonth, savedDay, savedHour, savedMin, savedSec));    //NTP info seeds RTC //year,month,day,hour,min,sec
      Serial.println("RTC was reseeded with NTP Time successufly");
    }
  }
}
//------END-of-Check&Seed-RTC-if-Nessassery-------------------
//------Display-date-to-LCD-----------------------------------
void F_Display_RTC_Date(struct DateTime rtc_date){
  //lcd.setCursor(0, 0);    
  //dd/mm/yyy
  if(rtc_date.day() < 10){
    lcd.print("0");
  }
  lcd.print(rtc_date.day(), DEC); 
  lcd.print("/");
  if(rtc_date.month() < 10){
    lcd.print("0");
  }
  lcd.print(rtc_date.month(), DEC);
  lcd.print("/");
  lcd.print(rtc_date.year(), DEC);
}
//-------END-Of-Display-date-to-LCD---------------------------
//-----Display-Time&Date-TO-LCD-Cycle-between-----------------
unsigned long F_Time_Date_Cycle_Display(struct DateTime rtc_Display, unsigned long PreviousMillis){
  //cycle the display timer two displays
  unsigned long currentMillis=millis();
  int cycleDelay= 15000;

  if((currentMillis - PreviousMillis) > cycleDelay){ //if cycle delay occured toggle the output
    PreviousMillis = currentMillis;
    row1LCD=!row1LCD;
  }
  if(row1LCD){ 
    //display Date
    lcd.setCursor(0, 0);
    F_Display_RTC_Date(rtc_Display);
  }
  else{
    //display Time
    lcd.setCursor(0, 0);
    lcd.print("  ");  //clear to line up with date
    lcd.setCursor(2, 0);
    F_Display_RTC_Time(rtc_Display);
  }
  return PreviousMillis;
}
//-----END-Of-Display-Time&Date-TO-LCD-Cycle-between-------------
//-----SetUp-Of-RTC-&-manual-adjust------------------------------
void initilize_RTC(){
  if(!rtc.begin()){
    lcd.setCursor(0, 3);
    lcd.print("Couldn't find RTC");
  while (1) delay(10);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power");
    //rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0)); //manual seed RTC 
  }
}
//-----END-Of-SetUp-Of-RTC-&-manual-adjust----------------------
//rework 
void F_Password_input(){
  char key = keypad.getKey();

  lcd.setCursor(0,2);
  lcd.print(x_locate);  //used to move LCD Cusor
  
  if(key){  
    //any key pressed 
    lcd.setCursor((15 + x_locate), 3);
    lcd.print(key);
    if(x_locate > 0){   //*lags user input by 1 to sudo hide password on LCD
      lcd.setCursor(14 + x_locate,3);
      lcd.print('*');
    }
    x_locate++;
    
    if(key== '*'){
      //* pressed DELETE
      input_Password="";  //delete previous input 
      x_locate=0;         //reset Cursor
      
      lcd.setCursor(10, 3);
      lcd.print("Deleted");
      
      F_Progress_dots();
      F_clear_LCD_R3();

      Serial.println("Password entry Deleted");
    }
    else if(key== '#'){
      //# pressed check input
      //==password
      Serial.println("Password entered");
      if(input_Password== Saved_Password){
        Serial.println("Password Correct");
        //testing telegram alirt
        alarmAlirtsent=false;
        
        //Password Correct
        //system state change
        isSet= !isSet;   //toggle system state armed/disarmed

        F_Clear_StateDisplay(); //clear LCD on state change
       
        input_Password="";
        x_locate=0;
        
        lcd.setCursor(10, 3);
        lcd.print("Correct");

        F_Progress_dots();
        F_clear_LCD_R3();

        if(isSet){ 
          bot.sendMessage(CHAT_IDTEST, "Alarm Has been Armed", "");
          
          F_Arm_Delay();
        }
        else{
          bot.sendMessage(CHAT_IDTEST, "Alarm Has been Disarmed", "");
        }
      }
      else{
        //Password incorrect
        //system no change state
        fail_count++;  //add 1 to fail count 

        x_locate=0;
        input_Password="";
        
        lcd.setCursor(8, 3);
        lcd.print("Incorrect");

        F_Progress_dots();
        F_clear_LCD_R3();
      }
    }
    else{
     // lcd.print("Incorrect!");
    input_Password+= key;
    //PAssword test output input to LCD
    lcd.setCursor(0,1);
    lcd.print(input_Password);
    }
  } 
}
//-----Write-""-Over-LCD-Row-3------------------------
void F_clear_LCD_R3(){
  lcd.setCursor(0, 3);
  lcd.print("                    "); 
}
//-----END-of-Write-""-Over-LCD-Row-3----------------
//-----LCD-Display-Progress-Dots-With-Delay----------
void F_Progress_dots(){
  int delay_Dot= 500;
  int dot_count= 3;
  
  for(int i=0; i<dot_count; i++){
    lcd.print(".");
    delay(delay_Dot);
  }
}
//-----END-Of LCD-Display-Progress-Dots-With-Delay---
//-----Setup-Of-SD-Card------------------------------
void initilize_SD(){
  if(!SD.begin()){
    Serial.println("Card Mount Failed");
    return;
  }
  
  uint8_t cardType = SD.cardType();
  
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
}
//------END-of-Setup-Of-SD-Card-----------------------
//------Update-Gobal-variable-From-SD-Card------------
//*******change when salt added to stored password***
void F_Retreve_SD_Password(){
  File FilePassword = SD.open("/PasswordFile.txt");  //openPasswordFile on SD
  
  if(FilePassword){ //if open
    char c;
    uint8_t i=0;

    while(FilePassword.available()){
      c=FilePassword.read();  //reads each char 1 by 1 
      if(c!='\n'){
        Saved_Password[i]=c;  //save each char into array
        i++;
      }
      else{   //if end of file
        Saved_Password[i]='\0';
        FilePassword.close(); 
        Serial.print("FilePassword file closed succes");
        break;
      }
    }
  }
  Serial.println("Password was retreved");
  FilePassword.close(); 
}
//-----END-Update-Gobal-variable-From-SD-Card---------
//-----Display-System-State---------------------------
void F_Display_State(){
  if(isSet==true){  //show the state of the system to the user armed/disarmed
    lcd.setCursor(13, 0);
    lcd.print("Armed");
  }
  else{
    lcd.setCursor(12, 0);
    lcd.print("Disarmed"); 
  }
}
//-----End-of-Display-System-State---------------------
//-----Clear-State-region-of-LCD-TR--------------------
void F_Clear_StateDisplay(){ //called if there is a state change clear previous output to lcd
  lcd.setCursor(12, 0); 
  lcd.print("        "); 
}
//----END-of-Clear-State-region-of-LCD-TR-------------
//----Change-Password---------------------------------
void F_Change_Password(){
  //rewrite to the password file 
  //incomplete
}
//----END-of-Change-Password-------------------------
//----write-To-SD------------------------------------
void writeFile(fs::FS &fs, const char * path, const char * message){
    //example writeFile(SD, "/fileName.txt", "text");
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}
//----END-of-write-To-SD----------------------------
//----Delete-File-From-SD----------------------------
void deleteFile(fs::FS &fs, const char * path){
    //example deleteFile(SD, "/fileName", "text");
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}
//---END-of-Delete-File-From-SD----------------------
//---On-Receved-Data---------------------------------
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  //complete
  memcpy(&moduleData, incomingData, sizeof(moduleData));  
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Device: ");
  Serial.println(moduleData.Device);
  Serial.print("Location: ");
  Serial.println(moduleData.Location);
  Serial.print("ID:  ");
  Serial.println(moduleData.ID);
  Serial.print("Event Nub: ");
  Serial.println(moduleData.EventCount);
  Serial.print("Alarm: ");
  Serial.println(moduleData.isAlarm);
  Serial.println();
}
//----END-of-On-Receved-Data--------------------------
//---Local-Logs-Write-to-SD--------------------------
void F_Save_Local_Log(){
  //***improve that saved_time[] is dynamicly sized in realation to sub stings +1******
  //save time, event, locaion running count to SD card
  if(!isLogSaved){ //only write one Log
    if(is_Alarm == true){
      struct DateTime log_Time= F_Get_RTC_Time();
      char saved_time[100]; //char array used as temp storage of Log can be improved

      sprintf(saved_time, "%d:%d:%d||%d/%d/%d||ALARM||%S||%S   ||%D||", log_Time.hour(),log_Time.minute(),log_Time.second(),log_Time.day(),log_Time.month(),log_Time.year(),moduleData.Location,moduleData.Device,moduleData.EventCount);
      Serial.println(saved_time);

      writeLog(SD, "/LogFile.txt", saved_time);
      LogCount++;
      isLogSaved=true;
    }
  }
}
//---END-of-Local-Logs-Write-to-SD--------------------
void F_EncryptPassword(){
  //uncompleted

  //take in users char[] of lenght 4 add 6 rand numbers to make a total lenght of 10 
  //encrypt each index using the same index of the key
  //return encrpyted word
  char Encrypted_Password[10]={'0','0','0','0','0','0'}; //inputer + cypher
  char input_Password[10]={'1','2','3','4','5'}; //user input password for testing
  int Encrpt_Key[10]={1,0,1,0}; //key to shift input using index 

  for(int i = 0; (i < 10 && input_Password[i] !=  '\0'); i++){
   Encrypted_Password[i]= input_Password[i] + Encrpt_Key[i];
   Serial.println("Password encrypted successfuly");
  }
  lcd.setCursor(0, 4);
  lcd.print(Encrypted_Password);

  char check_encrypted[4]={ Encrypted_Password[0],  Encrypted_Password[1], Encrypted_Password[2],  Encrypted_Password[3]};

  lcd.setCursor(9, 4);
  lcd.print(check_encrypted[0]);
  lcd.print(check_encrypted[1]);
  lcd.print(check_encrypted[2]);
  lcd.print(check_encrypted[3]);
  
  delay(10000);
}
//----Failed-Attempts-Check------------------
void F_fail_count_check(){
  //only account for keypad
  //Possible add wrong RFID 
  if(fail_count>=3){
    Serial.println("Alarm triggered just to Fail Count");
    is_Alarm=true;  //trigger alarm
    fail_count=0;
  }
}
//----END-of-Failed-Attempts-Check----------

//-----Start-of-Telegram-Bot----------------------------------
void initilize_telegram_Bot(){
  //completed
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org

  while (WiFi.status() != WL_CONNECTED){  //check connection
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Telegram_Bot init Success");
}
//--------If-New-message-sent-from-Telegram----------------------------
void handleNewMessages(int numNewMessages){
  //completed only update to add new menus
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);
  Serial.println("new telegram Message revieved");

  for (int i = 0; i < numNewMessages; i++){
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")//allow users if need set to all currently
      from_name = "Guest";

    if (text == "/status"){
      //diplay current Set State
      if (isSet){
        bot.sendMessage(chat_id, "Armed", "");
        F_Arm_Delay();
      }
      else{
        bot.sendMessage(chat_id, "Disarmed", "");
        F_Clear_Alarm_Display();
        alarmAlirtsent=false;
      }
    }
    if(text == "/info"){  
      //info regarding project
      bot.sendMessage(chat_id, "This is an user interactive Bot for interaction with Aidans Y3 Home Alarm Project");
    }
    if(text == "/arm"){
      isSet=true;
      F_Clear_StateDisplay();
      F_Display_State();
    }
    if(text == "/disarm"){
      isSet=false;
      F_Clear_StateDisplay();
      F_Display_State();
    }
    //Menu prompt
    if (text == "/start"){
      String welcome = "Project Telegram bot, " + from_name + ".\n\n";
      welcome += "/info :  on Project\n";
      welcome += "/arm : Turn on Alarm\n";
      welcome += "/disarm : Turn off Alarm\n";
      welcome += "/status : Returns current status of LED\n";

      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}
//------END-of-If-New-message-sent-from-Telegram----------------
//------READ-&-CHECK-RFID-------------------------
//complete
void readRFID(void){
  ////Read RFID card
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  // Look for new 1 cards
  if (!rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial())
    return;

  // Store NUID into nuidPICC array
  for (byte i = 0; i < 4; i++) {
    nuidPICC[i] = rfid.uid.uidByte[i];
  }

  for(int i=0; i<4; i++){
    //check input VS saved RFID card
    if(rfid.uid.uidByte[i]==Passs[i]){
      isRIGHTrfid=true;
    }
    else{
      isRIGHTrfid=false;
    }
  }
  
  if(isRIGHTrfid){
    //correct RFID
    Serial.println("Correct Card was Scanned");

    isSet= !isSet;
    F_Clear_StateDisplay();
    if(isSet==true){
      F_Arm_Delay();
      //send update to Telegram
      bot.sendMessage(CHAT_IDTEST, "Armed by RFID");
    }
    else{
      bot.sendMessage(CHAT_IDTEST, "Disarmed by RFID");
      F_Clear_Alarm_Display();
      alarmAlirtsent=false;
    }
  }
  else{
    //incorrect RFID
    Serial.println("Incorrect Card was Scanned");
  }
  Serial.print(F("RFID In dec: "));
  printDec(rfid.uid.uidByte, rfid.uid.size);
  Serial.println();
  // Halt PICC
  rfid.PICC_HaltA();
  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}
//------END-of-READ-&-CHECK-RFID------------------------
//---print-HEX-to-serial------------------------
//complete
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}
//---END-of-print-HEX-to-serial------------------
//---print-deciaml-to-serial---------------------
//complete
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}
//---END-of-print-deciaml-to-serial---------------
//---initialize-RFID------------------------------
//complete
void initilize_RFID(){
  Serial.println(F("Initialize System"));
  rfid.PCD_Init();
  Serial.print(F("Reader :"));
  rfid.PCD_DumpVersionToSerial();
}
//---END-of-initialize-RFID-----------------------
//can improve
void F_Arm_Delay(){
  //delay before Arming
  Serial.println("Start Arm Delay");

  lcd.setCursor(3,2);
  lcd.print("Arming in: ");
  
  for(int i=10; i>0; i--){
    if(i==10){
      lcd.setCursor(13,2);
    }
    else{
      lcd.setCursor(13,2);
      lcd.print(" ");
    }
    delay(armDelay);
    lcd.print(i);
  }
  lcd.setCursor(0, 2);
  lcd.print("                    "); 
}
//---CLear-LCD-of-Alarm-notification------------
//complete
void F_Clear_Alarm_Display(){
  //Clear Row 2&3 currently
  lcd.setCursor(5, 1);
  lcd.print("        ");
  lcd.setCursor(5, 2);
  lcd.print("          ");  
}
//---END-CLear-LCD-of-Alarm-notification---------
//---Append-data--------------------------------------
//complete
void writeLog(fs::FS &fs, const char * path, const char * message){
    //example writeFile(SD, "/fileName.txt", "text");
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        file.println();
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}
//---END-of-Append-data------------------------------

void initilize_LCD(){
  lcd.init();
  lcd.backlight();
  lcd.noBlink();
  lcd.noCursor();
}