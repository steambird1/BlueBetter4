#define _CRT_SECURE_NO_WARNINGS

// Converting long64 and double
#pragma warning(disable:4244)
// Unused tab
#pragma warning(disable:4102)

#include <iostream>
#include <stack>
#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <set>
#include <windows.h>
using namespace std;

bool __spec = false;

// Pre declare
class varmap;
struct intValue;
intValue run(string code, varmap &myenv);
intValue calcute(string expr, varmap &vm);

HANDLE stdouth;
void setColor(DWORD color) {
	SetConsoleTextAttribute(stdouth, color);
}

inline void begindout() {
	setColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
}

inline void endout() {
	setColor(FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
}

inline void specialout() {
	setColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
}

const int max_indent = 65536;

char buf0[255], buf01[255], buf1[65536];
bool in_debug = false;	// Runner debug option.
set<size_t> breakpoints;

// It's so useful that everyone needs it
bool beginWith(string origin, string judger) {
	return origin.length() >= judger.length() && origin.substr(0, judger.length()) == judger;
}

// At most delete maxfetch.
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

// quotes and dinner will be reserved
// N maxsplit = N+1 elements. -1 means no maxsplit
vector<string> split(string str, char delimiter = '\n', int maxsplit = -1, char allowedquotes = '"', char allowedinner = -1) {
	// Manually breaks
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

string unformatting(string origin) {
	string ns = "\"";
	for (size_t i = 0; i < origin.length(); i++) {
		if (origin[i] == '"' || origin[i] == '\\') {
			ns += "\\";
			ns += origin[i];
			// for debug propose
			//cout << ns << endl;
		}
		else {
			ns += origin[i];
		}
	}
	return ns + "\"";
}

struct intValue {
	double								numeric;
	string								str;
	bool								isNull = false;
	bool								isNumeric = false;

	intValue() {
		isNull = true;
		numeric = 0;
		str = "";
	}
	intValue(double numeric) : numeric(numeric) {
		isNumeric = true;
		sprintf(buf0, "%lf", numeric);
		str = buf0;
		while (str.back() == '0') str.pop_back();
		if (str.back() == '.') str.pop_back();
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
			cout << "str:" << str;
		}
	}

	string unformat() {
		if (isNull) {
			return "null";
		}
		else if (isNumeric) {
			sprintf(buf01, "%lf", numeric);
			return buf01;
		}
		else {
			// Unformat this string
			return unformatting(str);
		}
	}

	bool boolean() {
		if (isNull) return false;
		if (isNumeric)
			if (this->numeric == 0) return false;
			//return true;
		if (this->str.length()) return true;
		return false;
	}

} null;

// All the varmaps show one global variable space.
/*
Special properties:

__arg__			For a function, showing its parameters (delimitered by space).
__init__		For a class definition, showing its initalizing function.
	For class,	[name].[function]
__type__		For a class object, showing its kind.
*/
class varmap {
	public:
		
		typedef vector<map<string,string> >::reverse_iterator			vit;
		typedef map<string, string>::iterator							mit;
		
		varmap() {

		}
		void push() {
			vs.push_back(map<string,string>());
		}
		void pop() {
			vs.pop_back();
		}
		// If return object serial, DON'T MODIFY IT !
		string& operator[](string key) {
#pragma region Debug Purpose
			//cout << "Require key: " << key << endl;
#pragma endregion
			// Find where it is
			if (key == "this") {
				// Must be class
				return (*this_source)[this_name] = this_source->serial(this_name);
			}
			else if (key.substr(0, 5) == "this.") {
				vector<string> s = split(key, '.', 1);
				return (*this_source)[this_name + "." + s[1]];
			}
			else {
				for (vit i = vs.rbegin(); i != vs.rend(); i++) {
					if (i->count(key)) {
						if (unserial.count((*i)[key + ".__type__"])) {
							return ((*i))[key];
						}
						else {
							return ((*i))[key] = serial(key);
						}
						
					}
				}
				if (glob_vs.count(key)) return glob_vs[key];
				if (key.find('.') != string::npos) {
					// Must in same layer
					vector<string> la = split(key, '.', 1);
					for (vit i = vs.rbegin(); i != vs.rend(); i++) {
						if (i->count(la[0])) {
							(*i)[key] = "null";
							return (*i)[key];
						}
					}
				}
				else {
					if (!vs[vs.size() - 1].count(key)) vs[vs.size() - 1][key] = "null";
				}
				return vs[vs.size() - 1][key];
			}
			
		}
		string serial(string name) {
			// Note: global variable can't contain class
			
			string tmp = mymagic;
			for (vit i = vs.rbegin(); i != vs.rend(); i++) {
				if (i->count(name)) {
					for (auto &j : (*i)) {
						if (beginWith(j.first, name + ".")) {
							vector<string> spl = split(j.first, '.', 1);
							// debug
							//cout << "add: " << string(".") + spl[1] + "=" + j.second + "\n" << endl;
							// end
							tmp += string(".") + spl[1] + "=" + j.second + "\n";
						}
					}
				}
			}
			return tmp;
		}
		void deserial(string name, string serial) {
			if (!beginWith(serial, mymagic)) {
				return;
			}
			serial = serial.substr(mymagic.length());
			vector<string> lspl = split(serial);
			for (auto &i : lspl) {
				vector<string> itemspl = split(i, '=', 1);
				if (itemspl.size() < 2) itemspl.push_back("null");
				(*this)[name + itemspl[0]] = itemspl[1];
			}
			(*this)[name] = "null";
		}
		void set_global(string key, string value = "null") {
			glob_vs[key] = value;
		}
		void declare(string key) {
			vs[vs.size() - 1][key] = "null";
		}
		void set_this(varmap *source, string name) {
			this_name = name;
			this_source = source;
		}
		void dump() {
			cout << "*** VARMAP DUMP ***" << endl;
			cout << "this pointer: " << this_name << endl << "partial:" << endl;
			for (vit i = vs.rbegin(); i != vs.rend(); i++) {
				for (mit j = i->begin(); j != i->end(); j++) {
					cout << j->first << " = " << j->second << endl;
				}
				cout << endl;
			}
			cout << "global:" << endl;
			for (mit i = glob_vs.begin(); i != glob_vs.end(); i++) {
				cout << i->first << " = " << i->second << endl;
			}
			cout << "*** END OF DUMP ***" << endl;
		}
	
		string															this_name = "";
		varmap															*this_source;
			// Where 'this' points. use '.'
		const string mymagic = "__object$\n";
private:
	const set<string> unserial = { "", "function", "class", "null" };
	vector<map<string, string> >										vs;
	// Save evalable thing, like "" for string
		static map<string, string>										glob_vs;
};
map<string, string> varmap::glob_vs;

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

intValue getValue(string single_expr, varmap &vm) {
	if (single_expr == "null") return null;
	if (single_expr[0] == '"' && single_expr[single_expr.length() - 1] == '"') {
		return formatting(single_expr.substr(1, single_expr.length() - 2));
	}
	// Is number?
	bool dot = false, isnum = true;
	double neg = 1;	// Unused, at least now
	for (size_t i = 1; i < single_expr.length(); i++) {	// Not infl.
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
	char &fch = single_expr[0];
	if (fch < '0' || fch > '9') {
		if (fch == '-') {
			neg = -1;
			single_expr.erase(single_expr.begin());
		}
		else {
			isnum = false;
		}
	}
	if (isnum) {
		return atof(single_expr.c_str()) * neg;
	}
	else {
		vector<string> spl = split(single_expr, ' ', 1);
		vector<string> dotspl = split(spl[0], '.', 1);
		string set_this = "";
		if (dotspl.size() > 1 && vm[dotspl[0] + ".__type__"] != "null" && vm[dotspl[0] + ".__type__"] != "function") {
			set_this = dotspl[0];
			spl[0] = vm[dotspl[0] + ".__type__"] + "." + dotspl[1];
		}
		if (vm[spl[0] + ".__type__"] == "function") {
			// A function call.
			
			varmap nvm;
			nvm.push();
			nvm.set_this(vm.this_source, vm.this_name);
			if (spl[0].find('.') != string::npos) {
				vector<string> xspl = split(spl[0], '.');
				nvm.set_this(&vm, xspl[0]);
				xspl[0] = vm[spl[0] + ".__type__"];
			}
			string args = vm[spl[0] + ".__arg__"];
			if (args.length()) {
				vector<string> argname = split(args, ' ');
				vector<string> arg;
				vector<intValue> ares;
				bool str = false, dmode = false;
				if (spl.size() >= 2) {
					//arg = split(spl[1], ',');
					int quotes = 0;
					string tmp = "";
					for (auto &i : spl[1]) {
						if (i == '(') {
							if (quotes) tmp += i;
							quotes++;
						}
						else if (i == ')') {
							quotes--;
							if (quotes) tmp += i;
						} else if (i == '\\' && (!dmode)) {
							dmode = true;
							tmp += i;
						}
						else if (i == '"' && (!dmode)) {
							str = !str;
							tmp += i;
						}
						else if (i == ',' && (!quotes) && (!str)) {
							arg.push_back(tmp);
							tmp = "";
						}
						else {
							tmp += i;
						}
						dmode = false;
					}
					if (tmp.length()) arg.push_back(tmp);
				}
				else {
					arg.push_back(spl[1]);
				}
				if (arg.size() < argname.size()) return null;
				for (size_t i = 0; i < arg.size(); i++) {
					nvm[argname[i]] = (calcute(arg[i], vm)).unformat();
				}
			}
			if (set_this.length()) nvm.set_this(&vm, set_this);
			string s = vm[spl[0]];
			if (s == "null" || s.length() == 0) {
				cout << "Warning: Call of null function " << spl[0] << endl;
			}
			__spec = true;
			auto r = run(s, nvm);
			__spec = false;
			return r;
		}
		else {
			return getValue(vm[single_expr], vm);
		}
			// So you have refrences
	}
}

int priority(char op) {
	switch (op) {
	case ')':
		return 6;
		break;
	case ':': case '#':
		return 5;
		break;
	case '&': case '|':
		return 4;
		break;
	case '*': case '/': case '%':
		return 3;
		break;
	case '+': case '-':
		return 2;
		break;
	case '>': case '<': case '=':
		return 1;
		break;
	case '(':
		return 0;
		break;
	default:
		return -1;
	}
}

typedef long long							long64;
typedef unsigned long long					ulong64;

// Please notice special meanings.
intValue primary_calcute(intValue first, char op, intValue second, varmap &vm) {
	switch (op) {
	case '(': case ')':
		break;
	case ':':
		// As for this, 'first' should be direct var-name
		return getValue(vm[first.str + "." + second.str], vm);
		break;
	case '#':
		// To get a position for string, or power for integer.
		if (first.isNumeric) {
			return pow(first.numeric, second.numeric);
		}
		else {
			ulong64 ul = ulong64(second.numeric);
			if (ul >= first.str.length()) {
				return null;
			}
			else {
				return string({ first.str[ul] });
			}
		}
		break;
	case '*':
		if (first.isNumeric) {
			return first.numeric * second.numeric;
		}
		else {
			string rep = "";
			for (int cnt = 0; cnt < second.numeric; cnt++) {
				rep += first.str;
			}
			return rep;
		}
		break;
	case '%':
		return long64(first.numeric) % long64(second.numeric);
		break;
	case '/':
		return first.numeric / second.numeric;
		break;
	case '+':
		if (first.isNumeric) {
			return first.numeric + second.numeric;
		}
		else {
			return first.str + second.str;
		}
		break;
	case '-':
		return first.numeric - second.numeric;
		break;
	case '>':
		if (first.isNumeric) {
			return first.numeric > second.numeric;
		}
		else {
			return first.str > second.str;
		}
		break;
	case '<':
		if (first.isNumeric) {
			return first.numeric < second.numeric;
		}
		else {
			return first.str < second.str;
		}
		break;
	case '=':
		if (first.isNull && second.isNull) {
			return true;
		}
		if (first.isNumeric) {
			return first.numeric == second.numeric;
		}
		else {
			return first.str == second.str;
		}
		break;
	case '&':
		if (first.isNumeric && second.isNumeric) {
			return ulong64(first.numeric) & ulong64(second.numeric);
		}
		else {
			if (first.isNull || second.isNull) return 0;
			if (first.str.length() == 0 || second.str.length() == 0) return 0;
			return 1;
		}
		break;
	case '|':
		if (first.isNumeric && second.isNumeric) {
			return ulong64(first.numeric) | ulong64(second.numeric);
		}
		else {
			if (first.isNull && second.isNull) return 0;
			if (first.str.length() == 0 && second.str.length() == 0) return 0;
			return 1;
		}
		break;
	default:
		return null;
	}
}

// The interpretion of '\\', '"' will be finished here! Check the code.
intValue calcute(string expr, varmap &vm) {
	stack<char> op;
	stack<string> val;
	string operand = "";
	bool cur_neg = false, qmode = false, dmode = false;
	int ignore = 0;
	for (size_t i = 0; i < expr.length(); i++) {
		if (expr[i] == '"' && (!dmode)) qmode = !qmode;
		if (expr[i] == '\\' && (!dmode)) dmode = true;
		else dmode = false;
		int my_pr = priority(expr[i]), op_pr = -2;
		if (my_pr >= 0 && (!qmode) && (!dmode)) {
			if (expr[i] == '(') {
				// Here should be operator previously.
				if (i == 0 || priority(expr[i - 1]) >= 0) {
					op.push('(');
				}
				else {
					ignore++;
					operand += expr[i];
				}
			}
			else if (expr[i] == ')') {
				if (ignore <= 0) {
					if (operand.length()) {
						val.push(string(cur_neg ? "-" : "") + operand);
						cur_neg = false;
					}
					while ((!op.empty()) && (op.top() != '(')) {
						intValue v1, v2;
						char mc = op.top();
						op.pop();

						v1 = getValue(val.top(), vm);

						val.pop();
						if (mc == ':') {
							v2 = intValue(val.top());
						}
						else {
							v2 = getValue(val.top(), vm);
						}

						val.pop();
						intValue pres = primary_calcute(v2, mc, v1, vm);
						val.push(pres.unformat());
					}
					op.pop();	// '('
					operand = "";
				}
				else {
					ignore--;
					operand += expr[i];
				}
				
			}
			else if (ignore <= 0) {
				// May check here.
				if (expr[i] == '-' && (i == 0 || expr[i - 1] == '(' || expr[i - 1] == ',' || expr[i - 1] == ' ')) {
					if (i > 0 && (expr[i - 1] == ',' || expr[i - 1] == ' ')) {
						// It's the beginning of function parameters!
						operand += expr[i];
					}
					else {
						cur_neg = true;
					}
				}
				else {
					if (operand.length()) {
						val.push(string(cur_neg ? "-" : "") + operand);
						cur_neg = false;
					}
					while ((!op.empty()) && (op_pr = priority(op.top())) > my_pr) {
						intValue v1, v2;
						char mc = op.top();
						op.pop();
						v1 = getValue(val.top(), vm);


						val.pop();
						if (mc == ':') {
							v2 = intValue(val.top());
						}
						else {
							v2 = getValue(val.top(), vm);
						}
						val.pop();
						intValue pres = primary_calcute(v2, mc, v1, vm);
						val.push(pres.unformat());
					}
					op.push(expr[i]);
					operand = "";
				}
			}
			else {
				operand += expr[i];
			}
			
		}
		else {
			operand += expr[i];
		}
	}
	if (operand.length()) {
		val.push(string(cur_neg ? "-" : "") + operand);
		cur_neg = false;
	}
	while (!op.empty()) {
		intValue v1 = getValue(val.top(), vm), v2;
		val.pop();
		char mc = op.top();
		op.pop();
		if (mc == ':') {
			v2 = intValue(val.top());
		}
		else {
			v2 = getValue(val.top(), vm);
		}
		val.pop();
		intValue pres = primary_calcute(v2, mc, v1, vm);
		val.push(pres.unformat());
	}
	return getValue(val.top(), vm);
}

#define parameter_check(req) do {if (codexec.size() < req) {cout << "Error: required parameter not given (" << __FILE__ << "#" << __LINE__ << ")" << endl; return null;}} while (false)
#define parameter_check2(req,ext) do {if (codexec2.size() < req) {cout << "Error: required parameter not given in sub command " << ext << " (" << __FILE__ << "#" << __LINE__ << ")" << endl; return null;}} while (false)
#define parameter_check3(req,ext) do {if (codexec3.size() < req) {cout << "Error: required parameter not given in sub command " << ext << " (" << __FILE__ << "#" << __LINE__ << ")" << endl; return null;}} while (false)
#define dshell_check(req) do {if (spl.size() < req) {cout << "Bad command" << endl; exit(1);}} while (false)

string curexp(string exp, varmap &myenv) {
	vector<string> dasher = split(exp, ':');
	if (dasher.size() == 1) return exp;
	// calcute until the last.
	intValue final = calcute(dasher[dasher.size() - 1], myenv);
	for (size_t i = dasher.size() - 2; i >= 1; i--) {
		final = calcute(dasher[i] + ":" + final.str, myenv);
	}
	return dasher[0] + "." + final.str;
}

map<int, FILE*> files;

class jumpertable {
public:

	jumpertable() {

	}

	size_t& operator [](size_t origin) {
		if ((!jmper.count(origin)) || jmper[origin].size() < 1) {
			jmper[origin] = vector<size_t>({ UINT16_MAX });
			return jmper[origin][0];
		}
		else {
			//return jmper[origin][jmper[origin].size() - 1];
			jmper[origin].push_back(jmper[origin][jmper[origin].size()-1]);
			return jmper[origin][jmper[origin].size() - 1];
		}
	}

	bool revert(size_t origin, size_t revert_from) {
		bool flag = false;
		while (jmper[origin].size() && jmper[origin][jmper[origin].size()-1] == revert_from) {
			jmper[origin].pop_back();
			flag = true;
		}
		if (flag) reverted[origin] = revert_from;
		return flag;
	}

	bool revert_all(size_t revert_from) {
		clear_revert();
		bool flag = false;
		for (auto &i : jmper) {
			flag |= revert(i.first, revert_from);
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
		return jmper.count(origin);
	}

private:
	map<size_t, vector<size_t> > jmper;
	map<size_t, size_t> reverted;
};

// This 'run' will skip ALL class and function declarations.
// Provided environment should be pushed.
intValue run(string code, varmap &myenv) {
	vector<string> codestream = split(code);
	size_t execptr = 0;
	jumpertable jmptable;
	int prevind = 0;
	bool noroll = false;
	while (execptr < codestream.size()) {
#pragma region User Debugger
		if (in_debug && (!__spec)) {
			begindout();
			cout << "-> Pre-execute" << endl;
			string command = "";
			if (breakpoints.count(execptr)) {
				do {
					cout << "-> ";
					getline(cin, command);
					vector<string> spl = split(command, ' ', 1);
					if (spl.size() <= 0) continue;
					if (spl[0] == "break") {
						dshell_check(2);
						breakpoints.insert(atoi(spl[1].c_str()));
					}
					else if (spl[0] == "bdel") {
						if (breakpoints.count(atoi(spl[1].c_str()))) breakpoints.erase(atoi(spl[1].c_str()));
					}
					else if (spl[0] == "blist") {
						for (auto &i : breakpoints) cout << i << " ";
						cout << endl;
					}
					else if (spl[0] == "current") {
						cout << "Current line:\n" << codestream[execptr] << endl;
					}
					else if (spl[0] == "quit") {
						exit(0);
					}
					else if (spl[0] == "view") {
						dshell_check(2);
						cout << spl[1] << " = ";
						calcute(spl[1], myenv).output();
						cout << endl;
					}
					else if (spl[0] == "exec") {
						dshell_check(2);
						specialout();
						__spec = true;
						run(spl[1], myenv);
						__spec = false;
						begindout();
					}
				} while (command != "run");
			}
			endout();
		}
#pragma endregion
		vector<string> codexec = split(codestream[execptr], ' ', 1);
		if (codexec.size() && codexec[0][0] == '\n') codexec[0].erase(codexec[0].begin()); // LF, why?
		jmptable.revert_all(execptr);
		noroll = false;
		int ind = getIndent(codexec[0]);
		if (prevind < ind) myenv.push();
		else if (prevind > ind) myenv.pop();
		// To be filled ...
		if (codexec[0] == "class" || codexec[0] == "function")  {
			string s = "";
			do {
				s = codestream[++execptr];
			} while (getIndent(s) > 0);
			goto after_add_exp;
		}
		else if (codexec[0] == "print") {
			parameter_check(2);
			cout << calcute(codexec[1], myenv).str;
		}
		else if (codexec[0] == "return") {
			parameter_check(2);
			return calcute(codexec[1], myenv);
		}
		else if (codexec[0] == "set") {
			parameter_check(2);
			vector<string> codexec2 = split(codexec[1], '=', 1);
			parameter_check2(2,"set");
			if (codexec2[0][0]=='$') {
				codexec2[0].erase(codexec2[0].begin());
				codexec2[0] = calcute(codexec2[0], myenv).str;
			} else if (codexec2[0].find(":") != string::npos) {
				codexec2[0] = curexp(codexec2[0], myenv);
			}
			if (codexec2[1].length() > 4 && codexec2[1].substr(0, 4) == "new ") {
				vector<string> azer = split(codexec2[1], ' ');
				varmap vm;
				vm.push();
				vm.set_this(&myenv, codexec2[0]);
				myenv[codexec2[0]] = "null";
				myenv[codexec2[0] + ".__type__"] = azer[1];
				__spec = true;
				run(myenv[azer[1] + ".__init__"], vm);
				__spec = false;
			}
			else if (beginWith(codexec2[1], "object ")) {
				vector<string> codexec3 = split(codexec2[1], ' ', 1);
				parameter_check3(2, "Operator number");
				myenv.deserial(codexec2[0], calcute(codexec3[1], myenv).str);
			}
			else if (beginWith(codexec2[1], "serial ")) {
				vector<string> codexec3 = split(codexec2[1], ' ', 1);
				parameter_check3(2, "Operator number");
				myenv[codexec2[0]] = intValue(myenv.serial(codexec3[1])).unformat();
			}
#pragma region Internal Calcutions
			else if (codexec2[1] == "__input") {
				string t;
				getline(cin, t);
				myenv[codexec2[0]] = intValue(t).unformat();
			}
			else if (beginWith(codexec2[1], "__int ")) {
				vector<string> codexec3 = split(codexec2[1], ' ', 1);
				parameter_check3(2, "Operator number");
				myenv[codexec2[0]] = intValue(atof(calcute(codexec3[1], myenv).str.c_str())).unformat();
			}
			else if (beginWith(codexec2[1], "__chr ")) {
				vector<string> codexec3 = split(codexec2[1], ' ', 1);
				parameter_check3(2, "Operator number");
				char ch = char(int(calcute(codexec3[1], myenv).numeric));
				myenv[codexec2[0]] = intValue(string({ ch })).unformat();
			}
			else if (beginWith(codexec2[1], "__ord ")) {
				vector<string> codexec3 = split(codexec2[1], ' ', 1);
				parameter_check3(2, "Operator number");
				intValue cv = calcute(codexec3[1], myenv);
				if (cv.str.length()) {
					myenv[codexec2[0]] = intValue(int(char((cv.str[0])))).unformat();
				}
				else {
					myenv[codexec2[0]] = intValue(-1).unformat();
				}
				
			}
			else if (beginWith(codexec2[1], "__len")) {
				vector<string> codexec3 = split(codexec2[1], ' ', 1);
				intValue rsz = calcute(codexec3[1], myenv);
				myenv[codexec2[0]] = intValue(rsz.str.length()).unformat();
			}
			else if (beginWith(codexec2[1], "__intg ")) {
				vector<string> codexec3 = split(codexec2[1], ' ', 1);
				intValue rsz = calcute(codexec3[1], myenv);
				int res = int(rsz.numeric);
				myenv[codexec2[0]] = intValue(res).unformat();
			}
			else if (codexec2[1] == "__input_int") {
				//myenv[codexec2[0]]
				double dv;
				scanf("%lf", &dv);
				myenv[codexec2[0]] = intValue(dv).unformat();
			}
#pragma endregion
			else {
				myenv[codexec2[0]] = calcute(codexec2[1], myenv).unformat();
			}
			
		}
		else if (codexec[0] == "if" || codexec[0] == "elif") {
			parameter_check(2);
			// Certainly you have ':'
			if (codexec[1].length()) codexec[1].pop_back();
			if (calcute(codexec[1], myenv).boolean()) {
				// True, go on execution, and set jumper to the end of else before any else / elif
				size_t rptr = execptr;								// Where our code ends
				while (getIndentRaw(codestream[++rptr]) != ind);	// Go on until aligned else / elif
				rptr--;												// Something strange
				size_t eptr = execptr;
				while (eptr < codestream.size() - 1) {
					// End if indent equals and not 'elif' or 'else'.
					//if () break;
					string s = codestream[++eptr];
					int r = getIndent(s);
					vector<string> sp = split(s, ' ', 1);
					if (!sp.size()) continue;
					if (r == ind && sp[0] != "elif" && sp[0] != "else:") goto end101;
					if (r < ind) { // Multi-jump at once: Too far
						// Direct jumps
						if (jmptable.count(eptr - 1)) eptr = jmptable[eptr - 1];
						goto end101;
					}
				}
				jmptable[rptr] = codestream.size();	// Already end of code!
				goto end102;
			end101: jmptable[rptr] = eptr;
			end102:;
			}
			else {
				// Go on elif / else
				size_t eptr = execptr + 1;
				while (eptr < codestream.size() - 1) {
					// End if indent equals and not 'elif' or 'else'.
					string s = codestream[++eptr];
					int r = getIndent(s);
					vector<string> sp = split(s, ' ', 1);
					if (!sp.size()) continue;
					if (r <= ind) break;
				}
				execptr = eptr;
				goto after_add_exp;
			}
		}
		else if (codexec[0] == "else:") {
			// Go on executing
		}
		else if (codexec[0] == "run") {
			parameter_check(2);
			calcute(codexec[1], myenv);
		}
		else if (codexec[0] == "while") {
			parameter_check(2);
			if (codexec[1].length()) codexec[1].pop_back();
			if (calcute(codexec[1], myenv).boolean()) {
				// True, go on execution, and set jumper to here at the end.
				size_t eptr = execptr;
				while (eptr != codestream.size() - 1) {
					// End if indent equals.
					int r = getIndentRaw(codestream[++eptr]);
					if (r <= ind) {
						jmptable[--eptr] = execptr;
						goto after99;
					}
				}
				jmptable[eptr] = execptr;
			after99:;
			}
			else {
				noroll = true; //  End of statements' life
				while (execptr != codestream.size()-1 ) {
					int r = getIndentRaw(codestream[++execptr]);
					if (r == ind) goto after_add_exp; //break;
					if (r < ind) { // Multi-jump at once: Too far
						// Direct jumps
						if (jmptable.count(execptr - 1)) execptr = jmptable[execptr - 1];
						goto after_add_exp;
					}
				}
				if (jmptable.count(execptr)) execptr = jmptable[execptr]; // Must not look for execution

				goto after_add_exp;

			}
		}
		else if (codexec[0] == "continue") {
			// Go back to previous (nearest) small-indent
			while (execptr > 0) {
				execptr--;
				string s = codestream[execptr];
				int id = getIndent(s);
				vector<string> spl = split(s, ' ', 1);
				if (id < ind && (spl[0] == "while" || spl[0] == "for")) break;

			}
			goto after_add_exp;
		}
		else if (codexec[0] == "break") {
		// Find for looper
		size_t ep = execptr;
		int ide;	// The indent to search
		while (ep > 0) {
			ep--;
			string s = codestream[ep];
			ide = getIndent(s);
			vector<string> spl = split(s, ' ', 1);
			if (ide < ind && (spl[0] == "while" || spl[0] == "for")) break;

		}
		// ep is the position
			while (execptr != codestream.size() - 1) {
				execptr++;
				string s = codestream[execptr];
				int id = getIndent(s);
				//vector<string> spl = split(s, ' ', 1);
				//if (spl[0] != "for" && spl[0] != "while") continue;
				if (id == ide) goto after_add_exp;
				else if (id < ide) {
					if (jmptable.count(execptr - 1)) execptr = jmptable[execptr - 1];
					goto after_add_exp;
				}
				
			}
			if (jmptable.count(execptr)) execptr = jmptable[execptr];
			goto after_add_exp;
		}
		else if (codexec[0] == "for") {
			// for [var]=[begin]~[end]~[step]
			parameter_check(2);
			if (codexec[1].length()) codexec[1].pop_back();
			vector<string> codexec2 = split(codexec[1], '=', 1);
			if (codexec2[0].find(":") != string::npos) {
				codexec2[0] = curexp(codexec2[0], myenv);
			}
			vector<string> rangeobj = split(codexec2[1], '~');
			intValue stepper = 1, current;
			if (rangeobj.size() >= 3) stepper = calcute(rangeobj[2], myenv);
			if (myenv[codexec2[0]] == "null") {
				current = intValue(calcute(rangeobj[0], myenv).numeric);
			}
			else {
				current = intValue(calcute(myenv[codexec2[0]], myenv).numeric + stepper.numeric);
			}
			myenv[codexec2[0]] = current.unformat();
			if (calcute(myenv[codexec2[0]], myenv).numeric == calcute(rangeobj[1], myenv).numeric) {
				// Jump where end-of-loop
				noroll = true; //  End of statements' life
				while (execptr != codestream.size() - 1) {
					int r = getIndentRaw(codestream[++execptr]);
					if (r == ind) goto after_add_exp; //break;
					if (r < ind) { // Multi-jump at once: Too far
						// Direct jumps
						if (jmptable.count(execptr - 1)) execptr = jmptable[execptr - 1];
						goto after_add_exp;
					}
				}
				if (jmptable.count(execptr)) execptr = jmptable[execptr]; // Must not look for execution
				goto after_add_exp;
			}
			else {
				size_t eptr = execptr;
				while (eptr != codestream.size() - 1) {
					// End if indent equals.
					int r = getIndentRaw(codestream[++eptr]);
					if (r <= ind) {
						jmptable[eptr-1] = execptr;
						goto afterset1;
					}
				}
				jmptable[eptr] = execptr;	// Should be changed in 'while' loop??
			afterset1:;
			}
		}
		else if (codexec[0] == "dump") {
		if (codexec.size() >= 2) {
			cout << myenv.serial(codexec[1]) << endl;
		}
		else {
			myenv.dump();
		}
			
		}
		else if (codexec[0] == "file") {
			// Opening, reading, writing or closing file.
			// File handle is an integer.

			// codexec2[1] is value.
			vector<string> codexec2 = split(codexec[1], ' ', 1), codexec3;
			parameter_check(2);
			string &op = codexec2[0];
			if (op == "close") {
				int f = calcute(codexec2[1], myenv).numeric;
				if (files.count(f)) {
					fclose(files[f]);
					files.erase(f);
				}
				else {
					cout << "Incorrect file: " << f << endl;
				}
				
			}
			else if (op == "write") {
				codexec3 = split(codexec2[1], ',', 1);
				int f = calcute(codexec3[0], myenv).numeric;
				if (files.count(f)) {
					fputs(calcute(codexec3[1], myenv).str.c_str(), files[f]);
				}
				else {
					cout << "Incorrect file: " << f << endl;
				}
				
			}
			else {
				if (op == "open") {
					// file open [var]=[filename],[mode]
					codexec3 = split(codexec2[1], '=', 1);
					vector<string> codexec4 = split(codexec3[1], ',', 1);
					int n = 0;
					while (files.count(n++));
					if (codexec3[0].find(":") != string::npos) {
						codexec3[0] = curexp(codexec3[0], myenv);
					}
					myenv[codexec3[0]] = intValue(n).unformat();
					intValue ca = calcute(codexec4[0], myenv);
					string fn = ca.str;
					files[n] = fopen(fn.c_str(), calcute(codexec4[1], myenv).str.c_str());
					if (files[n] == NULL) {
						cout << "Cannot open file " << fn << " as " << n << ", errno: " << errno << endl;
					}
				}
				else if (op == "read") {
					// file read [store]=[var]
					codexec3 = split(codexec2[1], '=', 1);
					int fid = calcute(codexec3[1], myenv).numeric;
					if (files.count(fid) && !feof(files[fid])) {
						string res = "";
						char c;
						while ((!feof(files[fid])) && (c = fgetc(files[fid])) != '\n') res += c;
						if (codexec3[0].find(":") != string::npos) {
							codexec3[0] = curexp(codexec3[0], myenv);
						}
						myenv[codexec3[0]] = intValue(res).unformat();
					}
					else {
						cout << "Incorrect file: " << fid << endl;
					}
				}
				else if (op == "vaild") {
					// file vaild [store]=[var]
					codexec3 = split(codexec2[1], '=', 1);
					if (codexec3[0].find(":") != string::npos) {
						codexec3[0] = curexp(codexec3[0], myenv);
					}
					int fid = calcute(codexec3[1], myenv).numeric;
					if (files.count(fid) && !feof(files[fid])) {
						myenv[codexec3[0]] = intValue(1).unformat();
					}
					else {
						myenv[codexec3[0]] = intValue(0).unformat();
					}
				}
			}
			
		}
		else if (codexec[0] == "import") {
			// Do nothing
		}
	add_exp: if (jmptable.count(execptr)) {
		execptr = jmptable[execptr];
	}
			 else {
		execptr++;
	}
		 after_add_exp: prevind = ind;
			 if (!noroll) jmptable.rollback();
	}
	return null;
}

intValue preRun(string code) {
	// Should prepare functions for it.
	varmap newenv;
	newenv.push();
	vector<string> codestream;
	// Initalize libraries right here
	FILE *f = fopen("bmain.blue", "r");
	if (f != NULL) {
		while (!feof(f)) {
			fgets(buf1, 65536, f);
			codestream.push_back(buf1);
		}
	}
	fclose(f);
	// End

	vector<string> sc = split(code);
	codestream.insert(codestream.end()-1,sc.begin(),sc.end());
	string curclass = "";					// Will append '.'
	string curfun = "", cfname = "", cfargs = "";
	int fun_indent = max_indent;
	for (size_t i = 0; i < codestream.size(); i++) {
		vector<string> codexec = split(codestream[i], ' ', 2);
		if (codexec.size() <= 0 || codexec[0].length() <= 0) continue;
		int ind = getIndent(codexec[0], 2);
		if (codexec[0].length() && codexec[0][codexec[0].length() - 1] == '\n') codexec[0].pop_back();
		if (ind >= fun_indent) {
			string s = codestream[i];
			getIndent(s, fun_indent);
			curfun += s;
			curfun += '\n';
		}
		else {
			if (cfname.length()) {
				newenv.set_global(cfname, curfun);
				newenv.set_global(cfname + ".__type__", "function");
				newenv.set_global(cfname + ".__arg__", cfargs);
			}

			cfname = "";
			curfun = "";
			fun_indent = max_indent;
			cfargs = "";
			switch (ind) {
			case 0:
				// Certainly getting out
				curclass = "";
				/*
				class [name]:
					init:
						...
					function [name] [arg ...]:
						...
				*/
				if (codexec[0] == "class") {
					parameter_check(2);
					if (codexec[1][codexec[1].length() - 1] == '\n') codexec[1].pop_back();
					codexec[1].pop_back();	// ':'
					newenv.set_global(codexec[1] + ".__type__", "class");
					curclass = codexec[1] + ".";
				}
				else if (codexec[0] == "import") {
					parameter_check(2);
					vector<string> codexec2 = split(codestream[i], ' ', 1);
					FILE *f = fopen(codexec2[1].c_str(), "r");
					if (f != NULL) {
						while (!feof(f)) {
							fgets(buf1, 65536, f);
							codestream.push_back(buf1);
						}
					}
				}
				break;
			case 1:
				if (codexec[0] == "init:") {
					fun_indent = 2;
					cfname = curclass + "__init__";
				}/*
				else if (codexec[0] == "iterator:") {
					fun_indent = 2;
					cfname = curclass + "__iter__";
				}*/
				break;
			default:
				break;
			}
			if (codexec[0] == "function") {
				parameter_check(2);
				fun_indent = 1 + bool(curclass.length());
				if (codexec.size() >= 3) {
					if (codexec[2][codexec[2].length() - 1] == '\n') codexec[2].pop_back();
					codexec[2].pop_back(); // ':'
					cfargs = codexec[2];
				}
				else {
					cfargs = "";
					if (codexec[1][codexec[1].length() - 1] == '\n') codexec[1].pop_back();
					codexec[1].pop_back(); // ':'
				}
				cfname = curclass + codexec[1];
			}
		}
		
	}
	if (cfname.length()) {
		newenv.set_global(cfname, curfun);
		newenv.set_global(cfname + ".__type__", "function");
		newenv.set_global(cfname + ".__arg__", cfargs);
	}
	
	//return null;
	// End

	intValue res = run(code, newenv);

	// For debug propose
	//newenv.dump();
	return res;
}

int main(int argc, char* argv[]) {
	stdouth = GetStdHandle(STD_OUTPUT_HANDLE);

	// Test: Input code here:
#pragma region Compiler Test Option
	string code = "", file = "";
	in_debug = false;
#pragma endregion
	// End

	if (argc < 1) {
		cout << "Usage: " << argv[0] << " filename [options]";
		return 1;
	}
	if (!file.length() && !code.length()) {
		file = argv[1];
	}
#pragma region Read Options
	for (int i = 2; i < argc; i++) {
		string opt = argv[i];
		if (opt == "--debug") {
			in_debug = true;
		}
	}
#pragma endregion

	if (!code.length()) {
		FILE *f = fopen(file.c_str(), "r");
		if (f != NULL) {
			while (!feof(f)) {
				fgets(buf1, 65536, f);
				code += buf1;
			}
		}
	}
	// debug
	//cout << code << endl << "---" << endl << endl;
	// end

	if (in_debug) {
		begindout();
		cout << "Debug mode" << endl;
		string command = "";
		do {
			cout << "-> ";
			//cin >> command;
			getline(cin, command);
			vector<string> spl = split(command, ' ', 1);
			if (spl.size() <= 0) continue;
			if (spl[0] == "break") {
				dshell_check(2);
				breakpoints.insert(atoi(spl[1].c_str()));
			}
			else if (spl[0] == "bdel") {
				if (breakpoints.count(atoi(spl[1].c_str()))) breakpoints.erase(atoi(spl[1].c_str()));
			}
			else if (spl[0] == "blist") {
				for (auto &i : breakpoints) cout << i << " ";
				cout << endl;
			}
			else if (spl[0] == "quit") {
				exit(0);
			}
		} while (command != "run");
		endout();
	}

	return preRun(code).numeric;
}
