#pragma once
#include <iostream>
#include <stack>
#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <set>
#include <windows.h>
#include <ctime>
#include <cmath>
#include <thread>
#include <mutex>
using namespace std;

inline bool haveContent(string s, char filter = '\t') {
	for (auto &i : s) if (i != filter) return true;
	return false;
}

inline int countOf(string str, char charToFind) {
	int count = 0;
	for (auto &i : str) {
		if (i == charToFind) count++;
	}
	return count;
}

bool beginWith(string origin, string judger) {
	return origin.length() >= judger.length() && origin.substr(0, judger.length()) == judger;
}

int random() {
	static int seed = time(NULL);
	srand(seed);
	return seed = rand();
}

vector<string> split(string str, char delimiter = '\n', int maxsplit = -1, char allowedquotes = '"', char allowedinner = -1, bool reserve_dinner = false) {
	// Manually breaks
	bool qmode = false, dmode = false;
	vector<string> result;
	if (maxsplit > 0) result.reserve(maxsplit);
	string strtmp = "";
	for (size_t i = 0; i < str.length(); i++) {
		char &cs = str[i];
		if (cs == allowedquotes && (!dmode)) {
			qmode = !qmode;
		}
		if (cs == allowedinner && (!dmode)) {
			dmode = true;
			if (reserve_dinner) strtmp += cs;
		}
		else {
			dmode = false;
			if (cs == delimiter && (!qmode) && strtmp.length() && result.size() != maxsplit) {
				result.push_back(strtmp);
				strtmp = "";
			}
			else {
				strtmp += cs;
			}
		}
	}
	if (strtmp.length()) result.push_back(strtmp);
	return result;
}

class jumpertable {
public:

	jumpertable() {

	}

	size_t& operator [](size_t origin) {
		//if (origin == 11) {
		//	cout << "*Developer Debugger*" << endl;
		//}
		if ((!jmper.count(origin)) || jmper[origin].size() < 1) {
			jmper[origin] = vector<size_t>({ UINT16_MAX });
			return jmper[origin][0];
		}
		else {
			//return jmper[origin][jmper[origin].size() - 1];
			jmper[origin].push_back(jmper[origin][jmper[origin].size() - 1]);
			return jmper[origin][jmper[origin].size() - 1];
		}
	}

	bool revert(size_t origin, size_t revert_from, bool omit = false) {
		bool flag = false;
		while (jmper[origin].size() && jmper[origin][jmper[origin].size() - 1] == revert_from) {
			jmper[origin].pop_back();
			flag = true;
		}
		if (flag && (!omit)) {
			reverted[origin] = revert_from;
		}
		return flag;
	}

	bool revert_all(size_t revert_from, bool omit = false) {
		clear_revert();
		bool flag = false;
		for (auto &i : jmper) {
			flag |= revert(i.first, revert_from, omit);
		}
		return flag;
	}

	void rollback() {
		for (auto &i : reverted) {
			jmper[i.first].push_back(i.second);
		}
		clear_revert();
	}

	void clear_revert() {
		reverted = map<size_t, size_t>();
	}

	bool count(size_t origin) {
		return jmper.count(origin) && jmper[origin].size();
	}

private:
	map<size_t, vector<size_t> > jmper;
	map<size_t, size_t> reverted;
};

class inheritance_disjoint {
public:
	inheritance_disjoint() {

	}
	// Father: a, Son: b
	inline void unions(string a, string b) {
		father_classes[b].insert(a);
	}
	// Ask if b is a's father (or grand ... father) according to the usage
	bool is_same(string a, string b) {
		if (a == b) return true;
		auto mp = make_pair(a, b);
		if (result_caches.count(mp)) return result_caches[mp];
		bool result = false;
		for (auto &i : father_classes[a]) {
			if (is_same(i, b)) {
				result = true;
				break;
			}
		}
		return result_caches[mp] = result;
	}
private:
	//map<string, string> inhs;
	map<string, set<string> > father_classes;
	map<pair<string, string>, bool> result_caches;
} inh_map;

int getIndent(string &str, int maxfetch = -1) {
	int id = 0;
	while (str.length() && str[0] == '\n') str.erase(str.begin());
	while (str.length() && str[0] == '\t' && id != maxfetch) {
		id++;
		str.erase(str.begin());
	}
	return id;
}

int getIndentRaw(string str, int maxfetch = -1) {
	string s = str;
	return getIndent(s, maxfetch);
}


thread_local char buf0[255], buf01[255], buf1[65536];

#pragma region Declarations

intValue run(string code, varmap &myenv, string fname);
intValue calculate(string expr, varmap &vm);
void raiseError(intValue raiseValue, varmap &myenv, string source_function = "Unknown source", size_t source_line = 0, double error_id = 0, string error_desc = "");
intValue getValue(string single_expr, varmap &vm, bool save_quote = false, int multithreading = -1);
#define raise_ce(description) raiseError(null, myenv, fname, execptr + 1, __LINE__, description)
#define raise_varmap_ce(description) raiseError(null, *this, "Runtime", 0, __LINE__, description)
#define raise_gv_ce(description) raiseError(null, vm, "Runtime", 0, __LINE__, description);


#pragma endregion
