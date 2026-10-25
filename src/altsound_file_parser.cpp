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

#include <filesystem>
#include <fstream>
#include <iomanip>

namespace fs = std::filesystem;

extern AltsoundLogger alog;

namespace {

struct LegacyCategory {
	const char* name;
	float ducking;
	int channel;
	bool loop;
	bool stop;
};

constexpr LegacyCategory legacy_categories[] = {
	{ "jingle", .1f, 1, false, false },
	{ "music", 1.f, 0, true, false },
	{ "sfx", .8f, -1, false, false },
	{ "single", .1f, 1, false, true },
	{ "voice", .65f, -1, false, false },
};

bool isSampleEntry(const string& name)
{
	return !name.empty() && name[0] != '.'
		&& name.find(".txt") == string::npos
		&& name.find(".ini") == string::npos;
}

}

// ---------------------------------------------------------------------------
// CTOR/DTOR
// ---------------------------------------------------------------------------

AltsoundFileParser::AltsoundFileParser(const fs::path& altsound_path_in)
	: altsound_path(altsound_path_in)
{
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
//    sfx/
//    single/
//    voice/
bool AltsoundFileParser::parse(std::vector<AltsoundSampleInfo>& samples_out)
{
	ALT_DEBUG(0, "BEGIN AltsoundFileParser::parse()");
	ALT_INDENT;

	std::error_code ec;

	for (const LegacyCategory& category : legacy_categories) {
		const fs::path category_path = altsound_path / category.name;

		if (!fs::is_directory(category_path, ec)) {
			ALT_INFO(0, "Path not found: %s", category_path.string().c_str());
			continue;
		}
		ALT_INFO(0, "Current_path1: %s", category_path.string().c_str());

		const float default_gain = parseFileValue(category_path / "gain.txt", .1f);
		const float default_ducking = parseFileValue(category_path / "ducking.txt", category.ducking);

		for (const fs::directory_entry& sample_dir : fs::directory_iterator(category_path, ec)) {
			const string dir_name = sample_dir.path().filename().string();
			if (!sample_dir.is_directory(ec) || !isSampleEntry(dir_name))
				continue;

			ALT_INFO(1, "Sample directory: %s", sample_dir.path().string().c_str());

			unsigned int id;
			try {
				id = static_cast<unsigned int>(std::stoul(trim(dir_name.substr(0, 6))));
			}
			catch (const std::exception&) {
				ALT_ERROR(1, "Unable to parse sample ID from directory name: %s", dir_name.c_str());
				continue;
			}

			const float gain = parseFileValue(sample_dir.path() / "gain.txt", default_gain);
			const float ducking = parseFileValue(sample_dir.path() / "ducking.txt", default_ducking);

			for (const fs::directory_entry& file : fs::directory_iterator(sample_dir.path(), ec)) {
				if (!file.is_regular_file(ec) || !isSampleEntry(file.path().filename().string()))
					continue;

				AltsoundSampleInfo sample;
				sample.fname = file.path().generic_string();
				sample.id = id;

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
				sample.channel = category.channel;
				sample.loop = category.loop;
				sample.stop = category.stop;

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
		}
	}
	ALT_INFO(0, "Found %zu samples", samples_out.size());

	ALT_OUTDENT;
	ALT_DEBUG(0, "END AltsoundFileParser::parse()");
	return true;
}

// ----------------------------------------------------------------------------

float AltsoundFileParser::parseFileValue(const fs::path& file_path, float default_value)
{
	std::ifstream file(file_path);
	int value = 0;
	if (!(file >> value))
		return default_value;

	return value >= 100 ? 1.0f : value <= 0 ? 0.0f : (float)value / 100.f;
}
