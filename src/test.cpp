// ---------------------------------------------------------------------------
// test.cpp
// 07/23/23 - Dave Roscoe
//
// Standalone executive for AltSound for use by devs and authors.  This 
// executable links in all AltSound format processing, ingests sound
// commands from a file, and plays them through the same libraries used for
// VPinMAME.  The command file can be generated from live gameplay or 
// created by hand, to test scripted sound playback scenarios.  Authors can
// use this to test mix levels of one or more sounds in any combination to
// finalize the AltSound mix for a table.  This is particularly useful when
// testing modes.  Authors can script the specific sequences by hand, or
// capture the data from live gameplay.  Then the file can be edited to
// include only what is needed.  From there, the author can iterate on the
// specific sounds-under-test, without having to create it repeatedly on the
// table.
//
// Devs can use this tool to isolate problems and run it through a
// debugger as many times as need to find and fix a problem.  If a user finds
// a problem, all they need to do is:
// 1. enable sound command recording
// 2. set logging level to DEBUG
// 3. recreate the problem
// 4. send the problem description, along with the altsound.log and cmdlog.txt
// ---------------------------------------------------------------------------
// license:<TODO>
// ---------------------------------------------------------------------------

#include "altsound.h"

#include <thread>
#include <vector>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>

using std::string;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

// Structure to hold a sound command and its associated timing
struct TestData {
	unsigned int msec;
	uint32_t snd_cmd;
};

struct InitData {
	string log_path;
	std::vector<TestData> test_data;
	string vpm_path;
	string altsound_path;
	string game_name;
	ALTSOUND_HARDWARE_GEN hardware_gen;
};

// ----------------------------------------------------------------------------

string extractValue(const string& line) {
	size_t colonPos = line.find(':');
	if (colonPos == string::npos)
		throw std::runtime_error("Value could not be determined");
	size_t valueStart = colonPos + 1;
	while (valueStart < line.size() && line[valueStart] == ' ')
		++valueStart;

	string value = line.substr(valueStart);
	if (!value.empty() && value.back() == '\r')
		value.pop_back();
	return value;
}

bool playbackCommands(const std::vector<TestData>& test_data)
{
	for (size_t i = 0; i < test_data.size(); ++i) {
		const TestData& td = test_data[i];
		if (!AltsoundProcessCommand(td.snd_cmd, 0))
			throw std::runtime_error("Command playback failed");

		// Sleep for the duration specified in msec for each command, except for the last command.
		if (i < test_data.size() - 1)
			std::this_thread::sleep_for(std::chrono::milliseconds(td.msec));
		else {
			// Sleep for 5 seconds before exiting
			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		}
	}
	return true;
}

// ----------------------------------------------------------------------------
// Command file parser
// ----------------------------------------------------------------------------

bool parseCmdFile(InitData& init_data)
{
	std::cout << "BEGIN parseCmdFile" << std::endl;

	try {
		std::ifstream inFile(init_data.log_path);
		if (!inFile.is_open())
			throw std::runtime_error("Unable to open file: " + init_data.log_path);

		string line;

		// Process paths and game name
		if (!std::getline(inFile, line))
			throw std::runtime_error("altsound_path value could not be determined");

		string altsoundPath = extractValue(line);
	 	std::replace(altsoundPath.begin(), altsoundPath.end(), '\\', '/');
		if (altsoundPath.back() != '/')
			altsoundPath += '/';

		size_t altsoundPos = altsoundPath.find("/altsound/");
		if (altsoundPos == string::npos)
			throw std::runtime_error("altsound_path value could not be determined");

		init_data.altsound_path = altsoundPath;
		init_data.vpm_path = altsoundPath.substr(0, altsoundPos + 1);

		size_t nextSlashPos = altsoundPath.find('/', altsoundPos + 10);
		if (nextSlashPos == string::npos)
			throw std::runtime_error("game name could not be determined");

		init_data.game_name = altsoundPath.substr(altsoundPos + 10, nextSlashPos - (altsoundPos + 10));

		// Process hardware_gen
		if (!std::getline(inFile, line))
			throw std::runtime_error("hardware_gen value could not be determined");

		std::string hexString = extractValue(line);
		init_data.hardware_gen = (ALTSOUND_HARDWARE_GEN)std::stoull(hexString, nullptr, 16);

		std::cout << "Altsound path: " << init_data.altsound_path << std::endl;
		std::cout << "VPinMAME path: " << init_data.vpm_path << std::endl;
		std::cout << "Game name: " << init_data.game_name << std::endl;
		std::cout << "Hardware Gen: 0x" 
			<< std::setfill('0') << std::setw(13) 
			<< std::hex << init_data.hardware_gen << std::endl;

		// The rest of the lines are test data
		while (std::getline(inFile, line)) {
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			if (line.empty())
				continue;

			std::istringstream ss(line);

			TestData data;
			string temp, command;

			if (!std::getline(ss, temp, ','))
				continue;

			char* end;
			data.msec = std::strtoul(temp.c_str(), &end, 10);
			if (end == temp.c_str())
				throw std::runtime_error("Unable to parse time: " + temp);

			const string HEX_PREFIX = "0x";
			ss >> std::ws;
			if (!std::getline(ss, command, ',')) continue;
			if (command.substr(0, HEX_PREFIX.length()) == HEX_PREFIX)
				command = command.substr(HEX_PREFIX.length());
			else
				throw std::runtime_error("Command value is not in hexadecimal format: " + command);

			data.snd_cmd = std::strtoul(command.c_str(), &end, 16);
			if (end == command.c_str())
				throw std::runtime_error("Unable to parse command: " + command);

			init_data.test_data.push_back(data);
		}

		inFile.close();
		std::cout << "END parseCmdFile" << std::endl;
		return true;
	}
	catch (const std::runtime_error& e) {
		std::cout << e.what() << std::endl;
		std::cout << "END parseCmdFile" << std::endl;
		return false;
	}
}

// ---------------------------------------------------------------------------

std::pair<bool, InitData> init(const string& log_path)
{
	std::cout << "BEGIN init()" << std::endl;

	InitData init_data;
	init_data.log_path = log_path;

	try {
		if (!parseCmdFile(init_data))
			throw std::runtime_error("Failed to parse command file.");

		std::cout << "SUCCESS parseCmdFile()" << std::endl;
		std::cout << "Num commands parsed: " << init_data.test_data.size() << std::endl;

		AltsoundInit(init_data.vpm_path, init_data.game_name);
		AltsoundSetHardwareGen(init_data.hardware_gen);

		std::cout << "END init()" << std::endl;
		return std::make_pair(true, init_data);
	}
	catch (const std::runtime_error& e) {
		std::cout << e.what() << std::endl;
		std::cout << "END init()" << std::endl;
		return std::make_pair(false, InitData{});
	}
}

// ---------------------------------------------------------------------------
// Functional code
// ---------------------------------------------------------------------------

int main(int argc, const char* argv[]) {
	if (argc < 2) {
		std::cout << "Usage: " << argv[0] << " <gamename>-cmdlog.txt path" << std::endl;
		std::cout << "Where <gamename>-cmdlog.txt path is the full path and "
				<< "filename of recording file" << std::endl;
		return 1;
	}

	AltsoundSetLogger("./", ALTSOUND_LOG_LEVEL_DEBUG, true);

	const auto init_result = init(argv[1]);

	if (!init_result.first) {
		std::cout << "Initialization failed." << std::endl;
		return 1;
	}

	std::cout << "Press Enter to begin playback..." << std::endl;
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Wait for user input

	try {
		std::cout << "Starting playback for \"" << init_result.second.altsound_path << "\"..." << std::endl;
		if (!playbackCommands(init_result.second.test_data)) {
			std::cout << "Playback failed" << std::endl;
			return 1;
		}
		std::cout << "Playback finished for \"" << init_result.second.altsound_path << "\"..." << std::endl;
	}
	catch (const std::exception& e) {
		std::cout << "Unexpected error during playback:" << e.what()  << std::endl;
		return 1;
	}

	std::cout << "Playback completed! Press Enter to exit..." << std::endl;
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Wait for user input

	AltsoundShutdown();
	return 0;
}