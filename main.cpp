#define _CRT_SECURE_NO_WARNINGS

// Converting long64 and double
#pragma warning(disable:4244)

#include <iostream>
#include <stack>
#include <map>
#include <vector>
#include <string>
#include <cstdio>
using namespace std;

char buf0[255], buf01[255];

// All the varmaps show one global variable space.
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

} null;

intValue getValue(string single_expr, varmap &vm) {
	if (single_expr == "null") return null;
	if (single_expr[0] == '"' && single_expr[single_expr.length() - 1] == '"') {
		return formatting(single_expr.substr(1, single_expr.length() - 2));
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

int priority(char op) {
	switch (op) {
	case ')':
		return 5;
		break;
	case ':':
		return 4;
		break;
	case '*': case '/': case '%':
		return 3;
		break;
	case '+': case '-':
		return 2;
		break;
	case '>': case '<': case '==':
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

// Please notice special meanings.
intValue primary_calcute(intValue first, char op, intValue second, varmap &vm) {
	switch (op) {
	case '(': case ')':
		break;
	case ':':
		// As for this, 'first' should be direct var-name
		return vm[first.str + ":" + second.str];
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
	case '==':
		if (first.isNumeric) {
			return first.numeric == second.numeric;
		}
		else {
			return first.str == second.str;
		}
		break;
	default:
		return null;
	}
}

intValue calcute(string expr, varmap &vm) {
	stack<char> op;
	stack<string> val;
	string operand = "";
	for (size_t i = 0; i < expr.length(); i++) {
		int my_pr = priority(expr[i]), op_pr = -2;
		if (my_pr >= 0) {
			// May check here.
			val.push(operand);
			while ((!op.empty()) && (op_pr = priority(op.top())) > my_pr) {
				intValue v1, v2;
				char mc = op.top();
				op.pop();
				if (mc == ':') {
					v1 = intValue(val.top());
				}
				else {
					v1 = getValue(val.top(), vm);
				}
				
				val.pop();
				v2 = getValue(val.top(), vm);
				val.pop();
				intValue pres = primary_calcute(v2, mc, v1, vm);
				val.push(pres.unformat());
			}
			op.push(expr[i]);
			operand = "";
		}
		else {
			operand += expr[i];
		}
	}
	if (operand.length()) val.push(operand);
	while (!op.empty()) {
		intValue v1 = getValue(val.top(), vm), v2;
		val.pop();
		v2 = getValue(val.top(), vm);
		val.pop();
		char mc = op.top();
		op.pop();
		intValue pres = primary_calcute(v2, mc, v1, vm);
		val.push(pres.unformat());
	}
	return getValue(val.top(), vm);
}

intValue run(string code, varmap &prevenv, int indent = 0, string this_mean = "") {
	varmap curenv;
	vector<string> codestream = split(code);
	size_t execptr = 0;
	map<size_t, size_t> jmptable;
	while (execptr < codestream.size()) {
		// To be filled ...
		execptr++;
	after_add_exp:;
	}
}

int main(int argc, char* argv[]) {
	// Test compments
	varmap vm;
	vm.push();
	vm.set_global("a");
	vm.set_global("b", "5");
	vm.set_global("c", "\"a\"");
	vm.set_global("d", "6");
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
	cout << endl << "===" << endl;
	calcute("\"a\"*3+\"bz\"*5", vm).output();	// It seems error right here.
	cout << endl;
	// End

	return 0;
}
