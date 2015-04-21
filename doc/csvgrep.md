#csvgrep

## Benchmarking

### One column match

```
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | csvgrep -e "cp1250" -c Characteristic -r '.*[a-e]+.*' | md5sum > /dev/null
[1.86MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' grep "^[^,]*,[^,]*,[^,]*,[^,]*,[^,a-e]*[a-e][a-e]*[^,]*," | md5sum > /dev/null
[ 284MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' awk -F"," '$5 ~ /.*[a-e]+.*/ { print }' | md5sum > /dev/null
[ 208MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | bin/csvgrep -p 'Characteristic/[a-e]+/' | md5sum > /dev/null
[ 310MiB/s]
```

Note, to get reasonable speeds from grep and awk, we have to use `LC_ALL='C'` csvgrep doesn't need it, and the regular expression matching is also UTF8 compatible.

## Two column match

```
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | csvgrep -e "cp1250" -c Topic,Characteristic -r '.*[a-e]+.*' | md5sum > /dev/null
[1.87MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' grep "^[^,]*,[^,]*,[^,]*,[^,f-l]*[f-l][^,]*,[^,a-e]*[a-e][a-e]*[^,]*," | md5sum > /dev/null
[ 224MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | LC_ALL='C' awk -F"," '$4 ~ /.*[f-l]+.*/ && $5 ~ /.*[a-e]+.*/ { print }' | md5sum > /dev/null
[ 140MiB/s]
$ pv -a ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | bin/csvgrep -p 'Characteristic/[a-e]+/' -p 'Topic/[f-l]+/'] | md5sum > /dev/null
[ 258MiB/s]
```

csvkit's csvcut does not offer different patterns for different columns.
