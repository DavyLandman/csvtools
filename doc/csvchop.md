`csvchop` reads data from `stdin` and outputs to `stdout` the same data without the chopped columns.

### Options
```
-s ,
        Which character to use as separator (default is ,)
-k column,names,to,keep
-d column,names,to,drop
-K 0,1,3
	Which columns to keep
-D 0,1,3
	Which columns to drop
```

### Performance

The `csvcut` utility performs the same feature as `csvchop`.. Benchmarking on the  [Canada 2011 sensus](http://www12.statcan.gc.ca/census-recensement/2011/dp-pd/prof/details/download-telecharger/comprehensive/comp-csv-tab-dwnld-tlchrgr.cfm?Lang=E) data shows that `csvchop` runs at __150MB/s__ compared to `csvcut`'s 4.35MB/s.

Let's take a look at the numbers.  If we ignore embedded separators and newline, `cut` and `sed` could emulate similar functionality, so they are also included. 

### Benchmark

#### Dropping first column

```
$ cat ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | pipebench | csvcut -e "cp1250" -c Geo_Code > /dev/null
Piped  849.78 MB in 00h03m19.59s:    4.25 MB/second
$ cat ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | pipebench | cut -d ',' -f 2,3,4,5,6,7,8,9,10,11 > /dev/null
Piped  849.78 MB in 00h00m30.49s:   27.86 MB/second
$ cat ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | pipebench | sed 's/^[^,]*,//' > /dev/null
Piped  849.78 MB in 00h00m07.80s:  108.88 MB/second
$ cat ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | pipebench | ./csvchop -d Geo_Code > /dev/null
Piped  849.78 MB in 00h00m05.74s:  151.85 MB/second
$ cat ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | pipebench > /dev/null
Piped  849.78 MB in 00h00m00.30s:    2.75 GB/second
```

Remember, `sed`, and `cut` do not respect the embeded newlines, or a `,` inside a `"quoted cell"`.

#### Dropping a column in the middle

```
$ cat ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | pipebench | csvcut -e "cp1250" -c Topic > /dev/null
Piped  849.78 MB in 00h03m27.25s:    4.09 MB/second
$ cat ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | pipebench | cut -d ',' -f 1,2,3,5,6,7,8,9,10,11 > /dev/null
Piped  849.78 MB in 00h00m27.35s:   31.06 MB/second
$ cat ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | pipebench | sed 's/[^,]*,//4' > /dev/null
Piped  849.78 MB in 00h00m39.73s:   21.38 MB/second
$ cat ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | pipebench | ./csvchop/csvchop -d Topic > /dev/null
Piped  849.78 MB in 00h00m05.58s:  152.21 MB/second
$ cat ../csv-data/canada_sensus/98-316-XWE2011001-1501-ONT.csv | pipebench > /dev/null
Piped  849.78 MB in 00h00m00.30s:    2.75 GB/second
```
