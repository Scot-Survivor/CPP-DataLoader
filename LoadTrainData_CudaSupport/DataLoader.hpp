#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <thread>
#include <stdio.h>
#include <cstdlib>
#include <stdexcept>
#include <chrono>
#include <mutex>
#include <tuple>
#include <vector>


#include "base64.hpp"

//PYBIND11_MAKE_OPAQUE(std::vector<int>);
// https://pybind11.readthedocs.io/en/stable/advanced/pycpp/numpy.html
// https://stackoverflow.com/questions/44659924/returning-numpy-arrays-via-pybind11
// For implimenting native numpy arrays to speed the fuck up outta this array shit (mainly for downstream speed).
namespace py = pybind11;

struct GPU_Accelerated_Line {
	int64_t Index;

};


// Legacy

std::vector<py::list> LoadTrainDataST_Legacy(int64_t samplesToRead, std::string dataPath, std::string tokenizerName, int startToken, int endToken, int sampleLength, int paddingValue);

//future

py::array_t<int> LoadTrainDataST(uint64_t samplesToRead, std::string dataPath, std::string tokenizerName, int startToken, int endToken, int sampleLength, int paddingValue);
py::array_t<int> LoadTrainDataMT(int64_t samplesToRead, std::string dataPath, std::string tokenizerName, int startToken, int endToken, int sampleLength, int paddingValue);

void SaveTrainDataST(std::vector<std::vector<int>> Data, std::string FileName);

void ConvertToBinFormat(int64_t samplesToRead, std::string fileToLoad, std::string fileToSave);


namespace BIN {
	struct SampleHeaderData {
		uint64_t alignas(uint64_t) OffsetFromDataSectionStart;
		uint16_t alignas(uint16_t) SampleLength;
		uint8_t alignas(uint8_t) dtypeint16;
	};
};