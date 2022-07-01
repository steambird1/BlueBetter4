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
using namespace std;

// Pre declare
class varmap;
struct intValue;
intValue run(string code, varmap &myenv);
intValue calcute(string expr, varmap &vm);

const int max_indent = 65536;

char buf0[255], buf01[255], buf1[65536];

// At most delete maxfetch.
int getIndent(string &str, int maxfetch = -1) {
	int id = 0;
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
		string& operator[](string key) {
#pragma region Debug Purpose
			//cout << "Require key: " << key << endl;
#pragma endregion
			// Find where it is
			if (key == "this") {
				return (*this_source)[this_name];
			}
			else if (key.substr(0, 5) == "this.") {
				vector<string> s = split(key, '.', 1);
				return (*this_source)[this_name + "." + s[1]];
			}
			else {
				for (vit i = vs.rbegin(); i != vs.rend(); i++) {
					if (i->count(key)) return ((*i))[key];
				}
				if (glob_vs.count(key)) return glob_vs[key];
				if (!vs[vs.size() - 1].count(key)) vs[vs.size() - 1][key] = "null";
				return vs[vs.size() - 1][key];
			}
			
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
private:
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
		if (fch == 'n' || fch == '-') {
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
		if (vm[dotspl[0] + ".__type__"] != "null" && vm[dotspl[0] + ".__type__"] != "function") {
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
			vector<string> argname = split(vm[spl[0] + ".__arg__"], ' ');
			vector<string> arg;
			vector<intValue> ares;
			if (spl.size() >= 2) {
				arg = split(spl[1], ',');
			}
			if (arg.size() < argname.size()) return null;
			for (size_t i = 0; i < arg.size(); i++) {
				nvm[argname[i]] = (calcute(arg[i], vm)).unformat();
			}
			if (set_this.length()) nvm.set_this(&vm, set_this);
			return run(vm[spl[0]], nvm);
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

intValue calcute(string expr, varmap &vm) {
	stack<char> op;
	stack<string> val;
	string operand = "";
	for (size_t i = 0; i < expr.length(); i++) {
		int my_pr = priority(expr[i]), op_pr = -2;
		if (my_pr >= 0) {
			if (expr[i] == '(') {
				// Here should be operator previously.
				op.push('(');
			}
			else if (expr[i] == ')') {
				if (operand.length()) val.push(operand);
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
				// May check here.
				if (operand.length()) val.push(operand);
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
	if (operand.length()) val.push(operand);
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

// This 'run' will skip ALL class and function declarations.
// Provided environment should be pushed.
intValue run(string code, varmap &myenv) {
	vector<string> codestream = split(code);
	size_t execptr = 0;
	map<size_t, size_t> jmptable;
	int prevind = 0;
	while (execptr < codestream.size()) {
		vector<string> codexec = split(codestream[execptr], ' ', 1);
		int ind = getIndent(codexec[0]);
		if (prevind < ind) myenv.push();
		else if (prevind > ind) myenv.pop();
		// To be filled ...
		if (codexec[0] == "class" || codexec[0] == "function") {
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
				vector<string> dasher = split(codexec2[0], ':');
				// calcute until the last.
				intValue final = calcute(dasher[dasher.size()-1],myenv);
				for (size_t i = dasher.size() - 2; i >= 1; i--) {
					final = calcute(dasher[i] + ":" + final.str,myenv);
				}
				codexec2[0] = dasher[0] + "." + final.str;
			}
			if (codexec2[1].length() > 4 && codexec2[1].substr(0, 4) == "new ") {
				vector<string> azer = split(codexec2[1], ' ');
				varmap vm;
				vm.push();
				vm.set_this(&myenv, codexec2[0]);
				myenv[codexec2[0]] = "null";
				myenv[codexec2[0] + ".__type__"] = azer[1];
				run(myenv[azer[1] + ".__init__"], vm);
			}
			else if (codexec2[1] == "input") {
				fgets(buf1, 65536, stdin);
				myenv[codexec2[0]] = intValue(buf1).unformat();
			}
			else if (codexec2[1].length() > 4 && codexec2[1].substr(0, 4) == "int ") {
				myenv[codexec2[0]] = atoi(calcute(codexec2[1], myenv).str.c_str());
			}
			else if (codexec2[1] == "input int") {
				//myenv[codexec2[0]]
				double dv;
				scanf("%lf", &dv);
				myenv[codexec2[0]] = intValue(dv).unformat();
			}
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
				while (eptr != codestream.size() - 1) {
					// End if indent equals and not 'elif' or 'else'.
					//if () break;
					string s = codestream[++eptr];
					int r = getIndent(s);
					vector<string> sp = split(s, ' ', 1);
					if (r == ind && sp[0] != "elif" && sp[0] != "else:") break;
				}
				jmptable[rptr] = eptr;
			}
			else {
				// Go on elif / else
				size_t eptr = execptr + 1;
				while (eptr != codestream.size() - 1) {
					// End if indent equals and not 'elif' or 'else'.
					string s = codestream[++eptr];
					int r = getIndent(s);
					vector<string> sp = split(s, ' ', 1);
					if (r == ind) break;
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
					if (r == ind) break;
				}
				jmptable[--eptr] = execptr;
			}
			else {
				while (getIndentRaw(codestream[++execptr]) != ind); 
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
			while (execptr != codestream.size() - 1) {
				execptr++;
				string s = codestream[execptr];
				int id = getIndent(s);
				vector<string> spl = split(s, ' ', 1);
				if (id < ind && (spl[0] == "while" || spl[0] == "for")) break;
				
			}
			goto after_add_exp;
		}
	add_exp: if (jmptable.count(execptr)) {
		execptr = jmptable[execptr];
	}
			 else {
		execptr++;
	}
		 after_add_exp: prevind = ind;
	}
	return null;
}

intValue preRun(string code) {
	// Should prepare functions for it.
	varmap newenv;
	newenv.push();
	vector<string> codestream = split(code);
	string curclass = "";					// Will append '.'
	string curfun = "", cfname = "", cfargs = "";
	int fun_indent = max_indent;
	for (size_t i = 0; i < codestream.size(); i++) {
		vector<string> codexec = split(codestream[i], ' ', 2);
		int ind = getIndent(codexec[0], 2);
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
					codexec[1].pop_back();	// ':'
					newenv.set_global(codexec[1] + ".__type__", "class");
					curclass = codexec[1] + ".";
				}
				break;
			case 1:
				if (codexec[0] == "init:") {
					fun_indent = 2;
					cfname = curclass + "__init__";
				}
				break;
			default:
				break;
			}
			if (codexec[0] == "function") {
				parameter_check(2);
				fun_indent = 1 + bool(curclass.length());
				if (codexec.size() >= 3) {
					codexec[2].pop_back(); // ':'
					cfargs = codexec[2];
				}
				else {
					cfargs = "";
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
	// Test
	//preRun("set a=5\nset a.5=4\nset a.4=3\nset a.3=2\nset a:a:a:a:a=1\nprint a.2");
	//preRun("set a=input\nprint a");
	string code = "set q=0\nwhile q<10:\n\tset q=q+1\n\tif q=7:\n\t\tbreak\n\tprint q\n\tprint \"\n\"\nprint \"OK\"";
	cout << code << endl;
	preRun(code);
	// End
	return 0;
}
