# csvtools, fast processing of CSV streams
[![Build Status](https://travis-ci.org/DavyLandman/csvtools.svg?branch=master)](https://travis-ci.org/DavyLandman/csvtools)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/5024.svg)](https://scan.coverity.com/projects/5024)

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

Benchmarking on the  [Canada 2011 census](http://www12.statcan.gc.ca/census-recensement/2011/dp-pd/prof/details/download-telecharger/comprehensive/comp-csv-tab-dwnld-tlchrgr.cfm?Lang=E) we compare `csvtools` with other solutions. Note that these solutions might not correctly handle CSV's.

The performance was measured with a 850MB csv file on a SSD drive, and the maximum speed was 519MB/s.

### [csvcut](doc/csvcut.md)

| scenario | csvkit | cut | sed | csvtools |
| :--- | ---: | ---: | ---: | ---: |
| dropping first column | 4.32 MiB/s | 195 MiB/s | 228 MiB/s | _318 MiB/s_ |
| dropping third column | 4.12 MiB/s | 224 MiB/s | 91 MiB/s | _359 MiB/s_ |

So even compared to sed or cut, which aren't handeling quoted separators correctly, our `csvcut` is much faster.

### [csvgrep](doc/csvgrep.md)

| scenario | csvkit | grep | awk | csvtools |
| :--- | ---: | ---: | ---: | ---: |
| one pattern | 1.86 MiB/s | 284 MiB/s | 208 MiB/s | _310 MiB/s_ |
| two patterns | 1.87 MiB/s | 224 MiB/s | 140 MiB/s | _258 MiB/s_ |

Faster than grep and awk, this is because the column selection in grep is done with negative character classes multiple times.

There are ofcourse regular expressions possible where PCRE is slower than grep.

### [csvawk](doc/csvawk.md)

| scenario | awk | awk-csv-parser | csvtools |
| :--- | ---: | ---: | ---: |
| print second column | 317 MiB/s | 2.21 MiB/s | _307 MiB/s_ |
| count numeric column | 219 MiB/s | 2.23 MiB/s | _217 MiB/s_ |

Since we wrap `awk` the awk raw is the maximum speed, but we can see only a small overhead, for accurate results.

### Why so fast?
No malloc & memcpy!
Or as valgrind reports it:
```
==2473==   total heap usage: 18 allocs, 18 frees, 210 bytes allocated
```

In the crititical path of tokenizing the csv stream and writing it to `stdout`, there are no copies or memory allocations. The programs read into a buffer from `stdin` (or the file passed as last argument), the tokenizer stores offsets (to that buffer) and lenghts in a cell array, and the printer writes from the same buffer, using the offsets and lengths from the cell array. 

## Instalation

1. Clone this repository
2. Navigate to it
2. `make install` (or with prefix: `make install prefix=~/.apps/`)
3. enjoy :)
