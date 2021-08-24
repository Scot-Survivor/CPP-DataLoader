#include "DataLoader.hpp"

void LoadDataThread(uint64_t SamplesToRead, uint64_t FileHeaderSectionLength, size_t ThreadId, std::string FileName, int* SamplesArray, BIN::SampleHeaderData* SamplesMetadata, int startToken, int endToken, int sampleLength, int paddingValue) {
	std::ifstream File(FileName, std::ios::binary);
	uint64_t SampleWritePos;
	uint64_t NumberOfSamplesRead = 0;
	size_t BytesToRead;
	size_t PositionOfEndTokenWrite;
	int* SampleFromFileDataBuffer_int32 = (int*)malloc(sizeof(int) * sampleLength);
	uint16_t* SampleFromFileDataBuffer_int16 = (uint16_t*)malloc(sizeof(uint16_t) * sampleLength);

	for (size_t i = ThreadId * SamplesToRead; i < (ThreadId * SamplesToRead) + SamplesToRead; i++) {
		// setup some values.
		SampleWritePos = (ThreadId * SamplesToRead * sampleLength) + (sampleLength * NumberOfSamplesRead); // Thread offset in the mallocd array then the sample offset.
			
		for (size_t j = 0; j < sampleLength; j++) {
			SampleFromFileDataBuffer_int32[j] = paddingValue;
		}

		if (SamplesMetadata[i].dtypeint16 == 0) {

			// Seek to position of the sample in the file.
			File.seekg(FileHeaderSectionLength + SamplesMetadata[i].OffsetFromDataSectionStart + 8);

			// Set bytes to read then do a check to ensure full loading of the sequence.
			BytesToRead = sizeof(int) * sampleLength - (sizeof(int) * 2);
			if (SamplesMetadata[i].SampleLength < BytesToRead) {
				BytesToRead = SamplesMetadata[i].SampleLength;
			}
			File.read((char*)&SampleFromFileDataBuffer_int32[1], BytesToRead);
			PositionOfEndTokenWrite = (BytesToRead / 4) + 1;
		}

		if (SamplesMetadata[i].dtypeint16 == 1) {

			// reset the malloced array to 0 values for padding.
			for (size_t j = 0; j < sampleLength; j++) {
				SampleFromFileDataBuffer_int16[j] = paddingValue;
			}

			// Seek to the position in the file of the sample.
			File.seekg(FileHeaderSectionLength + SamplesMetadata[i].OffsetFromDataSectionStart + 8);

			// set bytes to read then do a check to ensure full loading of the sequence.
			BytesToRead = sizeof(uint16_t) * sampleLength - (sizeof(uint16_t) * 2);
			if (SamplesMetadata[i].SampleLength < BytesToRead) {
				BytesToRead = SamplesMetadata[i].SampleLength;
			}
			File.read((char*)&SampleFromFileDataBuffer_int16[1], BytesToRead);

			// Cast the 16 bit values to the 32 bit array.
			for (size_t j = 0; j < sampleLength; j++) {
				SampleFromFileDataBuffer_int32[j] = static_cast<int>(SampleFromFileDataBuffer_int16[j]);
			}
			PositionOfEndTokenWrite = (BytesToRead / 2) + 1;

		}

		// Apply padding & start + end token & trim / pad array.
		SampleFromFileDataBuffer_int32[0] =  startToken;
		SampleFromFileDataBuffer_int32[PositionOfEndTokenWrite] = endToken;


		//Copy the contents into the thread return memory buffer.
		memcpy(&SamplesArray[SampleWritePos], SampleFromFileDataBuffer_int32, sampleLength);
		NumberOfSamplesRead++;
	}
};

py::array_t<int> LoadTrainDataMT(int64_t samplesToRead, std::string dataPath, std::string tokenizerName, int startToken, int endToken, int sampleLength, int paddingValue) {
	// Time keeping variables.
	int64_t StartTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	int64_t EndTime;
	int64_t TimeTaken;
	// Samples & File Data Variables.
	std::string FileName = dataPath + tokenizerName;
	std::ifstream File;
	uint64_t NumberOfSamplesInFile;
	uint64_t FileHeaderSectionLength; // Length of the section not including the length of the uint64_t val that indicates length.
	uint64_t FileLength;
	BIN::SampleHeaderData* SamplesMetadata;
	// Progress tracking variables.
	uint64_t MaxSamples = samplesToRead;
	uint64_t ProgressReportInterval = MaxSamples / 100;
	// Multithreading setup variables.
	std::vector<std::thread> DataLoaderThreads;
	int* MultiThreadDataBuffer;
	uint32_t NumberOfThreadsAvalible;
	uint32_t NumberOfThreadsToUse;
	uint64_t SamplesToReadPerThread;
	uint64_t SamplesLaunched;

	// Open the file and query number of samples.
	File = std::ifstream(FileName, std::ios::binary | std::ios::ate);
	FileLength = File.tellg();
	File.seekg(0);
	File.read((char*)&FileHeaderSectionLength, sizeof(uint64_t));
	File.seekg(8);
	File.read((char*)&NumberOfSamplesInFile, sizeof(uint64_t));

	// Logic for handling a file that has less samples than user requested.
	if (samplesToRead > NumberOfSamplesInFile) {
		std::cout << "This File Does Not Contain Enough Samples, Loading " << NumberOfSamplesInFile << " Samples Instead." << std::endl;
		samplesToRead = NumberOfSamplesInFile;
		MaxSamples = samplesToRead;
		ProgressReportInterval = MaxSamples / 100;
	}
	else std::cout << "Loading " << samplesToRead << " Samples From " << FileName << std::endl;

	std::cout << "Loading File Header." << std::endl;

	// Loading the file header contents to a vector.
	SamplesMetadata = (BIN::SampleHeaderData*)malloc(sizeof(BIN::SampleHeaderData) *samplesToRead);
	File.seekg(16);
	File.read((char*)SamplesMetadata, sizeof(BIN::SampleHeaderData) * samplesToRead);
	std::cout << "File Header Loaded." << std::endl;
	std::cout << "Number Of Samples In File " << NumberOfSamplesInFile << std::endl;

	// Determine optimal number of threads to use.
	NumberOfThreadsAvalible = std::thread::hardware_concurrency() * 0.8;
	NumberOfThreadsToUse = NumberOfThreadsAvalible;

	// Will try to distrobute across all threads evenly.
	SamplesToReadPerThread = samplesToRead / NumberOfThreadsAvalible;
	std::cout << "All Threads Will Read " << SamplesToReadPerThread << " Samples." << std::endl;

	// Prepare The MultiThread data load buffer.
	// This is done as i am not sure how thread safe STL containers are so fuck it good ol malloc ;)
	MultiThreadDataBuffer = (int*)malloc(sizeof(int) * SamplesToReadPerThread * NumberOfThreadsToUse * sampleLength);

	// Close the file pointer to prevent the OS from getting confused.
	File.close();

	// Launch Threads.
	for (size_t i = 0; i < NumberOfThreadsToUse; i++) {
		DataLoaderThreads.push_back(std::thread(LoadDataThread, SamplesToReadPerThread, FileHeaderSectionLength, i, FileName, MultiThreadDataBuffer, SamplesMetadata, startToken, endToken, sampleLength, paddingValue));
	}

	// Wait for threads to finish.
	for (std::thread& th : DataLoaderThreads) {
		th.join();
	}

	// Clean up some heap allocated variables.
	free(SamplesMetadata);

	// Put the object inside a capsule to manage its data incase it goes out of scope in the python program and to return it as a numpy array without a copy!
	EndTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	TimeTaken = (EndTime - StartTime) / 1000000000;
	std::cout << "Time Taken: " << TimeTaken << " Seconds." << std::endl;
	py::capsule capsule = py::capsule(MultiThreadDataBuffer, [](void* MultiThreadedDataBuffer) { delete reinterpret_cast<std::vector<int>*>(MultiThreadedDataBuffer); });
	py::array_t<int> LoadedSamples(
		{ (int64_t)SamplesToReadPerThread * (int64_t)NumberOfThreadsToUse, (int64_t)sampleLength },
		MultiThreadDataBuffer,
		capsule);
	return LoadedSamples;

}