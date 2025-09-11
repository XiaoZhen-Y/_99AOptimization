#include "Remote_Time.h"
#include "time.h"
#include "rx8010.h"
#include "app_types.h"
#include "app_conf.h"
#include "c_convert.h"

unsigned char hex_to_bcd(unsigned char data)
{
    unsigned char temp;

    temp = (((data/10)<<4) + (data%10));
    return temp;
}

uint8_t year_storage = 0,month_storage = 0,day_storage = 0,hour_storage = 0,min_storage = 0,sec_storage = 0;
T_TIME tChangeTime = 0;

void Remote_TimeChange(uint16_t *uspMbHoldData)
{
    struct tm_t tm;
	
	BOOL time_change = 0;
	static uint8_t first_time = 0;

//	1、年月日变量
	uint8_t year_pad = 0;
	uint8_t month_pad = 0;
	uint8_t day_pad = 0;
	uint8_t hour_pad = 0;
	uint8_t min_pad = 0;
	uint8_t sec_pad = 0;
	
//	2、获取年月日 *(usMbHoldData + 0x140) = 年
	year_pad = *(uspMbHoldData + YEAR_ADDRESS);
	month_pad = *(uspMbHoldData + YEAR_ADDRESS + 1);
	day_pad = *(uspMbHoldData + YEAR_ADDRESS + 2);
	hour_pad = *(uspMbHoldData + YEAR_ADDRESS + 3);
	min_pad = *(uspMbHoldData + YEAR_ADDRESS + 4);
	sec_pad = *(uspMbHoldData + YEAR_ADDRESS + 5);
	
	if(year_pad > 0 && year_pad <= 99){
		year_storage = year_pad;
	}else{
		year_pad = year_storage;
	}
	
	if(month_pad > 0 && month_pad <= 12){
		month_storage = month_pad;
	}else{
		month_pad = month_storage;
	}
	
	if(day_pad > 0 && day_pad <= 31){
		day_storage = day_pad;
	}else{
		day_pad = day_storage;
	}
	
	if(hour_pad > 0 && hour_pad <= 23){
		hour_storage = hour_pad;
	}else{
		hour_pad = hour_storage;
	}
	
	if(min_pad > 0 && min_pad <= 59){
		min_storage = min_pad;
	}else{
		min_pad = min_storage;
	}
	
	if(sec_pad > 0 && sec_pad <= 59){
		sec_storage = sec_pad;
	}else{
		sec_pad = sec_storage;
	}
	
	if(year_storage !=0 && month_storage != 0 && day_storage != 0){
		
		first_time = 1;
	}
	
	if(first_time == 1){
		tChangeTime = tChangeTime;
	}else{
		tChangeTime = tSysClock;
	}
	
//	3、审查
//	4、写入年月日时分秒

//	if(year_pad > 0 && year_pad <= 99 && month_pad > 0 && month_pad <= 12 &&
//		day_pad > 0 && day_pad <= 31 && hour_pad > 0 && hour_pad <= 23 &&
//		min_pad > 0 && min_pad <= 59 && sec_pad > 0 && sec_pad <= 59){
	
	if(tSysClock - tChangeTime >= 10){
			
		year_pad = hex_to_bcd(year_pad);
		month_pad = hex_to_bcd(month_pad); 
		day_pad = hex_to_bcd(day_pad);
		hour_pad = hex_to_bcd(hour_pad);
		min_pad = hex_to_bcd(min_pad);
		sec_pad = hex_to_bcd(sec_pad);
		
		RX8010_WriteYearWithBCD(&year_pad);
		RX8010_WriteMonWithBCD(&month_pad);
		RX8010_WriteMdayWithBCD(&day_pad);
		RX8010_WriteHourWithBCD(&hour_pad);
		RX8010_WriteMinWithBCD(&min_pad);
		RX8010_WriteSecWithBCD(&sec_pad);
		time_change = 1;
		first_time = 0;
	}
	
	
	//---@设置好时间
	if(time_change == 1)
	{
		uint8_t year, mon, mday, hour, min, sec;
		
		time_change = 0;

		RX8010_ReadYearWithBCD(&year);
		RX8010_ReadMonWithBCD(&mon);
		RX8010_ReadMdayWithBCD(&mday);
		RX8010_ReadHourWithBCD(&hour);
		RX8010_ReadMinWithBCD(&min);
		RX8010_ReadSecWithBCD(&sec);

		tm.tm_year = bcd_to_dec(year)+30;
		tm.tm_mon = bcd_to_dec(mon) - 1;
		tm.tm_mday = bcd_to_dec(mday);
		tm.tm_hour = bcd_to_dec(hour);
		tm.tm_min = bcd_to_dec(min);
		tm.tm_sec = bcd_to_dec(sec);

		tTimeStamp = sec_time(&tm);
		
		year_storage = month_storage = day_storage =  hour_storage = min_storage = sec_storage = 0;
	}
	
	//	5、清空数据
	memset(uspMbHoldData + YEAR_ADDRESS, -1, 6*sizeof(uint16_t));//---@置位-1，但是由于是无符号数，故变为FFFF
}




