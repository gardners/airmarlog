/*
  Stand-alone program to log GPS and climatic data from AirMar WX150 and
  compatible weather stations.  Designed to run on a beagleboard XM.

  Paul Gardner-Stephen (paul.gardner-stephen@flinders.edu.au)
  (C) Flinders University, 2015.
  Portions copyright Serval Project Inc., 2014-2015.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

int set_nonblock(int fd)
{
  int flags;
  if ((flags = fcntl(fd, F_GETFL, NULL)) == -1)
    return -1;
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    return -1;
  return 0;
}

int set_block(int fd)
{
  int flags;
  if ((flags = fcntl(fd, F_GETFL, NULL)) == -1)
    return -1;
  if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1)
    return -1;
  return 0;
}

int setup_serial_port(int fd, int baud)
{
  struct termios t;

  if (tcgetattr(fd, &t)) return -1;
    
  speed_t baud_rate;
  switch(baud){
  case 0: baud_rate = B0; break;
  case 50: baud_rate = B50; break;
  case 75: baud_rate = B75; break;
  case 110: baud_rate = B110; break;
  case 134: baud_rate = B134; break;
  case 150: baud_rate = B150; break;
  case 200: baud_rate = B200; break;
  case 300: baud_rate = B300; break;
  case 600: baud_rate = B600; break;
  case 1200: baud_rate = B1200; break;
  case 1800: baud_rate = B1800; break;
  case 2400: baud_rate = B2400; break;
  case 4800: baud_rate = B4800; break;
  case 9600: baud_rate = B9600; break;
  case 19200: baud_rate = B19200; break;
  case 38400: baud_rate = B38400; break;
  default:
  case 57600: baud_rate = B57600; break;
  case 115200: baud_rate = B115200; break;
  case 230400: baud_rate = B230400; break;
  }

  if (cfsetospeed(&t, baud_rate)) return -1;    
  if (cfsetispeed(&t, baud_rate)) return -1;

  // 8N1
  t.c_cflag &= ~PARENB;
  t.c_cflag &= ~CSTOPB;
  t.c_cflag &= ~CSIZE;
  t.c_cflag |= CS8;

  t.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO | ECHOE);
  /* Noncanonical mode, disable signals, extended
   input processing, and software flow control and echoing */
  
  t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR |
		 INPCK | ISTRIP | IXON | IXOFF | IXANY | PARMRK);
  /* Disable special handling of CR, NL, and BREAK.
   No 8th-bit stripping or parity error handling.
   Disable START/STOP output flow control. */
  
  // Enable/disable CTS/RTS flow control
#ifndef CNEW_RTSCTS
  t.c_cflag &= ~CRTSCTS;
#else
  t.c_cflag &= ~CNEW_RTSCTS;
#endif

  // no output processing
  t.c_oflag &= ~OPOST;

  if (tcsetattr(fd, TCSANOW, &t))
    return -1;
   
  set_nonblock(fd);

  return 0;
}

int next_char(int fd)
{
  int w=0;
  time_t timeout=time(0)+10;
  while(time(0)<timeout) {
    unsigned char buffer[2];
    int r=read(fd,(char *)buffer,1);
    if (r==1) {
      // if (w) { printf("[%d]",w); fflush(stdout); }
      return buffer[0];
    } else { usleep(1000); w++; }
  }
  return -1;
}

int year=1900;
int month=0;
int day=0;
int hour=0;
int minute=0;
int second=0;
float latitude=0;
float longitude=0;
float hdop=0;
float altitude_metres=0;
int gps_fixed=0;
int previous_hour=-1;
int previous_minute=-1;

int log_first_line=1;
FILE *log_file=NULL;
char *log_dir=NULL;

int logRotate()
{
  if (!log_dir) return 0;
  if (log_file) fclose(log_file); log_file=NULL;
  char log_name[1024];
  snprintf(log_name,1024,"%s/wx150log-%04d.%02d.%02d.%02d",
	   log_dir,year,month,day,hour);
  log_file=fopen(log_name,"w");
  return 0;
}

int processLine(char *line) {
  float temp_in_centigrade,relative_humidity,dew_point_in_centigrade;
  float pressure_in_inches,pressure_in_bars;
  int num_satellites;
  int time_of_day;
  char north_south[1024];
  char east_west[1024];

  if (sscanf(line,"$WIMDA,%f,I,%f,B,%f,C,,,%f,,%f,",
	     &pressure_in_inches,
	     &pressure_in_bars,
	     &temp_in_centigrade,
	     &relative_humidity,
	     &dew_point_in_centigrade)==5) {
    // meteorological data
    if ((!log_file)||previous_hour!=hour) logRotate();
    if (log_file) {
      if (log_first_line) {
	fprintf(log_file,"year;month;day;hour;minute;second;temperature_c;relativehumidity;airpressure_b;dewpoint_c;latitude;longitude;altitude;hdop;gpsfixed\n");
      }
      fprintf(log_file,"%d;%d;%d;%d;%d;%d;%f;%f;%f;%f;%f;%f;%f;%f;%d\n",
	      year,month,day,hour,minute,second,
	      temp_in_centigrade,relative_humidity,
	      pressure_in_bars,dew_point_in_centigrade,
	      latitude,longitude,altitude_metres,hdop,gps_fixed);      
      log_first_line=0;
      if (previous_minute!=minute) fflush(log_file);
    }
    previous_hour=hour;
    previous_minute=minute;
  } else if (sscanf(line,"$GPGGA,%d,%f,%[^,],%f,%[^,],%d,%d,%f,%f,",
		    &time_of_day,
		    &latitude,
		    north_south,
		    &longitude,
		    east_west,
		    &gps_fixed,
		    &num_satellites,
		    &hdop,
		    &altitude_metres)==9) {
    // GPS location and time of day
    hour=time_of_day/10000;
    minute=(time_of_day/100)%100;
    second=time_of_day%100;

    if (toupper(north_south[0])=='S') latitude=-latitude;
    if (toupper(east_west[0])=='E') longitude=-longitude;
    
  } else if (sscanf(line,"$GPZDA,%d,%d,%d,%d,",
		    &time_of_day,
		    &day,
		    &month,
		    &year)==4) {
    hour=time_of_day/10000;
    minute=(time_of_day/100)%100;
    second=time_of_day%100;
  } else {
    // unknown line -- just ignore it.
  }
  return 0;
}


int main(int argc,char **argv)
{
  if (argc!=3) {
    fprintf(stderr,"Usage: wx150logger <serial port> <log directory>\n");
    exit(-1);
  }

  log_dir=argv[2];
  
  int fd=open(argv[1],O_RDWR);
  if (fd==-1) {
    fprintf(stderr,"Could not open serial port '%s'\n",argv[1]);
    exit(-1);
  }
  if (set_nonblock(fd)) {
    fprintf(stderr,"Could not set serial port '%s' non-blocking\n",argv[1]);
    exit(-1);
  }  
  setup_serial_port(fd,4800);

  char line[1024]; line[0]=0;
  int linelen=0;
  while(1) {
    // XXX Read lines from WX150
    int c=next_char(fd);
    
    if (c=='\r'||c=='\n') {
      line[linelen]=0;
      if (linelen>0) processLine(line);
      linelen=0;
    } else if (c>0)  {
      if (linelen<1023) line[linelen++]=c;
      line[linelen]=0;
    } else usleep(10000);
    
  }
 
  return 0;
}
