Implementation of a compact RDf index originally introduced by [Navarro](https://doi.org/10.1017/CBO9781316588284) supporting 2RQPS.

### Queries and graph

The queries are available in the file queries.txt of this repository.

The RDF graph used can be downloaded from here: [Wikidata (about 1000M triples)](http://compact-leapfrog.tk/files/wikidata-enumerated.dat.gz).

### Instructions for compiling

To run our code, please install the library SDSL from [this repository](https://github.com/simongog/sdsl-lite).

1. Clone this repository and execute:

```Bash
cd navGraph
mkdir build
cd build
cmake ..
make
```

This will create two executable files: build-index and query-index.

2. To build the index run:
```Bash
./build-index <path-to-wikidata-file> 
```
This will create the index on the same directory were the wikidata file is. Keep all these files in the same directory.

4. Move files data/wikidata-enumerated.dat.P and data/wikidata-enumerated.dat.SO to the directory were the index is stored. Please keep these file names, or change them acordingly, keeping the same prefix for all of them.   

5. To run queries, do as follows:
```Bash
./query-index <path-to-index-file> queries.txt 
```


 