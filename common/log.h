#ifndef LOG_H
#define LOG_H

#define LOG_DEBUG(msg)                                                         \
	do {                                                                   \
		std::ostringstream _log_ss;                                    \
		_log_ss << msg;                                                \
		std::cout << "\033[33m[DEBUG] "                                \
			  << "[" << __LINE__ << "] " << __FUNCTION__           \
			  << "(): " << _log_ss.str() << "\033[0m"              \
			  << std::endl;                                        \
	} while (0)

#define LOG_INFO(msg)                                                          \
	do {                                                                   \
		std::ostringstream _log_ss;                                    \
		_log_ss << msg;                                                \
		std::cout << "[INFO] "                                         \
			  << "[" << std::dec << __LINE__ << "] "               \
			  << __FUNCTION__ << "(): " << _log_ss.str()           \
			  << "\033[0m" << std::endl;                           \
	} while (0)

#define LOG_ERROR(msg)                                                         \
	do {                                                                   \
		std::ostringstream _log_ss;                                    \
		_log_ss << msg;                                                \
		std::cerr << "\033[31m[ERROR] "                                \
			  << "[" << __LINE__ << "] " << __FUNCTION__           \
			  << "(): " << _log_ss.str() << "\033[0m"              \
			  << std::endl;                                        \
	} while (0)

#endif
