#pragma once
#include <common.hpp>
#include <list>


namespace rack {


/** Cross-platform functions for operating systems routines
*/
namespace system {


/** Returns a list of all entries (directories, files, symbols) in a directory. */
std::list<std::string> getEntries(const std::string& path);
std::list<std::string> getEntriesRecursive(const std::string &path, int depth);
/** Returns whether the given path is a file. */
bool isFile(const std::string& path);
/** Returns whether the given path is a directory. */
bool isDirectory(const std::string& path);
/** Moves a file. */
void moveFile(const std::string& srcPath, const std::string& destPath);
/** Copies a file. */
void copyFile(const std::string& srcPath, const std::string& destPath);
/** Creates a directory.
The parent directory must exist.
*/
void createDirectory(const std::string& path);
/** Deletes a directory.
The directory must be empty. Fails silently.
*/
void removeDirectory(const std::string& path);
/** Returns the number of logical simultaneous multithreading (SMT) (e.g. Intel Hyperthreaded) threads on the CPU. */
int getLogicalCoreCount();
/** Sets a name of the current thread for debuggers and OS-specific process viewers. */
void setThreadName(const std::string& name);
/** Returns the caller's human-readable stack trace with "\n"-separated lines. */
std::string getStackTrace();
/** Returns the current number of nanoseconds since the epoch with the highest precion available on the OS.
The epoch is undefined and could be the UNIX epoch, time since boot, time since launch, etc.
*/
int64_t getNanoseconds();
/** Opens a URL, also happens to work with PDFs and folders.
Shell injection is possible, so make sure the URL is trusted or hard coded.
May block, so open in a new thread.
*/
void openBrowser(const std::string& url);
/** Opens Windows Explorer, Finder, etc at the folder location. */
void openFolder(const std::string& path);
/** Runs an executable without blocking.
The launched process will continue running if the current process is closed.
*/
void runProcessDetached(const std::string& path);
std::string getOperatingSystemInfo();
/** Unzips a ZIP file to a folder.
The folder must exist.
Returns 0 if successful.
*/
int unzipToFolder(const std::string& zipPath, const std::string& dir);


} // namespace system
} // namespace rack
