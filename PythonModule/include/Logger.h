#pragma once

#include <sstream>
#include <iostream>

enum class  Verbosity
{
	VB_DEBUG = 0,
	VB_VERBOSE = 1,
	VB_INFO = 2,
	VB_WARNING = 3,
	VB_ERROR = 4
};

class Logger
{
	using EndlType = std::ostream& (std::ostream&);

public:
	Logger(Verbosity lvl, Verbosity verbosity = Verbosity::VB_VERBOSE) :
		m_lvl(lvl),
		m_verbosity(verbosity),
		m_outStream(std::cout)
	{}

	void SetVerbosity(Verbosity v)
	{
		m_verbosity = v;
	}

	Logger& operator<<(EndlType endl)
	{
		if (m_lvl >= m_verbosity)
			m_outStream << endl;
		return *this;
	}

	template<typename T>
	Logger& operator<<(const T& data)
	{
		if (m_lvl >= m_verbosity)
			m_outStream << data;
		return *this;
	}


private:
	Verbosity m_lvl;
	Verbosity m_verbosity;
	std::ostream& m_outStream;
};

static Logger g_debug(Verbosity::VB_DEBUG);
static Logger g_verbose(Verbosity::VB_VERBOSE);
static Logger g_info(Verbosity::VB_INFO);
static Logger g_warning(Verbosity::VB_WARNING);
static Logger g_error(Verbosity::VB_ERROR);

#define LOG_DEBUG g_debug
#define LOG_VERBOSE g_verbose
#define LOG_INFO g_info
#define LOG_WARNING g_warning
#define LOG_ERROR g_error

void SetVerbosity(Verbosity v)
{
	g_debug.SetVerbosity(v);
	g_verbose.SetVerbosity(v);
	g_info.SetVerbosity(v);
	g_warning.SetVerbosity(v);
	g_error.SetVerbosity(v);
}

template <typename E>
constexpr typename std::underlying_type<E>::type ToUnderlying(E e) noexcept
{
	return static_cast<typename std::underlying_type<E>::type>(e);
}

Verbosity ToVerbosity(const int32_t& val)
{
	if (val < ToUnderlying(Verbosity::VB_DEBUG) || val > ToUnderlying(Verbosity::VB_ERROR))
		return Verbosity::VB_INFO;

	return static_cast<Verbosity>(val);
}