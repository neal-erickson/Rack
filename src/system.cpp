#include <system.hpp>
#include <string.hpp>

#include <thread>
#include <regex>
#include <chrono>
#include <dirent.h>
#include <sys/stat.h>
#include <cxxabi.h> // for __cxxabiv1::__cxa_demangle

#if defined ARCH_LIN || defined ARCH_MAC
	#include <pthread.h>
	#include <sched.h>
	#include <execinfo.h> // for backtrace and backtrace_symbols
	#include <unistd.h> // for execl
	#include <sys/utsname.h>
#endif

#if defined ARCH_MAC
	#include <mach/mach_init.h>
	#include <mach/thread_act.h>
#endif

#if defined ARCH_WIN
	#include <windows.h>
	#include <shellapi.h>
	#include <processthreadsapi.h>
	#include <dbghelp.h>
#endif

#define ZIP_STATIC
#include <zip.h>


namespace rack {
namespace system {


std::list<std::string> getEntries(const std::string& path) {
	std::list<std::string> filenames;
	DIR* dir = opendir(path.c_str());
	if (dir) {
		struct dirent* d;
		while ((d = readdir(dir))) {
			std::string filename = d->d_name;
			if (filename == "." || filename == "..")
				continue;
			filenames.push_back(path + "/" + filename);
		}
		closedir(dir);
	}
	filenames.sort();
	return filenames;
}


std::list<std::string> getEntriesRecursive(const std::string &path, int depth) {
	std::list<std::string> entries = getEntries(path);
	if (depth > 0) {
		// Don't iterate using iterators because the list will be growing.
		size_t limit = entries.size();
		auto it = entries.begin();
		for (size_t i = 0; i < limit; i++) {
			const std::string &entry = *it++;
			if (isDirectory(entry)) {
				std::list<std::string> subEntries = getEntriesRecursive(entry, depth - 1);
				// Append subEntries to entries
				entries.splice(entries.end(), subEntries);
			}
		}
	}
	return entries;
}


bool isFile(const std::string& path) {
	struct stat statbuf;
	if (stat(path.c_str(), &statbuf))
		return false;
	return S_ISREG(statbuf.st_mode);
}


bool isDirectory(const std::string& path) {
	struct stat statbuf;
	if (stat(path.c_str(), &statbuf))
		return false;
	return S_ISDIR(statbuf.st_mode);
}


void moveFile(const std::string& srcPath, const std::string& destPath) {
	std::remove(destPath.c_str());
	// Whether this overwrites existing files is implementation-defined.
	// i.e. Mingw64 fails to overwrite.
	// This is why we remove the file above.
	std::rename(srcPath.c_str(), destPath.c_str());
}


void copyFile(const std::string& srcPath, const std::string& destPath) {
	// Open source
	FILE* source = fopen(srcPath.c_str(), "rb");
	if (!source)
		return;
	DEFER({
		fclose(source);
	});
	// Open destination
	FILE* dest = fopen(destPath.c_str(), "wb");
	if (!dest)
		return;
	DEFER({
		fclose(dest);
	});
	// Copy buffer
	const int bufferSize = (1 << 15);
	char buffer[bufferSize];
	while (1) {
		size_t size = fread(buffer, 1, bufferSize, source);
		if (size == 0)
			break;
		size = fwrite(buffer, 1, size, dest);
		if (size == 0)
			break;
	}
}


void createDirectory(const std::string& path) {
#if defined ARCH_WIN
	std::wstring pathW = string::toWstring(path);
	CreateDirectoryW(pathW.c_str(), NULL);
#else
	mkdir(path.c_str(), 0755);
#endif
}


void createDirectories(const std::string& path) {
	for (size_t i = 1; i < path.size(); i++) {
		char c = path[i];
		if (c == '/' || c == '\\')
			createDirectory(path.substr(0, i));
	}
	createDirectory(path);
}


void removeDirectory(const std::string& path) {
	rmdir(path.c_str());
}


void removeDirectories(const std::string& path) {
	removeDirectory(path);
	for (size_t i = path.size() - 1; i >= 1; i--) {
		char c = path[i];
		if (c == '/' || c == '\\')
			removeDirectory(path.substr(0, i));
	}
}


int getLogicalCoreCount() {
	return std::thread::hardware_concurrency();
}


void setThreadName(const std::string& name) {
#if defined ARCH_LIN
	pthread_setname_np(pthread_self(), name.c_str());
#elif defined ARCH_WIN
	// Unsupported on Windows
#endif
}


std::string getStackTrace() {
	int stackLen = 128;
	void* stack[stackLen];
	std::string s;

#if defined ARCH_LIN || defined ARCH_MAC
	stackLen = backtrace(stack, stackLen);
	char** strings = backtrace_symbols(stack, stackLen);

	// Skip the first line because it's this function.
	for (int i = 1; i < stackLen; i++) {
		s += string::f("%d: ", stackLen - i - 1);
		std::string line = strings[i];
#if 0
		// Parse line
		std::regex r(R"((.*)\((.*)\+(.*)\) (.*))");
		std::smatch match;
		if (std::regex_search(line, match, r)) {
			s += match[1].str();
			s += "(";
			std::string symbol = match[2].str();
			// Demangle symbol
			char* symbolD = __cxxabiv1::__cxa_demangle(symbol.c_str(), NULL, NULL, NULL);
			if (symbolD) {
				symbol = symbolD;
				free(symbolD);
			}
			s += symbol;
			s += "+";
			s += match[3].str();
			s += ")";
		}
#else
		s += line;
#endif
		s += "\n";
	}
	free(strings);

#elif defined ARCH_WIN
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, true);
	stackLen = CaptureStackBackTrace(0, stackLen, stack, NULL);

	SYMBOL_INFO* symbol = (SYMBOL_INFO*) calloc(sizeof(SYMBOL_INFO) + 256, 1);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	for (int i = 1; i < stackLen; i++) {
		SymFromAddr(process, (DWORD64) stack[i], 0, symbol);
		s += string::f("%d: %s 0x%0x\n", stackLen - i - 1, symbol->Name, symbol->Address);
	}
	free(symbol);
#endif

	return s;
}


int64_t getNanoseconds() {
#if defined ARCH_WIN
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	// TODO Check if this is always an integer factor on all CPUs
	int64_t nsPerTick = 1000000000LL / frequency.QuadPart;
	int64_t time = counter.QuadPart * nsPerTick;
	return time;
#endif
#if defined ARCH_LIN
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	int64_t time = int64_t(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
	return time;
#endif
#if defined ARCH_MAC
	using clock = std::chrono::high_resolution_clock;
	using time_point = std::chrono::time_point<clock>;
	time_point now = clock::now();
	using duration = std::chrono::duration<int64_t, std::nano>;
	duration d = now.time_since_epoch();
	return d.count();
#endif
}


void openBrowser(const std::string& url) {
#if defined ARCH_LIN
	std::string command = "xdg-open \"" + url + "\"";
	(void) std::system(command.c_str());
#endif
#if defined ARCH_MAC
	std::string command = "open \"" + url + "\"";
	std::system(command.c_str());
#endif
#if defined ARCH_WIN
	std::wstring urlW = string::toWstring(url);
	ShellExecuteW(NULL, L"open", urlW.c_str(), NULL, NULL, SW_SHOWDEFAULT);
#endif
}


void openFolder(const std::string& path) {
#if defined ARCH_LIN
	std::string command = "xdg-open \"" + path + "\"";
	(void) std::system(command.c_str());
#endif
#if defined ARCH_MAC
	std::string command = "open \"" + path + "\"";
	std::system(command.c_str());
#endif
#if defined ARCH_WIN
	std::wstring pathW = string::toWstring(path);
	ShellExecuteW(NULL, L"explore", pathW.c_str(), NULL, NULL, SW_SHOWDEFAULT);
#endif
}


void runProcessDetached(const std::string& path) {
#if defined ARCH_WIN
	SHELLEXECUTEINFOW shExInfo;
	ZeroMemory(&shExInfo, sizeof(shExInfo));
	shExInfo.cbSize = sizeof(shExInfo);
	shExInfo.lpVerb = L"runas";

	std::wstring pathW = string::toWstring(path);
	shExInfo.lpFile = pathW.c_str();
	shExInfo.nShow = SW_SHOW;

	if (ShellExecuteExW(&shExInfo)) {
		// Do nothing
	}
#else
	// Not implemented on Linux or Mac
	assert(0);
#endif
}


std::string getOperatingSystemInfo() {
#if defined ARCH_LIN || defined ARCH_MAC
	struct utsname u;
	uname(&u);
	return string::f("%s %s %s %s", u.sysname, u.release, u.version, u.machine);
#elif defined ARCH_WIN
	OSVERSIONINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.dwOSVersionInfoSize = sizeof(info);
	GetVersionExW(&info);
	// See https://docs.microsoft.com/en-us/windows/desktop/api/winnt/ns-winnt-_osversioninfoa for a list of Windows version numbers.
	return string::f("Windows %u.%u", info.dwMajorVersion, info.dwMinorVersion);
#endif
}


int unzipToFolder(const std::string& zipPath, const std::string& dir) {
	int err;
	// Open ZIP file
	zip_t* za = zip_open(zipPath.c_str(), 0, &err);
	if (!za) {
		WARN("Could not open ZIP file %s: error %d", zipPath.c_str(), err);
		return err;
	}
	DEFER({
		zip_close(za);
	});

	// Iterate ZIP entries
	for (int i = 0; i < zip_get_num_entries(za, 0); i++) {
		zip_stat_t zs;
		err = zip_stat_index(za, i, 0, &zs);
		if (err) {
			WARN("zip_stat_index() failed: error %d", err);
			return err;
		}

		std::string path = dir + "/" + zs.name;

		if (path[path.size() - 1] == '/') {
			// Create directory
			system::createDirectory(path);
			// HACK
			// Create and delete file to update the directory's mtime.
			std::string tmpPath = path + "/.tmp";
			FILE* tmpFile = fopen(tmpPath.c_str(), "w");
			fclose(tmpFile);
			std::remove(tmpPath.c_str());
		}
		else {
			// Open ZIP entry
			zip_file_t* zf = zip_fopen_index(za, i, 0);
			if (!zf) {
				WARN("zip_fopen_index() failed");
				return -1;
			}
			DEFER({
				zip_fclose(zf);
			});

			// Create file
			FILE* outFile = fopen(path.c_str(), "wb");
			if (!outFile) {
				WARN("Could not create file %s", path.c_str());
				return -1;
			}
			DEFER({
				fclose(outFile);
			});

			// Read buffer and copy to file
			while (true) {
				char buffer[1 << 15];
				int len = zip_fread(zf, buffer, sizeof(buffer));
				if (len <= 0)
					break;
				fwrite(buffer, 1, len, outFile);
			}
		}
	}
	return 0;
}


} // namespace system
} // namespace rack
