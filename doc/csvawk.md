# csvawk

## Benchmarking

### Print second column

```
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' awk -F"," '{ print $2; }' | md5sum > /dev/null
[ 317MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' awk -f ../awk-csv-parser/src/csv-parser.awk -v separator=',' -v enclosure='"' --source '{ csv_parse_record($0, separator, enclosure, csv); print csv[1]; }' | md5sum > /dev/null
[2.21MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' bin/csvawk '{ print $2; }' | md5sum > /dev/null
[ 307MiB/s]
```

### Count numeric column

```
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' awk -F"," '{ s += $7; } END { print s; }'
[ 219MiB/s]
3.67433e+08
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' awk -f ../awk-csv-parser/src/csv-parser.awk -v separator=',' -v enclosure='"' --source '{ csv_parse_record($0, separator, enclosure, csv); s += csv[6]; } END { print s; }' 
[2.23MiB/s]
3.69596e+08
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' bin/csvawk '{ s += $7; } END { print s; }' 
[ 217MiB/s]
3.69596e+08
```

(Here you can also see the normal awk failing to calculate the correct total.)
