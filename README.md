# csvtools, fast processing of CSV streams
[![Build Status](https://travis-ci.org/DavyLandman/csvtools.svg?branch=master)](https://travis-ci.org/DavyLandman/csvtools)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/5024.svg)](https://scan.coverity.com/projects/5024)
[![codecov.io](https://codecov.io/github/DavyLandman/csvtools/coverage.svg?branch=master)](https://codecov.io/github/DavyLandman/csvtools?branch=master)


As our data gets bigger, CSV files grow in size.
The CSV format is not exactly pipe-friendly due to embedded newlines and quoted separators.
[onyxfish/csvkit](https://github.com/onyxfish/csvkit) offers a great set of utilties for most tasks you would want to perform on CSV's in a gnu toolset kind of way.
However, it is not fast. For reasonable data sets, this doesn't matter, but for CSVs of more than a few MBs, you start to feel the pain.

This repository contains gnu-alike tools for parsing [RFC 4180](https://tools.ietf.org/html/rfc4180) CSVs at high speed.

## Tools

- `csvcut` a `cut(1)` equivalent to drop columns from a csv file
- `csvgrep` a `grep(1)` equivalent to match on one or more collumns per row, and only keep the rows matching all or any of the patterns. (it uses PRCE for regular expression goodness)
- `csvawk` a wrapper for `awk(1)` which correctly recognizes rows and cells (even across newlines). This is comparable to [geoffroy-aubry/awk-csv-parser](https://github.com/geoffroy-aubry/awk-csv-parser), except that it also supports embedded newlines.
- `csvpipe` and `csvunpipe` translate the newlines separating rows to `\0` such that `sort -z` and `uniq -z` and other null-terminated-line based tools can be used more correctly.

## Performance

Benchmarking is complicated, the primary goal is to measure only that of interest, by reducing the impact of other factors. Originally csvtools was benchmarked on the [Canada 2011 census](http://www12.statcan.gc.ca/census-recensement/2011/dp-pd/prof/details/download-telecharger/comprehensive/comp-csv-tab-dwnld-tlchrgr.cfm?Lang=E), however, we were primarily measuring the limits of the SSD and the caches around the file system. 

Now we benchmark with a custom tool: [`bench/runner.c`](bench/runner.c). This benchmark first generates an in memory random csv data set (see [`bench/generate.c`](bench/generate.c)), and then pipes this into the applications under test. This at least takes the IO and FS out of the equation.

we compare `csvtools` with other solutions. Note that these solutions might not correctly handle CSV's. The reported numbers are _median_ MiB/s.

### Pure pipe speed

| command | median speed |
| :-- | --: |
| `cat > /dev/null` | 2042.1 MiB/s |
| `wc -l > /dev/null` | 2149.0 MiB/s |
| `md5sum > /dev/null` | 566.8 MiB/s |


### csvcut

| scenario | csvkit | cut | sed | csvtools |
| :--- | ---: | ---: | ---: | ---: |
| first column | 8.0 MiB/s | 278.8 MiB/s | 356.9 MiB/s | _644.1 MiB/s_ |
| middle column  | 8.1 MiB/s | 280.3 MiB/s |  138.6 MiB/s | _555.8 MiB/s_ |
| last column | 8.0 MiB/s | 280.0 MiB/s | 90.1 MiB/s | _565.0 MiB/s_ |
| two adjoining columns | 7.3 MiB/s | 359 MiB/s | 59.6 MiB/s | _561.6 MiB/s_ |
| two distinct columns | 7.3 MiB/s | 449 MiB/s | 59.8 MiB/s | _480.9 MiB/s_ |

So even compared to sed or cut, which aren't handling quoted separators correctly, our `csvcut` is much faster. 

### csvgrep

| scenario | csvkit | grep | awk | csvtools |
| :--- | ---: | ---: | ---: | ---: |
| first column | 7.6 MiB/s | 347.9 MiB/s | 469.2 MiB/s | _588.0 MiB/s_ |
| middle column | 7.8 MiB/s | 302.8 MiB/s | 379.3 MiB/s | _579.0 MiB/s_ |
| last column | 7.7 MiB/s | 392.7 MiB/s | 341.5 MiB/s | _632.5 MiB/s_ |
| two distinct columns | 9.0 MiB/s | 273.9 MiB/s | 380.0 MiB/s | _569.7 MiB/s_ |

Faster than grep and awk, this is because the column selection in grep is done with negative character classes multiple times.

There are off course regular expressions possible where PCRE is slower than grep.

### csvawk

| scenario | awk | awk-csv-parser | csvtools |
| :--- | ---: | ---: | ---: |
| print second column | 428.5 MiB/s | 2.45 MiB/s | _278.5 MiB/s_ |
| sum last column | 350.5 MiB/s | 2.4 MiB/s | _225.9 MiB/s_ |

Sadly, `csvawk` is slower than pure `awk`. This is caused by the custom record separator (instead of the normal newline). Benchmarking `csvawk` piping to `awk` shows it performs around 800 MiB/s, and if newlines are used as separators, the whole `csvawk` performs around similar to `awk`'s raw performance. 

However, newlines are not valid separators, since they can occur inside quoted fields. For `csvawk` we generate [`\x1E`](https://en.wikipedia.org/wiki/C0_and_C1_control_codes#Field_separators) between records (as per ISO 646), and [`\x1F`](https://en.wikipedia.org/wiki/C0_and_C1_control_codes#Field_separators) between fields in a record. 

The results of the second benchmark differ, since awk doesn't correctly handle nested separators.

### Why so fast?
No malloc & memcpy!

Or as valgrind reports it:
```
==2473==   total heap usage: 18 allocs, 18 frees, 210 bytes allocated
```

In the critical path of tokenizing the csv stream and writing it to `stdout`, there are no copies or memory allocations. The programs read into a buffer from `stdin` (or the file passed as last argument), the tokenizer stores offsets (to that buffer) and lenghts in a cell array, and the printer writes from the same buffer, using the offsets and lengths from the cell array. 

## Instalation

1. Clone this repository
2. Navigate to it
2. `make install` (or with prefix: `make install prefix=~/.apps/`)
3. enjoy :)

## Future work

- Decide on issue #4
- Think of better names that don't clash with csvkit?
- More tests
- add option to remove the header
- sort on columns?

