14/03/23 LCD wifi start up promt
14/03/23 telegram conflit with ESP-NOW
14/03/23 RFID causing the system to stall for 6-10seconds
14/03/23 bug where if RTC time would accurate to the min by +-15 but out by hours 
16/03/23 updated pinout of keypad


## Known bugs
- WiFi not present on start up causing a RTC reset
- Teligram has no knowlege of systems network connectivity
- incorrect NTP server causings unexpected Day light savings offset
## TASK LIST
- [ ] Tramper trigger
- [ ] Buzzer mount
- [ ] Store RFID ID in flash 
- [ ] Encrypt RFID ID
- [ ] improve telegram menus
- [ ] Debug mode dip switch
- [ ] Create system logs
- [ ] Batch write system logs
- [ ] Create custom library
- [ ] Allow settings change through telegram
- [ ] WiFi manager when no WiFi present
- [ ] WiFi manager dip switch
- [ ] dynamicly alocate