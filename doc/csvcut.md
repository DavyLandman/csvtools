# csvcut

## Benchmarking:

#### Dropping first column

```
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | csvcut -e "cp1250" -c Geo_Code | md5sum > /dev/null
[4.32MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' cut -d ',' -f 2,3,4,5,6,7,8,9,10,11 | md5sum > /dev/null
[195MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' sed 's/^[^,]*,//' | md5sum > /dev/null
[ 228MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | bin/csvcut -d Geo_Code | md5sum > /dev/null
[ 318MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | md5sum  > /dev/null
[ 519MiB/s]
```

Remember, `sed`, and `cut` do not respect the embeded newlines, or a `,` inside a `"quoted cell"`.

#### Dropping a column in the middle

```
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | csvcut -e "cp1250" -c Topic | md5sum > /dev/null
[4.12MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' cut -d ',' -f 1,2,3,5,6,7,8,9,10,11 | md5sum > /dev/null
[ 224MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' sed 's/[^,]*,//4' | md5sum > /dev/null
[  91MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | bin/csvcut -d Topic | md5sum > /dev/null
[ 359MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | md5sum  > /dev/null
[ 527MiB/s]
```
