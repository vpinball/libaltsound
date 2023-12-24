// ---------------------------------------------------------------------------
// altsound_file_parser.cpp
//
// Parser for Legacy format sample files and directories
// ---------------------------------------------------------------------------
// license:BSD-3-Clause
// copyright-holders:Carsten W�chter, Dave Roscoe
// ---------------------------------------------------------------------------

#include "altsound_file_parser.hpp"
#include "altsound_logger.hpp"

#include <iomanip>
#include <sys/stat.h>
#ifdef _MSC_VER

#ifdef __cplusplus
extern "C" {
#endif

#include "dirent/dirent.h"

#ifdef __cplusplus
}
#endif


#else
#include <dirent.h>
#endif

extern AltsoundLogger alog;

// ---------------------------------------------------------------------------
// CTOR/DTOR
// ---------------------------------------------------------------------------

AltsoundFileParser::AltsoundFileParser(const string& altsound_path_in)
	: altsound_path(altsound_path_in)
{
	if (!altsound_path_in.empty() && altsound_path_in.back() != '/')
		altsound_path += '/';
}

// ---------------------------------------------------------------------------

// DAR@20230525
// This support is for PinSound style of alternate sound mixes.
// The files are split into directories according to the following:
//
// <ROM shortname>/
//    music/
//        <instruction1>-name/
//            <soundfile1>.wav(or .ogg)
//            ...
//        <instruction2>-name/
//        ...
//    jingle/
//        <instruction1>-name/
//            <soundfile1>.wav(or .ogg)
//            ...
//        <instruction2>-name/
//        ...
//    sfx/
//        <instruction1>-name/
//            <soundfile1>.wav(or .ogg)
//            ...
//        <instruction2>-name/
//        ...
//    single/
//        <instruction1>-name/
//            <soundfile1>.wav(or .ogg)
//            ...
//        <instruction2>-name/
//        ...
//    voice/
//        <instruction1>-name/
//            <soundfile1>.wav(or .ogg)
//            ...
//        <instruction2>-name/
//        ...
bool AltsoundFileParser::parse(std::vector<AltsoundSampleInfo>& samples_out)
{
	ALT_DEBUG(0, "BEGIN AltsoundFileParser::parse()");
	ALT_INDENT;

	const string path_jingle = string("jingle") + '/';
	const string path_music = string("music") + '/';
	const string path_sfx = string("sfx") + '/';
	const string path_single = string("single") + '/';
	const string path_voice = string("voice") + '/';

	for (int i = 0; i < 5; ++i) {
		float default_gain = .1f;
		float default_ducking = 1.f; //!! default depends on type??

		const string subpath = (i == 0) ? path_jingle :
			((i == 1) ? path_music :
			((i == 2) ? path_sfx :
			((i == 3) ? path_single : path_voice)));

		// Set default ducking values.  Can be overridden by ducking.txt file
		if (subpath == path_jingle || subpath == path_single) {
			default_ducking = .1f; // % music volume retained when active
		}
		else if (subpath == path_sfx) {
			default_ducking = .8f;
		}
		else if (subpath == path_voice) {
			default_ducking = .65f;
		}

		DIR *dir;
		struct dirent *entry;

		string PATH = altsound_path + subpath;
		ALT_INFO(0, "Current_path1: %s", PATH.c_str());

		// Check for new default gain
		{
		string PATHG = PATH + "gain.txt";
		float parsedGain = parseFileValue(PATHG);
		if (parsedGain != -1.0f) {
			default_gain = parsedGain;
		}

		// Check for new default ducking
		PATHG = PATH + "ducking.txt";
		float parsedDucking = parseFileValue(PATHG);
		if (parsedDucking != -1.0f) {
			default_ducking = parsedDucking;
		}
		}

		dir = opendir(PATH.c_str());
		if (!dir)
		{
			// Path not found.. try the others
			ALT_INFO(0, "Path not found: %s", PATH.c_str());
			continue;
		}

		// parse individual sound file data
		entry = readdir(dir);
		while (entry != nullptr) {
			if (entry->d_name[0] != '.'
				&& strstr(entry->d_name, ".txt") == nullptr
				&& strstr(entry->d_name, ".ini") == nullptr)
			{
				// Not a system file or txt file.  Assume it's a directory
				// (per PinSound format requirements).  Backup the current
				// directory stream and entry
				DIR* backup_dir = dir;
				struct dirent backup_entry = *entry;

				float gain = default_gain;
				float ducking = default_ducking;

				DIR *dir2;
				struct dirent *entry2;

				string PATH2 = PATH + entry->d_name;

				// Check for overriding gain value
				string PATHG = PATH2 + "gain.txt";
				float parsedGain = parseFileValue(PATHG);
				if (parsedGain != -1.0f) {
					gain = parsedGain;
				}

				// check for overriding ducking value
				PATHG = PATH2 + "ducking.txt";
				float parsedDucking = parseFileValue(PATHG);
				if (parsedDucking != -1.0f) {
					ducking = parsedDucking;
				}

				dir2 = opendir(PATH2.c_str());
				ALT_INFO(1, "opendir(%s)", PATH2.c_str());

				entry2 = readdir(dir2);
				while (entry2 != nullptr) {
					if (entry2->d_name[0] != '.'
						&& strstr(entry2->d_name, ".txt") == nullptr
						&& strstr(entry2->d_name, ".ini") == nullptr)
					{
						//LOG(("CURRENT_ENTRY2: %s\n", entry->d_name));
						const char* ptr = strrchr(PATH2.c_str(), '/');
						char id[7] = { 0, 0, 0, 0, 0, 0, 0 };

						AltsoundSampleInfo sample;

						sample.fname = PATH2 + entry2->d_name;

						memcpy(id, ptr + 1, 6);
						sample.id = std::stoul(trim(id), nullptr);

						// DAR@20230828
						// Original code divided the gain by 20 before storing.
						// That equates to multiplying the fractional gain below
						// by 5.  This often results in gain values greater than
						// 1.0f which indicates an amplified sound.  If the gain
						// being read in is 100% (1.0f) then this value will end up
						// being 5.0f which seems awfully high.  However, to
						// replicate original behavior, it is being preserved below
						sample.gain = gain * 5.0f;
						sample.ducking = ducking;

						if (subpath == path_music) {
							sample.channel = 0;
							sample.loop = true;
							sample.stop = false;
						}

						if (subpath == path_jingle || subpath == path_single) {
							sample.channel = 1;
							sample.loop = false;
							sample.stop = (subpath == path_single) ? true : false;
						}

						if (subpath == path_sfx || subpath == path_voice) {
							sample.channel = -1;
							sample.loop = false;
							sample.stop = false;
						}

						samples_out.push_back(sample);

						std::ostringstream debug_stream;
						debug_stream << "ID = 0x" << std::setfill('0') << std::setw(4) << std::hex << sample.id << std::dec
							<< ", CHANNEL = " << sample.channel
							<< ", DUCKING = " << std::fixed << std::setprecision(2) << sample.ducking
							<< ", GAIN = " << std::fixed << std::setprecision(2) << sample.gain
							<< ", LOOP = " << sample.loop
							<< ", FNAME = " << sample.fname;

						ALT_DEBUG(0, debug_stream.str().c_str());
					}
					entry2 = readdir(dir2);
				}
				closedir(dir2);

				dir = backup_dir;
				*entry = backup_entry;
			}
			entry = readdir(dir);
		}
		closedir(dir);
	}
	ALT_INFO(0, "Found %d samples", samples_out.size());

	ALT_OUTDENT;
	ALT_DEBUG(0, "BEGIN AltsoundFileParser::parse()");
	return true;
}

// ----------------------------------------------------------------------------

float AltsoundFileParser::parseFileValue(const string& filePath)
{
	FILE *f = fopen(filePath.c_str(), "r");
	if (!f) {
		// file not found
		return -1.0f;
	}

	int tmpValue = 0;
	if (fscanf(f, "%d", &tmpValue) != 1) {
		// failed to read an integer from file
		fclose(f);
		return -1.0f;
	}

	if (fclose(f) != 0) {
		// failed to close file
		return -1.0f;
	}

	return tmpValue >= 100 ? 1.0f : tmpValue < 0.0f ? 0.0f : (float)tmpValue / 100.f;
}
