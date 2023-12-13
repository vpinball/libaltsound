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
#include "altsound_logger.hpp"

#include <thread>
#include <vector>
#include <algorithm>
#include <chrono>

using std::string;

extern AltsoundLogger alog;

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
		if (!ALT_CALL(AltsoundProcessCommand(td.snd_cmd, 0)))
			ALT_WARNING(0, "Command playback failed");

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
	ALT_DEBUG(0, "BEGIN parseCmdFile");

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

		ALT_INFO(1, "Altsound path: %s", init_data.altsound_path.c_str());
		ALT_INFO(1, "VPinMAME path: %s", init_data.vpm_path.c_str());
		ALT_INFO(1, "Game name: %s", init_data.game_name.c_str());
		ALT_INFO(1, "Hardware Gen: 0x%013x", (uint64_t)init_data.hardware_gen);

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
		ALT_DEBUG(0, "END parseCmdFile");
		return true;
	}
	catch (const std::runtime_error& e) {
		ALT_ERROR(0, e.what());
		ALT_DEBUG(0, "END parseCmdFile");
		return false;
	}
}

// ---------------------------------------------------------------------------

std::pair<bool, InitData> init(const string& log_path)
{
	ALT_DEBUG(0, "BEGIN init()");

	InitData init_data;
	init_data.log_path = log_path;

	try {
		if (!ALT_CALL(parseCmdFile(init_data)))
			throw std::runtime_error("Failed to parse command file.");

		ALT_INFO(1, "SUCCESS parseCmdFile()");
		ALT_INFO(1, "Num commands parsed: %d", init_data.test_data.size());

		AltsoundInit(init_data.vpm_path, init_data.game_name);
		AltsoundSetHardwareGen(init_data.hardware_gen);

		ALT_DEBUG(0, "END init()");
		return std::make_pair(true, init_data);
	}
	catch (const std::runtime_error& e) {
		ALT_ERROR(0, e.what());
		ALT_DEBUG(0, "END init()");
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

	alog.setLogPath("./");
	alog.setLogLevel(AltsoundLogger::Level::Debug);
	alog.enableConsole(true);

	auto init_result = init(argv[1]);

	if (!init_result.first) {
		ALT_ERROR(0, "Initialization failed.");
		return 1;
	}

	std::cout << "Press Enter to begin playback..." << std::endl;
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Wait for user input

	try {
		ALT_INFO(1, "Starting playback for \"%s\"...", init_result.second.altsound_path.c_str());
		if (!playbackCommands(init_result.second.test_data)) {
			ALT_ERROR(0, "Playback failed");
			return 1;
		}
		ALT_INFO(1, "Playback finished for \"%s\"...", init_result.second.altsound_path.c_str());
	}
	catch (const std::exception& e) {
		ALT_ERROR(0, "Unexpected error during playback: %s", e.what());
		return 1;
	}

	std::cout << "Playback completed! Press Enter to exit..." << std::endl;
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Wait for user input

	AltsoundShutdown();
	return 0;
}