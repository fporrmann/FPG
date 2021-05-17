# FP-Growth with integrated Fitering for the use in SPADE #



## Installation ##
**Requirements**

	CMake >= 3.10.0
	g++ >= 7
	python3-dev

**Clone the repo**

    git clone https://github.com/fporrmann/FPG.git
	cd FPG/PythonModule

**Build the Module**

	mkdir build
	cd build
	cmake ..
	make

The Python module (Linux: ***fim.so***; Windows: ***fim.pyd***) can be found in the build and evaluation directory.

## Running the Tests ##
Build the Python module as described in the [installation](#installation) section.

**Requirements / Python-Modules**

	python3
	numpy
	json

**Execute a test using the desired test configuration**

	cd FPG/Evaluation
	python3 runTest.py <CONFIG>

**Test Configurations**

| Test # | Length     | Neurons | Config-File             |
| ------ | -------    | ------- | ----------------------- |
|      1 | 22.32s     | 150     | cfg/movement_PGHF.json  |
|      2 | 16min. 43s | 150     | cfg/test_long.json      |
|      3 | 5s         | 150     | cfg/test_short.json     |
|      4 | 22.32s     | 300     | cfg/test_300n.json      |
|      5 | 22.32s     | 450     | cfg/test_450n.json      |

## Data Acquisition ##
The electrophysiological data is imported via [GIN](https://gin.g-node.org/)
as a submodule. A GIN account is necessary to download the data.

**Requirements / Python-Modules**

```
# create the conda environment
cd DataAcquisition
conda env create -f environment.yml
conda activate fpg
```

**Create data for evaluation**

```
# run data download, preprocess and save workflow
./preprocess_data.sh
```
