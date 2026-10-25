// ---------------------------------------------------------------------------
// altsound_file_parser.hpp
//
// Parser for Legacy format sample files and directories
// ---------------------------------------------------------------------------
// license:BSD-3-Clause
// copyright-holders:Carsten W�chter, Dave Roscoe
// ---------------------------------------------------------------------------

#ifndef ALTSOUND_FILE_PARSER_HPP
#define ALTSOUND_FILE_PARSER_HPP
#if !defined(__GNUC__) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || (__GNUC__ >= 4)	// GCC supports "pragma once" correctly since 3.4
#pragma once
#endif

#include "altsound_data.hpp"

using std::string;

class AltsoundFileParser {
public:

	// Standard constructor
	explicit AltsoundFileParser(const std::filesystem::path& altsound_path_in);

	bool parse(std::vector<AltsoundSampleInfo>& samples_out);

protected:

	// Default constructor
	AltsoundFileParser() {}

private: // functions

	float parseFileValue(const std::filesystem::path& file_path, float default_value);

private: // data
	std::filesystem::path altsound_path;
};

#endif //ALTSOUND_FILE_PARSER_HPP
