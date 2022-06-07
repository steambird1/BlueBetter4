#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <stack>
#include <map>
#include <vector>
#include <string>
#include <cstdio>
using namespace std;

char buf0[255];

class varmap {
	public:
		
		typedef vector<map<string,string> >::reverse_iterator			vit;
		
		varmap() {

		}
		void push() {
			vs.push_back(map<string,string>());
		}
		void pop() {
			vs.pop_back();
		}
		string& operator[](string key) {
			// Find where it is
			for (vit i = vs.rbegin(); i != vs.rend(); i++) {
				if (i->count(key)) return ((*i))[key];
			}
			if (glob_vs.count(key)) return glob_vs[key];
			return vs[vs.size()-1][key];
		}
		void set_global(string key, string value = "null") {
			glob_vs[key] = value;
		}
		void declare(string key) {
			vs[vs.size() - 1][key] = "null";
		}
	private:
		vector<map<string,string> >										vs;
			// Save evalable thing, like "" for string
		static map<string, string>										glob_vs;
};
map<string, string> varmap::glob_vs;

// quotes and dinner will be reserved
// N maxsplit = N+1 elements. -1 means no maxsplit
vector<string> split(string str, char delimiter = '\n', int maxsplit = -1, char allowedquotes = '"', char allowedinner = '\\') {
	bool qmode = false, dmode = false;
	vector<string> result;
	string strtmp = "";
	for (size_t i = 0; i < str.length(); i++) {
		char &cs = str[i];
		if (cs == allowedquotes && (!dmode)) {
			qmode = !qmode;
		}
		if (cs == allowedinner && (!dmode)) {
			dmode = true;
		} else {
			dmode = false;
			if (cs == delimiter && (!qmode) && strtmp.length() && result.size() != maxsplit) {
				result.push_back(strtmp);
				strtmp = "";
			} else {
				strtmp += cs;
			}
		}
	}
	if (strtmp.length()) result.push_back(strtmp);
	return result;
}

string formatting(string origin, char dinner = '\\') {
	string ns = "";
	bool dmode = false;
	for (size_t i = 0; i < origin.length(); i++) {
		if (origin[i] == dinner && (!dmode)) {
			dmode = true;
		}
		else {
			ns += origin[i];
			dmode = false;
		}
	}
	return ns;
}

struct intValue {
	double								numeric;
	string								str;
	bool								isNull = false;
	bool								isNumeric = false;
	
	intValue() {
		isNull = true;
	}
	intValue(double numeric) : numeric(numeric) {
		isNumeric = true;
		sprintf(buf0, "%lf", numeric);
		str = buf0;
	}
	intValue(string str) : str(str) {

	}

	// Usually use for debug proposes
	void output() {
		if (isNull) {
			cout << "null";
		}
		else if (isNumeric) {
			cout << "num:" << numeric;
		}
		else {
			cout << "str:" << formatting(str);
		}
	}

} null;

intValue getValue(string single_expr, varmap &vm) {
	if (single_expr == "null") return null;
	if (single_expr[0] == '"' && single_expr[single_expr.length() - 1] == '"') {
		return single_expr.substr(1, single_expr.length() - 2);
	}
	// Is number?
	bool dot = false, isnum = true;
	for (size_t i = 0; i < single_expr.length(); i++) {
		char &ch = single_expr[i];
		if (ch == '.') {
			if (dot) {
				// Not a number
				isnum = false;
				break;
			}
			else {
				dot = true;
			}
		}
		else if (ch < '0' || ch > '9') {
			isnum = false;
			break;
		}
	}
	if (isnum) {
		return atof(single_expr.c_str());
	}
	else {
		return getValue(vm[single_expr], vm);	// So you have refrences
	}
}

int main(int argc, char* argv[]) {
	// Test compments
	varmap vm;
	vm.push();
	vm.set_global("a");
	vm.set_global("b", "5");
	vm.set_global("c", "\"a\"");
	getValue("null", vm).output();
	cout << endl;
	getValue("5.6", vm).output();
	cout << endl;
	getValue("\"nu\\\"ll\\\"\"", vm).output();
	cout << endl;
	getValue("a", vm).output();
	cout << endl;
	getValue("b", vm).output();
	cout << endl;
	getValue("c", vm).output();
	cout << endl;
	// End

	return 0;
}
