# digitime
Sync time tool for DIGI scales, verified on SM-100, SM-300, SM-500 models<br>
### Usage:<br>
digitime -i host -p port [-d date] [-t time] [-v]<br>
Format date and time: -d DDMMYY, -t HHMM.<br>
### Example:<br>
>digitime -i 192.168.1.10 -p 2010<br>		
>digitime -i 192.168.1.10 -p 2010 -d 240517 -t 1053 -v<br>


Консольная утилита для перевода даты и времени на весах DIGI.<br>
На SM-100 команда перевода отправляется в другом формате, поэтому сделано определение модели весов.<br>
Проверено и работает на моделях:<br>
 * SM-100
 * SM-300
 * SM-500