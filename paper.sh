mkdir -p export
mkdir -p plot
mkdir -p build
rm -rf plot/*
rm -rf export/*

export sourceName="mainSolution"

# COMPILE
cd build
cmake ..
make
cd ..
echo ""

# GUI
declare -a gridType=()
declare -a interpScheme=()
declare -a problemsSolved=0
declare -a inputOptions
declare -a medium

gridType+=("staggered")
interpScheme+=("NA")
gridType+=("collocated")
interpScheme+=("CDS")
gridType+=("collocated")
interpScheme+=("I2DPIS")

numRuns=${#gridType[@]}

problemsSolved+=2
problemsSolved+=4

cd input
inputOptions=(*.txt)
inputOptions=("${inputOptions[@]%.*}")
cd ..

medium="gulfMexicoShale";

# SOLVE
echo "-- Solving benchmarking problems"
echo ""
for ((i=0; i<numRuns; i++));
do
	cd build
	./$sourceName ${gridType[$i]} ${interpScheme[$i]} ${medium} ${problemsSolved}
	echo ""
	cd ..
done

# PLOT
echo "-- Plotting results"
python3 -W ignore ./postpro/terzaghiPlotSolutionPaper.py ${medium}
python3 -W ignore ./postpro/mandelPlotSolutionPaper.py ${medium}
echo ""