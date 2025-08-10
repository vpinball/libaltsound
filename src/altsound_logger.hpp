// ---------------------------------------------------------------------------
// altsound_logger.hpp
//
// Runtime and debug logger for AltSound
// ---------------------------------------------------------------------------
// license:BSD-3-Clause
// copyright-holders: Dave Roscoe
// ---------------------------------------------------------------------------

#ifndef ALTSOUND_LOGGER_H
#define ALTSOUND_LOGGER_H
#if !defined(__GNUC__) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || (__GNUC__ >= 4)	// GCC supports "pragma once" correctly since 3.4
#pragma once
#endif

#if _MSC_VER >= 1700
 #ifdef inline
  #undef inline
 #endif
#endif

#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdarg>
#include <cstdlib>
#include <algorithm>

using std::string;

// convenience macros
#define ALT_INFO(indent, msg, ...) alog.info(indent, msg, ##__VA_ARGS__)
#define ALT_ERROR(indent, msg, ...) alog.error(indent, msg, ##__VA_ARGS__)
#define ALT_WARNING(indent, msg, ...) alog.warning(indent, msg, ##__VA_ARGS__)
#define ALT_DEBUG(indent, msg, ...) alog.debug(indent, msg, ##__VA_ARGS__)
//#define ALT_INDENT alog.indent()
#define ALT_INDENT
//#define ALT_OUTDENT alog.outdent()
#define ALT_OUTDENT
#define ALT_CALL(func) ([&]() { alog.indent(); auto ret = func; alog.outdent(); return ret; }())
#define ALT_RETURN(retval) do { alog.outdent(); return retval; } while (0)

class AltsoundLogger
{
public:
	enum Level {
		None = 0,
		Info,
		Error,
		Warning,
		Debug,
		UNDEFINED
	};

	explicit AltsoundLogger();
	explicit AltsoundLogger(const string& filename);

	~AltsoundLogger() = default;

	// DAR@20230706
	// Because these are variadic template functions, their definitions must
	// remain in the header
	//
	// Log INFO level messages
	void info(int rel_indent, const char* format, ...)
	{
		if (log_level >= Level::Info) {
			va_list args;
			va_start(args, format);
			log(base_indent + rel_indent, Level::Info, format, args);
			va_end(args);
		}
	}

	// Log ERROR level messages
	void error(int rel_indent, const char* format, ...)
	{
		if (log_level >= Level::Error) {
			va_list args;
			va_start(args, format);
			log(base_indent + rel_indent, Level::Error, format, args);
			va_end(args);
		}
	}

	// Log WARNING level messages
	void warning(int rel_indent, const char* format, ...)
	{
		if (log_level >= Level::Warning) {
			va_list args;
			va_start(args, format);
			log(base_indent + rel_indent, Level::Warning, format, args);
			va_end(args);
		}
	}

	// Log DEBUG level messages
	void debug(int rel_indent, const char* format, ...)
	{
		if (log_level >= Level::Debug) {
			va_list args;
			va_start(args, format);
			log(base_indent + rel_indent, Level::Debug, format, args);
			va_end(args);
		}
	}

	void setLogPath(const string& path);
	void setLogLevel(Level level);
	void enableConsole(const bool enable);

	// increase base indent
	static void indent();

	// decrease base indent
	static void outdent();

	// convert string to Level enum value
	Level toLogLevel(const string& lvl_in);

private:  // methods

	// DAR@20230706
	// Because this is a variadic template function, it must remain in the header
	//
	// main logging method
	void log(int indentLevel, Level lvl, const char* format, va_list args) {
		va_list args_copy;
		va_copy(args_copy, args);
		int size = vsnprintf(nullptr, 0, format, args_copy);
		va_end(args_copy);

		if (size >= 0) {
			char* buffer = nullptr;
			if (size > 0) {
				buffer = static_cast<char*>(malloc(size + 1));
				vsnprintf(buffer, size + 1, format, args);
			}

			std::stringstream message;
			message << string(indentLevel * indentWidth, ' ')
				<< toString(lvl) << ": " << (buffer ? buffer : "") << "\n";
			const string finalMessage = message.str();

			if (out.is_open()) {
				out << finalMessage;
				out.flush();
			}

			if (console)
				std::cout << finalMessage;

			if (buffer)
				free(buffer);
		}
	}

	// DAR@20230706
	// This is only used for logging message within the logger class
	//
	void none(int rel_indent, const char* format, ...)
	{
		if (log_level >= Level::None) {
			va_list args;
			va_start(args, format);
			log(base_indent + rel_indent, Level::None, format, args);
			va_end(args);
		}
	}

	// convert Level enum value to a string
	const char* toString(Level lvl);

private: // data

	// Thread-local storage for the base indentation level
	static thread_local int base_indent;
	Level log_level = None;
	bool console = false;
	static constexpr int indentWidth = 4;
	std::ofstream out;
};

// ----------------------------------------------------------------------------
// Inline methods
// ----------------------------------------------------------------------------

inline void AltsoundLogger::setLogPath(const string& logPath)
{
	if (out.is_open())
		out.close();

	if (logPath.empty())
		return;

	string full_path = logPath;
	std::replace(full_path.begin(), full_path.end(), '\\', '/');

	if (full_path.back() != '/')
		full_path += '/';

	full_path += "altsound.log";

	out.open(full_path.c_str());

	none(0, "path set: %s", full_path.c_str());
}

inline void AltsoundLogger::setLogLevel(Level level)
{
	log_level = level;
	none(0, "New log level set: %s", toString(log_level));
}

inline void AltsoundLogger::enableConsole(const bool enable)
{
	console = enable;
}

// ----------------------------------------------------------------------------

inline void AltsoundLogger::indent()
{
	++base_indent;
}

// ----------------------------------------------------------------------------

inline void AltsoundLogger::outdent()
{
	if (base_indent > 0) {
		--base_indent;
	}
}

#endif //ALTSOUND_LOGGER_H
