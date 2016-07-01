#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PI 3.141592653589793
#define EARTH_MAJOR_AXIS 6378140.
#define EARTH_MINOR_AXIS 6356755.
#define EARTH_AXIS_RATIO (EARTH_MINOR_AXIS / EARTH_MAJOR_AXIS)
#define SWAP( A, B, TEMP) { TEMP = A; A = B; B = TEMP; }

/* The following code reads in the MPC file ObsCodes.txt,  which gives
longitudes and parallax constants;  and outputs the same file with new
columns for latitude and altitude.  Be warned that the parallax
constants are usually given to five places,  or a precision of about
64 meters.  Therefore,  the latitude and longitude are good to a
similar precision.  The longitude is given to .0001 degree precision,
and is therefore good to about 11 meters.  Some stations give one more
or one fewer decimal place,  with corresponding change in precision
(though not necessarily accuracy).  Also,  note that the altitudes that
are computed are relative to the ellipsoid,  not to the geoid/mean sea level.

   It's been modified a bit so that it can read in lat/lon rectangles
from the 'geo_rect.txt' file.  Each rectangle covers part of the interior
of a state or nation;  if a given lat/lon from 'ObsCodes.txt' falls within
that rectangle,  you can assign that state/nation data to it.  I first made
a slew of rectangles covering most MPC codes,  ran this program,  found
some codes without state/nation info,  and made some more rectangles to
cover those codes.  Every now and then,  I have to add another rectangle
to cover a new code.
*/


#define GEO_RECT struct geo_rect

GEO_RECT
   {
   double lat1, lon1, lat2, lon2;
   char name[60];
   };

int load_geo_rects( GEO_RECT *rects)
{
   FILE *ifile = fopen( "geo_rect.txt", "rb");
   int rval = 0;
   char buff[100];

   if( ifile)
      {
      while( fgets( buff, sizeof( buff), ifile))
         if( sscanf( buff, "%lf %lf %lf %lf",
                  &rects[rval].lon1, &rects[rval].lat1,
                  &rects[rval].lon2, &rects[rval].lat2) == 4)
            {
            double temp_double;
            int i;

            if( rects[rval].lat1 > rects[rval].lat2)
               SWAP( rects[rval].lat1, rects[rval].lat2, temp_double);
            if( rects[rval].lon1 > rects[rval].lon2)
               SWAP( rects[rval].lon1, rects[rval].lon2, temp_double);
            for( i = 0; buff[i] >= ' '; i++)
               ;
            while( i < 56)          /* pad the string */
               buff[i++] = ' ';
            buff[i] = '\0';
            strcpy( rects[rval].name, buff + 42);
                     /* Crossing the prime meridian is a problem,  since
                        you go from longitude 359.9999 to .0001.  This is
                        handled by making two rectangles,  one on each side
                        of the prime meridian.  */
            if( rects[rval].lon2 - rects[rval].lon1 > 180.)
               {
               SWAP( rects[rval].lon1, rects[rval].lon2, temp_double);
               rects[rval + 1] = rects[rval];
               rects[rval].lon2 = 361.;
               rects[rval + 1].lon1 = -1.;
               rval++;
               }
            rval++;
            }
      fclose( ifile);
      }
   return( rval);
}

int code_compare( const void *a, const void *b)
{
   const char *a1 = (const char *)(*(const char **)a);
   const char *b1 = (const char *)(*(const char **)b);
   int rval = strcmp( a1 + 4, b1 + 4);

   if( !rval)        /* same country */
      rval = strcmp( a1, b1);
   return( rval);
}

#define MAX_RECTS 1000
#define MAX_CODES 8000

int main( const int unused_argc, const char **unused_argv)
{
   char buff[190];
   int in_code_section = 0, n_geo_rects;
   FILE *ifile = fopen( "ObsCodes.htm", "rb");
   FILE *ofile;
   char **codes = (char **)calloc( MAX_CODES, sizeof( char *));
   int n_codes = 0, i, j;
   GEO_RECT *rects = (GEO_RECT *)calloc( MAX_RECTS, sizeof( GEO_RECT));

   if( !ifile)
      {
      printf( "stations.txt not opened\n");
      exit( 0);
      }
   n_geo_rects = load_geo_rects( rects);
   while( fgets( buff, sizeof( buff), ifile))
      {
      if( !memcmp( buff, "000  ", 5))
         in_code_section = 1;
      if( !memcmp( buff, "</pre>", 6))
         in_code_section = 0;
      if( in_code_section && strlen( buff) > 30 && buff[3] == ' ')
         {

         memmove( buff + 60, buff + 30, strlen( buff + 29));
         memset( buff + 30, ' ', 30);
         if( buff[7] == '.' && buff[14] == '.' && buff[23] == '.' &&
                       atof( buff + 13))
            {
            const double x = atof( buff + 13);
            const double y = atof( buff + 21);
            double lon = atof( buff + 4), lat;
            char alt_buff[7];
            int rect_no = -1, i;

            if( buff[19] != ' ' && buff[28] != ' ')
               {        /* make sure there's precision for altitude */
               double radius = sqrt( x * x + y * y);
               double ang = atan( y / x);
               double r_earth = cos( ang) * cos( ang) +
                        sin( ang) * sin( ang) * EARTH_AXIS_RATIO;

               sprintf( alt_buff, "%5.0f",
                            (radius - r_earth) * EARTH_MAJOR_AXIS);
               }
            else        /* put blanks in for altitude */
               strcpy( alt_buff, "     ");

            lat = atan( y / (x * EARTH_AXIS_RATIO * EARTH_AXIS_RATIO));
            lat *= 180. / PI;
            for( i = 0; i < n_geo_rects; i++)
               if( rects[i].lon1 < lon && rects[i].lon2 > lon)
                  if( rects[i].lat1 < lat && rects[i].lat2 > lat)
                     rect_no = i;
            if( lon > 180.)
               lon -= 360.;
            sprintf( buff + 30, "%8.4f %s ", lat, alt_buff);
            if( rect_no >= 0)
               memcpy( buff + 45, rects[rect_no].name, 15);
            else
               memset( buff + 45, ' ', 15);
            buff[59] = ' ';
            if( rect_no >= 0)
               {
               char name[90], *tptr = rects[rect_no].name;
               const char *dropouts[4] = { "SAfric", "CzechR", "NewZea", NULL };
               int include_both_parts = 1;

               memcpy( name, buff, 3);
               name[3] = ' ';
               if( !memcmp( tptr, tptr + 7, 6))
                  include_both_parts = 0;
               for( i = 0; dropouts[i]; i++)
                  if( !memcmp( dropouts[i], tptr, 6))
                     include_both_parts = 0;
               strcpy( name + 4, tptr + (include_both_parts ? 0 : 7));
               codes[n_codes] = (char *)malloc( strlen( name) + 1);
               strcpy( codes[n_codes], name);
               n_codes++;
               }
            }
         if( in_code_section)
            printf( "%s", buff);
         }
      }
   qsort( codes, n_codes, sizeof( char *), code_compare);
   ofile = fopen( "shortmpc.txt", "wb");
   j = 0;
   for( i = 0; i < n_codes; i++)
      {
      const int codes_per_line = 15;

      codes[i][3] = '\0';
      if( !i || strcmp( codes[i] + 4, codes[i - 1] + 4))
         {
         j = 0;
         fprintf( ofile, "\n# %s\n", codes[i] + 4);
         }
      j++;
      fprintf( ofile, "%s%s", codes[i], (j % codes_per_line ? " " : "\n"));
      }
   fclose( ofile);
}