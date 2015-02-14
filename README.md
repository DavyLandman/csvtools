# csvtools, fast processing of CSV streams

I've been using a lot of CSV files for analyzing big data sets.
The CSV format is not exactly pipe-friendly due to embedded newlines and quoted separators.
For a while I was using [onyxfish/csvkit](https://github.com/onyxfish/csvkit) which offers a great set of utilties for most tasks you would want to perform.
However, it is not fast. For reasonable data sets, this doesn't matter, but for the sizes of CSVs I was working with, I needed more.

This repository contains tools for parsing [RFC 4180](https://tools.ietf.org/html/rfc4180) CSVs.

## csvchop

Often I have to drop one or more columns from a stream.
`csvchop` reads data from `stdin` and outputs to `stdout` the same data without the chopped columns.

### Options
```
> ./csvchop -h
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

Check [`csvchop/README.md`](https://github.com/DavyLandman/csvtools/blob/master/csvchop/README.md) for the benchmark, which also compares to `cut` and `sed` solutions.

### Why so fast?
No malloc & memcpy!
Or as valgrind reports it:
```
==2473==   total heap usage: 18 allocs, 18 frees, 210 bytes allocated
```

In the crititical path of tokenizing the csv stream and writing it to `stdout`, there are no copies or memory allocations. `csvchop` reads into a buffer from `stdin`, the tokenizer stores offsets (to that buffer) and lenghts in a cell array, and the printer writes from the same buffer, using the offsets and lengths from the cell array. 

