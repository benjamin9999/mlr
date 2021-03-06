
//#include "utils.h"
#include "stdafx.h"

#include <vector>
#include <codecvt>
#include <fstream>
#include <iostream>
#include <boost/format.hpp>

#include <Windows.h>

using namespace std;
using boost::format;

wstring s2ws(const string& str)
{
	typedef codecvt_utf8<wchar_t> convert_typeX;
	wstring_convert<convert_typeX, wchar_t> converterX;
	return converterX.from_bytes(str);
}

string ws2s(const wstring& wstr)
{
	typedef codecvt_utf8<wchar_t> convert_typeX;
	wstring_convert<convert_typeX, wchar_t> converterX;
	return converterX.to_bytes(wstr);
}

vector<string> fileglob(const string& pathpat) {
	vector<string> lst;
	WIN32_FIND_DATA ffd;

	HANDLE hFind = FindFirstFile(s2ws(pathpat).c_str(), &ffd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
			lst.push_back(ws2s(wstring(ffd.cFileName)));
		} while (FindNextFile(hFind, &ffd) != 0);
	}
	return lst;
}

vector<string> explode(const string& str, char ch) {
	vector<string> items;
	string src(str);
	int nextmatch = src.find(ch);
	while (1) {
		string item = src.substr(0, nextmatch);
		items.push_back(item);
		if (nextmatch == string::npos) break;
		src = src.substr(nextmatch + 1);
		nextmatch = src.find(ch);
	}
	return items;
}


/*
* example from
* http://nickperrysays.wordpress.com/2011/05/24/monitoring-a-file-last-modified-date-with-visual-c/
*/
long long getmtime(const string& fn) {
	long long mtime = -1;
	HANDLE hFile = CreateFileA(fn.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {
		FILETIME ftCreate, ftAccess, ftWrite;
		if (!GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite)) {
			mtime = 0;
		} else {
			mtime = (long long)ftWrite.dwHighDateTime << 32 | ftWrite.dwLowDateTime;
		}
		CloseHandle(hFile);
	}
	return mtime;
}


vector<char> file_get_contents(const string& fn) {
	vector<char> buf;
	ifstream f(fn);
	f.exceptions(ifstream::badbit | ifstream::failbit | ifstream::eofbit);
	f.seekg(0, ios::end);
	streampos length(f.tellg());
	if (length){
		f.seekg(0, ios::beg);
		buf.resize(static_cast<size_t>(length));
		f.read(&buf.front(), static_cast<std::size_t>(length));
	}
	return buf;
}

void file_get_contents(const string& fn, vector<char>& buf) {
	ifstream f(fn);
	f.exceptions(ifstream::badbit | ifstream::failbit | ifstream::eofbit);
	f.seekg(0, ios::end);
	streampos length(f.tellg());
	if (length){
		f.seekg(0, ios::beg);
		buf.resize(static_cast<size_t>(length));
		f.read(&buf.front(), static_cast<std::size_t>(length));
	}
}


unsigned get_cpu_count() {
	//return 1;
	SYSTEM_INFO si = { 0, };
	GetSystemInfo(&si);
	if (1)
		return si.dwNumberOfProcessors / 2;
	else
		return si.dwNumberOfProcessors;
}


void sse_configure()
{
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);  // SSE flush-denormals-to-zero
	_mm_setcsr(_mm_getcsr() | 0x8040);           // SSE2 denormals-are-zero
}


void bind_to_cpu(const unsigned cpu)
{
	const HANDLE self = GetCurrentThread();
	if (1) {
		DWORD_PTR pmask = SetThreadAffinityMask(self, 1 << (cpu*2));
	} else {
		DWORD_PTR pmask = SetThreadAffinityMask(self, 1 << cpu);
	}
}
