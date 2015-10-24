#!/bin/bash
echo "csvcut"
pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | bin/csvcut -d Geo_Code | md5sum > /dev/null
pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | bin/csvcut -d Topic | md5sum > /dev/null
echo "csvgrep"
pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | bin/csvgrep -p 'Characteristic/[a-e]+/' | md5sum > /dev/null
pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | bin/csvgrep -p 'Characteristic/[a-e]+/' -p 'Topic/[f-l]+/'] | md5sum > /dev/null
echo "csvawk"
pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' bin/csvawk '{ print $2; }' | md5sum > /dev/null
pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' bin/csvawk '{ s += $7; } END { print s; }' 
