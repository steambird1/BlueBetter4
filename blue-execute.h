#pragma once
#include "blue-lib.h"
#include "blue-platform-adaptor.h"
#if defined(__linux__)
#include <dlfcn.h>
#endif

int removesFile(string filename) {
	remove(filename.c_str());
	return errno;
}

int makeDirectory(string dirname) {
	return mkdir(dirname.c_str());
}

int removeDirectory(string dirname) {
	return rmdir(dirname.c_str());
}

#pragma warning(disable:C4244)
// mydir should have '\\' in the end
// Will return something to serial unless __is_recesuive = true
// TODO: Change it into map<string, string> returns
// *** var_prefix REQUIRES '.' ***
map<string, intValue> yieldDirectory(string mydir, string matcher, string var_prefix, bool prohibit_recesuive = false, bool __is_recesuive = false) {
	fileinfo f;
	finder_handle handler = _findfirst((mydir + matcher).c_str(), &f);
	const intValue ftyper = intValue("file_info");
	map<string, intValue> result;
	if (handler < 0) {
		return { {".__has_error__", intValue(1)} };
	}
	do {
		string sfname = f.name;
		if (sfname == "." || sfname == "..") continue;
		//result += blue_fileinfo(f).serial(mydir);
		string hf = var_prefix + mydir + sfname;
		result[hf + ".__type__"] = ftyper;
		result[hf + ".attrib"] = intValue(f.attrib);
		result[hf + ".time_access"] = intValue(f.time_access);
		result[hf + ".time_write"] = intValue(f.time_write);
		result[hf + ".time_create"] = intValue(f.time_create);
		result[hf + ".size"] = intValue(f.size);
		if (f.attrib & _A_SUBDIR) {
			if (!prohibit_recesuive) {
				auto yres = yieldDirectory(mydir + sfname + '\\', matcher, var_prefix, false, true);
				result.insert(yres.begin(), yres.end());
			}
		}
	} while (!_findnext(handler, &f));
	_findclose(handler);
	return result;
}
#pragma warning(enable:C4244)


class interpreter {
public:

	//typedef intValue(*bcaller)(string, varmap&);
	using bcaller = function<intValue(string, varmap&)>;

	size_t getLength(int fid) {
		size_t cur = ftell(files[fid]);
		fseek(files[fid], 0, SEEK_END);
		size_t res = ftell(files[fid]);
		fseek(files[fid], cur, SEEK_SET);
		return res;
	}

	enum thread_status {
		unknown = -999,
		not_exist = -1,
		not_joinable = 0,
		joinable = 1
	};

	void generateClass(string variable, string classname, varmap &myenv, bool run_init = true) {
		myenv[variable] = null;
		myenv[variable + ".__type__"] = classname;
		if (run_init) {
			varmap vm;
			vm.push();
			vm.set_this(&myenv, variable);
			__spec++;
			string cini = classname + ".__init__";
			run(myenv[cini].str, vm, cini);
			__spec--;
		}
	}

	string curexp(string exp, varmap &myenv) {
		vector<string> dasher = split(exp, ':', 1);
		if (dasher.size() == 1) return exp;
		// Not the connection!
		return dasher[0] + "." + calculate(dasher[1], myenv).str;
	}

	inline string auto_curexp(string exp, varmap &myenv) {
		if (exp.find(':') != string::npos) {
			return curexp(exp, myenv);
		}
		else {
			return exp;
		}
	}

	void raiseError(intValue raiseValue, varmap &myenv, string source_function = "Unknown source", size_t source_line = 0, double error_id = 0, string error_desc = "") {
		if (source_function == "__error_handler__") {
			curlout();
			cout << "During processing error, another error occured:" << endl;
			cout << "Line: " << source_line << endl;
			cout << "Error #: " << error_id << endl;
			cout << "Description: " << error_desc << endl;
			cout << "System can't process error in error handler. System will quit." << endl;
			exit(1);
			endout();
			return;
		}
		myenv.set_global("err.value", raiseValue);
		myenv.set_global("err.source", intValue(source_function));
		myenv.set_global("err.line", intValue(source_line));
		myenv.set_global("err.id", intValue(error_id));
		myenv.set_global("err.description", intValue(error_desc));
		varmap emer_var;
		emer_var.push();
		run(myenv["__error_handler__"].str, emer_var, "__error_handler__");
	}

	// If save_quote, formatting() will not process anything inside quote.
	// If multithreading, run() will be async and the function WILL RETURN NULL (since I don't "promise").
	intValue getValue(string single_expr, varmap &vm, bool save_quote = false, int multithreading = -1) {
		if (single_expr == "null" || single_expr == "") return null;
		// Remove any '(' in front
		while (single_expr.length() && single_expr[0] == '(') {
			if (single_expr[single_expr.length() - 1] == ')') single_expr.pop_back();
			single_expr.erase(single_expr.begin());
		}
		if (single_expr[0] == '"' && single_expr[single_expr.length() - 1] == '"') {
			return formatting(single_expr.substr(1, single_expr.length() - 2), '\\', (save_quote ? '\"' : char(-1)));
		}
		// Is number?
		bool dot = false, isnum = true;
		double neg = 1;
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
			if (spl.size() && spl[0].length()) {
				// Internal expressions here ...
				if (spl[0][0] == '$') {
					spl[0] = vm[spl[0].substr(1)].str;
				}
				else if (spl[0] == "new") {
					// Object creation
					varmap tvm;
					tvm.push();
					generateClass("__temp_new", spl[1], tvm);
					return tvm["__temp_new"];
				}
				else if (spl[0] == "ishave") {
					if (spl.size() < 2) {
						raise_gv_ce("Parameter missing");
						return intValue(0);
					}
					// Remove () for them:
					while (spl[1].length() && spl[1][0] == '(') spl[1].erase(spl[1].begin());
					while (spl[1].length() && spl[1][spl[1].length() - 1] == ')') spl[1].pop_back();
					if (spl[1].find(':') != string::npos) {
						spl[1] = curexp(spl[1], vm);
					}
					return intValue(vm.count(spl[1]));
				}
				else if (spl[0] == "isref") {
					if (spl.size() < 2) {
						raise_gv_ce("Parameter missing");
						return intValue(0);
					}
					// Remove () for them:
					while (spl[1].length() && spl[1][0] == '(') spl[1].erase(spl[1].begin());
					while (spl[1].length() && spl[1][spl[1].length() - 1] == ')') spl[1].pop_back();
					if (spl[1].find(':') != string::npos) {
						spl[1] = curexp(spl[1], vm);
					}
					return intValue(vm.have_referrer(spl[1]));
				}
			}
			vector<string> dotspl = { "","" };
			size_t fl = spl[0].find_last_of('.');
			if (fl >= string::npos || fl + 1 >= spl[0].length()) {	// string::npos may overrides
				dotspl[0] = spl[0];
				dotspl.pop_back();
			}
			else {
				dotspl[0] = spl[0].substr(0, fl);
				dotspl[1] = spl[0].substr(fl + 1);
			}
			string set_this = "";
			bool set_no_this = false, is_static = false;
			bool class_obj = false;
			if (dotspl.size() > 1 && !vm[dotspl[0] + ".__type__"].isNull && vm[dotspl[0] + ".__type__"].str != "function") {
				class_obj = true;
				if (vm[dotspl[0] + ".__type__"].str == "class" && (vm[dotspl[0] + ".__shared__"].str == "1" || vm[spl[0] + ".__shared__"].str == "1")) {
					// Do nothing!
					set_no_this = true;
					is_static = true;
				}
				else {
					set_this = dotspl[0];

				}

			}
			bool is_func = false;
			if (class_obj) {
				//if (class_obj) spl[0] = vm[dotspl[0] + ".__type__"] + "." + dotspl[1];	// Call the list function.
				string header;
				if (is_static) {
					header = dotspl[0];
				}
				else {
					header = vm[dotspl[0] + ".__type__"].str;
				}
				string tmp = header + "." + dotspl[1];
				if (vm[tmp + ".__type__"].str == "function") {
					is_func = true;
					spl[0] = tmp;
				}
			}
			else {
				is_func = vm[spl[0] + ".__type__"].str == "function";
			}
			if (is_func) {
				// A function call.

				varmap nvm;
				nvm.push();
				nvm.transform_referrer_from("this", vm, "this");
				if (spl[0].find('.') != string::npos && (!set_no_this)) {
					vector<string> xspl = split(spl[0], '.');
					nvm.set_this(&vm, xspl[0]);
					xspl[0] = vm[spl[0] + ".__type__"].str;
				}
				string args = vm[spl[0] + ".__arg__"].str;
				string array_arg = "";
				static string pa_name = "...";	// Constants: No changes!
				static string dots = ".";
				vector<string> argname;	// Stores arguments that is required.
				vector<string> arg;	// Stores arguments that is provided.
				vector<intValue> ares;	// Stores values to deliver
				if (beginWith(args, pa_name)) {
					array_arg = args.substr(pa_name.length());
					generateClass(array_arg, "list", nvm);
				}
				else {
					argname = split(args, ' ');
				}
				if (args.length()) {
					// TODO: Test various function calls after that
					/*bool str = false, dmode = false;
					if (spl.size() >= 2) {	// Procees with arguments ...
						int quotes = 0;
						string tmp = "";
						for (auto &i : spl[1]) {
							if (i == '(' && (!str)) {
								if (quotes) tmp += i;
								quotes++;
							}
							else if (i == ')' && (!str)) {
								quotes--;
								if (quotes) tmp += i;
							}
							else if (i == '\\' && (!dmode)) {
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
						if (spl.size() >= 2) arg.push_back(spl[1]);
					}*/
					arg = parameterSplit(spl[1]);
					if (!array_arg.length()) {
						if (arg.size() != argname.size()) {
							if (argname.size() && arg.size() == argname.size() - 1) {
								arg.push_back("null");	 // Placeholder
							}
							else {
								raise_gv_ce(string("Warning: Parameter dismatches or function does not exist while calling function ") + spl[0]);
								return null;
							}
						}
					}

					for (size_t i = 0; i < arg.size(); i++) {
						// Special character '^' will pass the referer!
						if (arg[i].length() && arg[i][0] == '^') {
							string rname = auto_curexp(arg[i].substr(1), vm);
							if (vm.have_referrer(rname)) {
								if (array_arg.length()) {
									nvm.transform_referrer_from(array_arg + dots + to_string(i), vm, rname);
								}
								else {
									nvm.transform_referrer_from(argname[i], vm, rname);
								}
								continue;
							}
							else if (vm.count(rname)) {
								if (array_arg.length()) {
									nvm.set_referrer(array_arg + dots + to_string(i), rname, &vm);
								}
								else {
									nvm.set_referrer(argname[i], rname, &vm);
								}
								continue;
							}
							else {
								// Continue processor and attempt to see it as normal formula
								arg[i].erase(arg[i].begin());
								raise_gv_ce(string("Warning: Not an acceptable referrer: ") + arg[i]);
							}
						}
						auto re = calculate(arg[i], vm);
						if (re.isObject) {
							// Passing ByVal, automaticly deserial
							if (array_arg.length()) {
								nvm.deserial(array_arg + dots + to_string(i), re.serial_data);
							}
							else {
								nvm.deserial(argname[i], re.serial_data);
							}
						}
						else {
							if (array_arg.length()) {
								nvm[array_arg + dots + to_string(i)] = re;
							}
							else {
								nvm[argname[i]] = re;
							}
						}
					}
				}
				if (array_arg.length()) {
					int external = 0;
					if (single_expr[single_expr.length() - 1] == ',') {
						nvm[array_arg + dots + to_string(arg.size())] = null;
						external = 1;
					}
					nvm[array_arg + dots + "_length"] = intValue(arg.size() + external);
				}
				if (set_this.length()) nvm.set_this(&vm, set_this);
				if (set_no_this) {
					nvm["__is_sharing__"].set_str("1");
				}
				string s = vm[spl[0]].str;
				if (vm[spl[0]].isNull || s.length() == 0) {
					raise_gv_ce(string("Warning: Call of null function ") + spl[0]);
				}
				if (multithreading >= 0) {
					__spec++;
					thread_table[multithreading] = thread([this](string s, varmap nvm, string spls) {run(s, nvm, spls); }, s, nvm, spl[0]);	// CAN'T WRITE THIS
					thread_table[multithreading].detach();
					__spec--;
					return null;
				}
				else {
					__spec++;
					auto r = run(s, nvm, spl[0]);
					__spec--;
					if (r.isNumeric && neg < 0) {
						r = intValue(-r.numeric);
					}
					return r;
				}

			}
			else {
				auto r = vm[spl[0]];
				if (r.isNumeric && neg < 0) {
					r = intValue(-r.numeric);
				}
				return r;
			}
			// So you have refrences
		}
	}

	// Single ones must have the same as multi ones.
	// extras: Indicate if it work as extra operator (used in 'set', 'global' and maybe 'preset').
	int priority(char op, bool extras = false) {
		switch (op) {
		case ')':
			if (extras) return -1;
		case ':':	// PASSTHROUGH
			// Must make sure ':' has the most priority
			return 7;
			break;
		case '#':
			return 6;
			break;
		case '&': case '|': case '^':
			return 5;
			break;
		case '*': case '/': case '%':
			return 4;
			break;
		case '+': case '-':
			return 3;
			break;
		case '>': case '<': case '=':	// >=, <= stay the same
			return 2;
			break;
		case '(':
			return 0;
			break;
		case '.':
			return -2;				// Special non-operator dealling by calculate()
			break;
		default:
			return -1;
		}
	}

	int priority(string op, bool extras = false) {
		if (!op.length()) return -1;
		return priority(op[0], extras);
	}

	typedef long long							long64;
	typedef unsigned long long					ulong64;

	// Please notice special meanings.
	intValue primary_calculate(intValue first, string op, intValue second, varmap &vm) {
		if (!op.length()) return null;	// Can't be.
		switch (op[0]) {
#define if_have_additional_op(oper) if (op.length() >= 2 && op[1] == oper)
		case '(': case ')':
			break;
		case ':':
			// As for this, 'first' should be direct var-name
			return vm[first.str + "." + second.str];
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
			if (second.numeric == 0) {
				return null;
			}
			return long64(first.numeric) % long64(second.numeric);
			break;
		case '/':
			if (second.numeric == 0) {
				return null;
			}
			return first.numeric / second.numeric;
			break;
		case '+':
			if (first.isNumeric && second.isNumeric) {
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
			// Judge >= or >>
			if_have_additional_op('=') {
				if (first.isNumeric) {
					return first.numeric >= second.numeric;
				}
				else {
					return first.str >= second.str;
				}
			}
			else if_have_additional_op('>') {
				if (first.isNumeric && second.isNumeric) {
					return ulong64(first.numeric) >> ulong64(second.numeric);
				}
				else {
					return null;
				}
			}
			else {
				if (first.isNumeric) {
					return first.numeric > second.numeric;
				}
				else {
					return first.str > second.str;
				}
			}
			break;
		case '<':
			// Judge <=, <> or <<
			if_have_additional_op('=') {
				if (first.isNumeric) {
					return first.numeric <= second.numeric;
				}
				else {
					return first.str <= second.str;
				}
			}
else if_have_additional_op('<') {
			if (first.isNumeric && second.isNumeric) {
				return ulong64(first.numeric) << ulong64(second.numeric);
			}
			else {
				return null;
			}
		}
		else if_have_additional_op('>') {
			// Not equal
			if (first.isNumeric) {
				return first.numeric != second.numeric;
			}
			else {
				return first.str != second.str;
			}
		}
		else {
		if (first.isNumeric) {
			return first.numeric < second.numeric;
		}
		else {
			return first.str < second.str;
		}
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
			// Judge &&
			if_have_additional_op('&') {
				if (first.isNumeric && second.isNumeric) {
					return ulong64(first.numeric) && ulong64(second.numeric);
				}
			}
			else {
				if (first.isNumeric && second.isNumeric) {
					return ulong64(first.numeric) & ulong64(second.numeric);
				}
			}
			if (first.isNull || second.isNull) return 0;
			if (first.str.length() == 0 || second.str.length() == 0) return 0;
			return 1;
			break;
		case '|':
			// Judge ||
			if_have_additional_op('|') {
				if (first.isNumeric && second.isNumeric) {
					return ulong64(first.numeric) || ulong64(second.numeric);
				}
			}
			else {
				if (first.isNumeric && second.isNumeric) {
					return ulong64(first.numeric) | ulong64(second.numeric);
				}
			}
			if (first.isNull && second.isNull) return 0;
			if (first.str.length() == 0 && second.str.length() == 0) return 0;
			return 1;
			break;
		case '^':
			if (first.isNumeric && second.isNumeric) {
				return ulong64(first.numeric) ^ ulong64(second.numeric);
			}
			else {
				bool first_bool = true, second_bool = true;
				if (first.isNull) first_bool = false;
				else if (first.str.length() == 0) first_bool = false;

				if (second.isNull) second_bool = false;
				else if (second.str.length() == 0) second_bool = false;

				return first_bool ^ second_bool;
			}
		default:
			raiseError(intValue("Unknown operator"), vm, "Runtime");
			return null;
		}
#undef if_have_additional_op
	}

	intValue calculate(string expr, varmap &vm) {
		if (expr.length() == 0) return null;
		stack<string> op;
		stack<intValue> val;
		string operand = "";
		bool cur_neg = false, qmode = false, dmode = false;
		int ignore = 0, op_pr;

		// Making tempatory environment
		vm.push();

		auto auto_push = [&]() {
			if (cur_neg) {
				val.push(getValue(operand, vm).negative());
			}
			else {
				val.push(getValue(operand, vm));
			}
			cur_neg = false;
		};

		// my_pr not provided (-1): Keep on poping
		auto auto_pop = [&](int my_pr = -1) {
			op_pr = -2;
			while ((!op.empty()) && (op_pr = priority(op.top())) >= my_pr) {	 // Therefore we changes right-to-left to left-to-right
				intValue v1, v2;
				string mc = op.top();
				op.pop();
				v1 = val.top();


				val.pop();
				v2 = val.top();
				val.pop();
				intValue pres = primary_calculate(v2, mc, v1, vm);
				val.push(pres);
			}
		};

		for (size_t i = 0; i < expr.length(); i++) {
			if (expr[i] == '"' && (!dmode)) qmode = !qmode;
			if (expr[i] == '\\' && (!dmode)) dmode = true;
			else dmode = false;
			int my_pr = priority(expr[i]);
			// Special processor for function call with '^'
			if (expr[i] == '^' && (i == 0 || expr[i - 1] == '(' || expr[i - 1] == ',' || expr[i - 1] == ' ')) {
				my_pr = -1;	// Not to regard it as an operator
			}
			if (my_pr >= 0 && (!qmode) && (!dmode)) {
				if (expr[i] == '(') {
					// Here should be operator previously.
					int t = 1;
					while (int(i) - t > 0 && expr[i - t] == '(') t++;
					if ((i == 0 || priority(expr[i - t]) >= 0) && (!ignore)) {
						op.push("(");
					}
					else {
						ignore++;
						operand += expr[i];
					}
				}
				else if (expr[i] == ')') {
					if (ignore <= 0) {
						if (operand.length()) {
							auto_push();
						}
						while ((!op.empty()) && (op.top() != "(")) {
							intValue v1, v2;
							string mc = op.top();
							op.pop();

							v1 = val.top();

							val.pop();
							v2 = val.top();

							val.pop();
							intValue pres = primary_calculate(v2, mc, v1, vm);
							val.push(pres);
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
						if (expr[i] == ':') {
							if (i > 0 && expr[i - 1] == ')') {
								// Getting the value from the stack
								auto obj = val.top();
								if (obj.isObject) {
									val.pop();
									string gen = vm.generate();
									vm.deserial(gen, obj.serial_data);
									val.push(intValue(gen));
								}
								else {
									raise_gv_ce("Cannot get a member from non-object thing");
								}

							}
							else if (operand.length()) {
								val.push(intValue(operand));
							}
							else {
								raise_gv_ce("Cannot use ':' without any object");
							}
							cur_neg = false;
						}
						else if (operand.length()) {	// Can't be things like || or &&
							auto_push();
							cur_neg = false;
						}	// Dealing with connecting operators like ||, &&.
						else if (!op.empty()) {	// Can't have something like &a& -> a&&
							if (i > 0 && priority(expr[i - 1]) >= 0) {
								switch (expr[i - 1]) {
								case '&':
									if (expr[i] == '&') {
										op.pop();
										op.push("&&");
										goto end_of_pusher;
									}
									break;
								case '|':
									if (expr[i] == '|') {
										op.pop();
										op.push("||");
										goto end_of_pusher;
									}
									break;
								case '<':
									if (expr[i] == '>') {
										op.pop();
										op.push("<>");
										goto end_of_pusher;
									}
									// PASSTHROUGH FOR << or <=
								case '>':
									if (expr[i] == '=' || expr[i] == expr[i - 1]) {
										op.pop();
										op.push({ expr[i - 1], expr[i] });
										goto end_of_pusher;
									}
									break;
								default:
									break;
								}
							}
						}
						auto_pop(my_pr);
						op.push(string({ expr[i] }));
						operand = "";
					end_of_pusher:;
					}
				}
				else {
					operand += expr[i];
				}

			}
			else if (i > 0 && expr[i] == '.' && expr[i - 1] == ')') {
				// Should have operand clear. Dealling with non-operator '.'
				auto obj = val.top();
				if (obj.isObject) {
					val.pop();
					string gen = vm.generate();
					vm.deserial(gen, obj.serial_data);
					operand += gen + '.';
				}
				else {
					raise_gv_ce("Cannot get a member from non-object thing");
				}
			}
			else {
				operand += expr[i];
			}
		}
		if (operand.length()) {
			auto_push();
			cur_neg = false;
		}
		auto_pop();
		vm.pop();			// Reverting
		return val.top();
	}

#define parameter_check(req) do {if (codexec.size() < req) {raise_ce("Error: required parameter not given") ;return null;}} while (false)
#define parameter_check2(req,ext) do {if (codexec2.size() < req) {raise_ce(string("Error: required parameter not given in sub command ") + ext); return null;}} while (false)
#define parameter_check3(req,ext) do {if (codexec3.size() < req) {raise_ce(string("Error: required parameter not given in sub command ") + ext); return null;}} while (false)
#define dshell_check(req) do {if (spl.size() < req) {cout << "Bad command" << endl; goto dend;}} while (false)

	intValue run(string code, varmap &myenv, string fname) {
		vector<string> codestream = split(code, '\n', -1, '\"', '\\', true);
		// Process codestream before run
		for (auto &cep : codestream) {
			while (cep.length() && haveContent(cep) && cep[cep.length() - 1] == '\t') cep.pop_back();
		}
		size_t execptr = 0;
		jumpertable jmptable;
		int prevind = 0;
		bool noroll = false;
		while (execptr < codestream.size()) {
#pragma region User Debugger
			auto debugcall = [&]() {
				begindout();
				__spec = 0;
				cout << "-> Pre-execute" << endl;
				curlout();
				printf("%03ld > ", execptr);
				cout << codestream[execptr] << endl;
				begindout();
				string command = "";
				do {
					cout << "-> ";
					getline(cin, command);
					vector<string> spl = split(command, ' ', 1);
					if (spl.size() <= 0) continue;
					if (spl[0] == "current") {
						//cout << "Current line:\n" << codestream[execptr] << endl;
						cout << "Current program:\n";
						for (size_t i = 0; i < codestream.size(); i++) {
							if (i == execptr) curlout();
							printf("%03ld%c  ", i, i == execptr ? '*' : ' ');
							begindout();
							cout << codestream[i] << endl;
						}
					}
					else if (spl[0] == "quit") {
						exit(0);
					}
					else if (spl[0] == "view") {
						dshell_check(2);
						cout << spl[1] << " = ";
						calculate(spl[1], myenv).output();
						cout << endl;
					}
					else if (spl[0] == "exec") {
						dshell_check(2);
						specialout();
						__spec++;
						run(spl[1], myenv, "Debugger");
						__spec--;
						begindout();
					}
					else if (spl[0] == "nextp") {
						np = true;
						break;
					}
					else if (spl[0] == "nexts") {
						np = true;
						spec_ovrd = true;
						break;
					}
				dend:;
				} while (command != "run");
				endout();
			};
#pragma endregion
			if (((!__spec) || spec_ovrd) && np) {
				np = false;
				spec_ovrd = false;
				debugcall();
			}
			string &cep = codestream[execptr];
			//while (cep.length() && cep[0] != '\t' && cep[cep.length() - 1] == '\t') cep.pop_back();	// Processed
			vector<string> codexec = split(cep, ' ', 1);
			if (codexec.size() && codexec[0][0] == '\n') codexec[0].erase(codexec[0].begin()); // LF, why?
			int ind = getIndent(codexec[0]);
			jmptable.revert_all(execptr);
			noroll = false;
			while (prevind < ind) {
				myenv.push();
				prevind++;
			}
			while (prevind > ind) {
				myenv.pop();
				prevind--;
			}
			string prerun = cep;
			getIndent(prerun);	//?
			if (codexec.size() <= 0 || codexec[0][0] == '#') goto add_exp;	// Be proceed as command
			else if (codexec[0] == "class" || codexec[0] == "function" || codexec[0] == "property" || codexec[0] == "error_handler:") {
				string s = "";
				int ind;
				do {
					if (execptr >= codestream.size() - 1) {
						execptr = codestream.size();
						goto after_add_exp;
					}
					s = codestream[++execptr];
					ind = getIndent(s);
				} while (ind > 0);
				goto after_add_exp;
			}
			else if (codexec[0] == "print") {
				parameter_check(2);
				auto res = calculate(codexec[1], myenv);
				if (res.isObject) {
					res.output();
				}
				else {
					cout << res.str;
				}
				
			}
			else if (codexec[0] == "return") {
				if (codexec.size() < 2) return null;
				return calculate(codexec[1], myenv);
			}
			else if (codexec[0] == "raise") {
				parameter_check(2);
				raiseError(calculate(codexec[1], myenv), myenv, fname, execptr + 1, -1, "User define error");
			}
			else if (codexec[0] == "set" || codexec[0] == "declare") {
				static const string const_sign = "const ";	// set a=const ...
				parameter_check(2);
				vector<string> codexec2 = split(codexec[1], '=', 1);
				parameter_check2(2, codexec[0]);
				string external_op = "", &czero = codexec2[0];
				bool constant = false;
				if (beginWith(codexec2[1], const_sign)) {
					constant = true;
					codexec2[1] = codexec2[1].substr(const_sign.length());
				}
				char det;
				// Only 2 layers' detect, reversely
				if (czero.length() >= 2 && priority(det = czero[czero.length() - 1], true) > 0) {
					external_op = det;
					czero.pop_back();
					if (czero.length() >= 2 && priority(det = czero[czero.length() - 1], true) > 0) {
						external_op = det + external_op;
						czero.pop_back();
					}
				}
				parameter_check2(2, "set");
				if (codexec2[0][0] == '$') {
					codexec2[0].erase(codexec2[0].begin());
					codexec2[0] = calculate(codexec2[0], myenv).str;
				}
				else if (codexec2[0].find(":") != string::npos) {
					codexec2[0] = curexp(codexec2[0], myenv);
				}
				// End of resolver
				if (codexec2[0].find(".__const__") != string::npos || myenv[codexec2[0] + ".__const__"].str == "1") {
					raise_ce(string("Cannot set a value of constant: ") + codexec2[0]);
					goto add_exp;
				}
				if (constant) {
					myenv[codexec2[0] + ".__const__"] = intValue("1");
				}
				if (codexec[0] == "declare") {
					myenv.declare(codexec2[0]);
				}
				if (codexec2[1].length() > 4 && codexec2[1].substr(0, 4) == "new ") {
					vector<string> azer = split(codexec2[1], ' ');	// Classname is azer[1]
					if (myenv[azer[1] + ".__must_inherit__"].str == "1" || myenv[azer[1] + ".__shared__"].str == "1") {
						raise_ce(string("Warning: class ") + azer[1] + " is not allowed to create object.");
					}
					else {
						generateClass(codexec2[0], azer[1], myenv);
					}

				}
				else if (beginWith(codexec2[1], "object ")) {
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					parameter_check3(2, "Operator number");
					// WARNING: WILL NOT CALCUTE ANYMORE
					myenv.traditional_deserial(codexec2[0], getValue(codexec3[1], myenv, true).str);
				}
				else if (beginWith(codexec2[1], "serial ")) {
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					parameter_check3(2, "Operator number");
					myenv[codexec2[0]] = myenv.traditional_serial(codexec3[1]);
				}
				else if (codexec2[1] == "clear" || codexec2[1] == "null") {
					myenv.tree_clean(codexec2[0]);
				}
				else if (beginWith(codexec2[1], "referof ")) {
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					parameter_check3(2, "Operator number");
					if (codexec3[1].find(':') != string::npos) {
						codexec3[1] = curexp(codexec3[1], myenv);
					}
					myenv.set_referrer(codexec2[0], codexec3[1], &myenv);
				}
				else if (beginWith(codexec2[1], "copyof")) {
					// Copy referrer (directly from its info) or copy value. Therefore, something like 'list' should be changed.
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					parameter_check3(2, "Operator number");
					if (codexec3[1].find(':') != string::npos) {
						codexec3[1] = curexp(codexec3[1], myenv);
					}
					if (myenv.have_referrer(codexec3[1])) {
						myenv.transform_referrer_from(codexec2[0], myenv, codexec3[1]);
					}
					else {
						//myenv[codexec2[0]] = myenv[codexec3[1]];
						auto &res = myenv[codexec3[1]];
						if (res.isObject) {
							myenv.deserial(codexec2[0], res.serial_data);
						}
						else {
							myenv[codexec2[0]] = res;
						}
					}
				}
#pragma region Internal Calcutions
				else if (codexec2[1] == "__input") {
					string t;
					getline(cin, t);
					myenv[codexec2[0]] = intValue(t);
				}
				else if (beginWith(codexec2[1], "__int ")) {
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					parameter_check3(2, "Operator number");
					myenv[codexec2[0]] = intValue(atof(calculate(codexec3[1], myenv).str.c_str()));
				}
				else if (beginWith(codexec2[1], "__chr ")) {
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					parameter_check3(2, "Operator number");
					char ch = char(int(calculate(codexec3[1], myenv).numeric));
					myenv[codexec2[0]] = intValue(string({ ch }));
				}
				else if (beginWith(codexec2[1], "__ord ")) {
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					parameter_check3(2, "Operator number");
					intValue cv = calculate(codexec3[1], myenv);
					if (cv.str.length()) {
						myenv[codexec2[0]] = intValue(int(char((cv.str[0]))));
					}
					else {
						myenv[codexec2[0]] = intValue(-1);
					}

				}
				else if (beginWith(codexec2[1], "__len")) {
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					intValue rsz = calculate(codexec3[1], myenv);
					myenv[codexec2[0]] = intValue(rsz.str.length());
				}
				else if (beginWith(codexec2[1], "__intg ")) {
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					intValue rsz = calculate(codexec3[1], myenv);
					int res = int(rsz.numeric);
					myenv[codexec2[0]] = intValue(res);
				}
				else if (codexec2[1] == "__input_int") {
					//myenv[codexec2[0]]
					double dv;
					scanf("%lf", &dv);
					myenv[codexec2[0]] = intValue(dv);
				}
				else if (codexec2[1] == "__char_input") {
					char cv;
					scanf("%c", &cv);
					myenv[codexec2[0]] = intValue(int(cv));
				}
				else if (codexec2[1] == "__str_input") {
					string strv;
					cin >> strv;
					myenv[codexec2[0]] = intValue(strv);
				}
				else if (codexec2[1] == "__time") {
					myenv[codexec2[0]] = intValue(time(NULL));
				}
				else if (codexec2[1] == "__clock") {
					myenv[codexec2[0]] = intValue(clock());
				}
				else if (codexec2[1] == "__random") {
					myenv[codexec2[0]] = intValue(random());
				}
				else if (beginWith(codexec2[1], "__is_int")) {
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					intValue rsz = calculate(codexec3[1], myenv);
					myenv[codexec2[0]] = intValue(rsz.isNumeric);
				}
				else if (beginWith(codexec2[1], "__typeof")) {	// Not necessary any longer
					raise_ce("__typeof is deprecated. You shouldn't use it anymore. Please use obj.__type__ or typeof obj.");
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					parameter_check3(2, "Operator number");
					if (codexec3[1].find(':') != string::npos) {
						codexec3[1] = curexp(codexec3[1], myenv);
					}
					myenv[codexec2[0]] = myenv[codexec3[1] + ".__type__"];
				}
				else if (beginWith(codexec2[1], "__inheritanceof")) {
					// set a=inheritanceof b,c (a = 0 or 1)
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					parameter_check3(2, "Operator number");
					vector<string> codexec4 = split(codexec3[1], ',', 1);
					if (codexec4.size() < 2) {
						myenv[codexec3[1]] = intValue(0);
					}
					else {
						for (auto &i : codexec4) {
							if (i.find(':') != string::npos) {
								i = curexp(i, myenv);
							}
						}
					}
					if (myenv[codexec4[0] + ".__type__"].isNull || myenv[codexec4[0] + ".__type__"].str == "class") {
						myenv[codexec2[0]] = intValue(inh_map.is_same(codexec4[0], codexec4[1]));
					}
					else {
						if (inh_map.is_same(myenv[codexec4[0] + ".__type__"].str, myenv[codexec4[1] + ".__type__"].str)) {
							myenv[codexec2[0]] = intValue(1);
						}
						else {
							myenv[codexec2[0]] = intValue(0);
						}
					}
				}
				else if (beginWith(codexec2[1], "__membersof")) {
					vector<string> codexec3 = split(codexec2[1], ' ', 1);
					parameter_check3(2, "Operator number");
					if (codexec3[1].find(':') != string::npos) {
						codexec3[1] = curexp(codexec3[1], myenv);
					}
					auto result = myenv.get_member(codexec3[1]);
					generateClass(codexec2[0], "list", myenv);
					for (size_t i = 0; i < result.size(); i++) {
						myenv[codexec2[0] + "." + to_string(i)] = intValue(result[i]);
					}
					myenv[codexec2[0] + "._length"] = intValue(result.size());	// According to updated property
				}
#pragma endregion
				else {
					auto res = calculate(codexec2[1], myenv);
					// Turn to origin setter
					// If use class, head_part will be 'this'.
					string osetter = codexec2[0], head_part = codexec2[0], tail_part = "";
					bool using_class = false;

					// Always split like [a.b].[c] (c: Member, a.b: Father, to find class)
					auto spoint = codexec2[0].rfind('.');
					if (spoint != string::npos) {
						head_part = codexec2[0].substr(0, spoint);
						tail_part = codexec2[0].substr(spoint + 1);	// Both of them don't have '.'
						string htype = myenv[head_part + ".__type__"].str;
						if (!myenv.unserial.count(htype)) {
							using_class = true;
							osetter = htype + '.' + tail_part;
						}
					}

					if (myenv[osetter + ".__is_prop__"].str == "1") {
						// Is property, call function setter
						varmap nvm, ngvm;
						nvm.push();
						ngvm.push();
						if (using_class) {
							nvm.set_this(&myenv, head_part);
							ngvm.set_this(&myenv, head_part);
						}

						auto __res_dealer = [&nvm, &res]() {
							if (res.isObject) {
								nvm.deserial("value", res.serial_data);
							}
							else {
								nvm["value"] = res;
							}
						};

						__res_dealer();
						if (external_op.length()) {

							auto original_value = run(myenv[osetter].str, ngvm, string("Property Get:") + codexec2[0] + " while setting");
							res = primary_calculate(original_value, external_op, res, ngvm);	//?
							__res_dealer();
						}
						run(myenv[osetter + ".__setter__"].str, nvm, string("Property Set:") + codexec2[0]);

						/*varmap nvm;
						nvm.push();
						string tname = myenv.generate();
						myenv.declare(tname);

						auto __res_dealer = [&]() {
							if (res.isObject) {
								myenv.deserial(tname, res.str);
							}
							else {
								myenv[tname] = res;
							}
						};

						__res_dealer();
						// Get the getter if required
						if (external_op.length()) {
							// Property get doesn't need it. Isolate.
							auto original_value = run(myenv[osetter].str, nvm, string("Property Get:") + codexec2[0] + " while setting");
							res = primary_calculate(original_value, external_op, res, nvm);	//?
							__res_dealer();
						}
						calculate(osetter + ".__setter__ " + tname, myenv);	// Will directly set
						*/
					}
					else if (res.isObject) {
						if (external_op.length()) {
							raise_ce("Warning: using operators like +=, -=, *= for object is meaningless");
						}
						myenv.deserial(codexec2[0], res.serial_data);
					}
					else if (external_op.length()) {
						myenv[codexec2[0]] = primary_calculate(myenv[codexec2[0]], external_op, res, myenv);
					}
					else {
						myenv[codexec2[0]] = res;
					}
				}

			}
			else if (codexec[0] == "setstr") {
				/*
				setstr a#pos[+]=...
				*/
				// Just like 'set'.
				parameter_check(2);
				vector<string> codexec2_origin = split(codexec[1], '=', 1);
				vector<string> codexec2;
				int quotes = 0;
				string tmp = "";
				for (auto &i : codexec2_origin[0]) {
					switch (i) {
					case '(':
						quotes++;
						break;
					case ')':
						quotes--;
						break;
					case '#':
						if (!quotes) {
							codexec2.push_back(tmp);
							tmp = "";
						}
						break;
					default:
						tmp += i;
					}
				}
				if (!tmp.length()) {
					raise_ce("Bad parameter for setstr");
					goto add_exp;
				}
				bool do_insert = false;
				if (tmp[tmp.length() - 1] == '+') {
					do_insert = true;
					tmp.pop_back();
				}
				codexec2.push_back(tmp);
				if (codexec2[0][0] == '$') {
					codexec2[0].erase(codexec2[0].begin());
					codexec2[0] = calculate(codexec2[0], myenv).str;
				}
				else if (codexec2[0].find(":") != string::npos) {
					codexec2[0] = curexp(codexec2[0], myenv);
				}
				if (codexec2[0].find(".__const__") != string::npos || myenv[codexec2[0] + ".__const__"].str == "1") {
					raise_ce(string("Cannot set a value of constant: ") + codexec2[0]);
					goto add_exp;
				}
				auto &intv = myenv[codexec2[0]];
				if (intv.isNull) {
					intv.set_str("");
				}
				else if (intv.isNumeric) {
					intv.set_str(intv.str);
				}
				size_t dpos = codexec2[1].find('~');
				int pos, epos;
				if (dpos != string::npos && (!do_insert)) {		// If insert it won't work
					vector<string> codexec3 = split(codexec2[1], '~', 1);
					pos = calculate(codexec3[0], myenv).numeric;
					epos = calculate(codexec3[1], myenv).numeric;
				}
				else {
					pos = calculate(codexec2[1], myenv).numeric;
					epos = pos;
				}
				if (intv.str.length() <= pos) {
					raise_ce("Index out of range");
				}	// can't use intv since it will be changed soon
				auto cres = calculate(codexec2_origin[1], myenv).str;
				if (!do_insert) myenv[codexec2[0]].str.erase(myenv[codexec2[0]].str.begin() + pos, myenv[codexec2[0]].str.begin() + epos + 1);
				// Can we?
				myenv[codexec2[0]].str.insert(pos, cres);
				//intv.str[pos] = calculate(codexec2_origin[1], myenv).str[0];
			}
			else if (codexec[0] == "global") {
				bool constant = false;
				parameter_check(2);
				vector<string> codexec2 = split(codexec[1], '=', 1);	// May be global a=const ...
				parameter_check2(2, "set");
				if (codexec2[0][0] == '$') {
					codexec2[0].erase(codexec2[0].begin());
					codexec2[0] = calculate(codexec2[0], myenv).str;
				}
				else if (codexec2[0].find(":") != string::npos) {
					codexec2[0] = curexp(codexec2[0], myenv);
				}
				if (codexec2[0].find(".__const__") != string::npos || myenv[codexec2[0] + ".__const__"].str == "1") {
					raise_ce(string("Cannot set a value of constant: ") + codexec2[0]);
					goto add_exp;
				}
				static const string const_sign = "const ";	// set a=const ...
				if (beginWith(codexec2[1], const_sign)) {
					constant = true;
					codexec2[1] = codexec2[1].substr(const_sign.length());
				}
				char det;
				string external_op = "", &czero = codexec2[0];
				// Only 2 layers' detect, reversely
				if (czero.length() >= 2 && priority(det = czero[czero.length() - 1], true) > 0) {
					external_op = det;
					czero.pop_back();
					if (czero.length() >= 2 && priority(det = czero[czero.length() - 1], true) > 0) {
						external_op = det + external_op;
						czero.pop_back();
					}
				}
				// Should be like:
				// myenv[codexec2[0]] = primary_calculate(myenv[codexec2[0]], external_op, res, myenv);
				if (codexec2[0].find(".__const__") != string::npos || myenv[codexec2[0] + ".__const__"].str == "1") {
					raise_ce(string("Cannot set a value of constant: ") + codexec2[0]);
					return null;
				}
				auto res = calculate(codexec2[1], myenv);
				if (res.isObject) {
					if (external_op.length()) {
						raise_ce("Warning: using operators like +=, -=, *= for object is meaningless");
					}
					myenv.global_deserial(codexec2[0], res.serial_data);
					if (constant) {
						myenv.set_global(codexec2[0] + ".__const__", intValue("1"));
					}
				}
				else if (external_op.length()) {
					myenv.set_global(codexec2[0], primary_calculate(myenv.get_global(codexec2[0]), external_op, res, myenv), constant);
				}
				else {
					myenv.set_global(codexec2[0], res, constant);
				}
			}
			else if (codexec[0] == "if" || codexec[0] == "elif") {
				parameter_check(2);
				// Certainly you have ':'
				if (codexec[1].length()) codexec[1].pop_back();
				if (calculate(codexec[1], myenv).boolean()) {
					// True, go on execution, and set jumper to the end of else before any else / elif
					size_t rptr = execptr;								// Where our code ends
					while (rptr < codestream.size() - 1 && getIndentRaw(codestream[++rptr]) > ind);	// Go on until aligned else / elif
					//if (getIndentRaw(codestream[rptr]) != ind) rptr = codestream.size() - 1; //rptr--;												// Something strange
					//else rptr--;
					//rptr--;	// For too much adds.
					if (getIndentRaw(codestream[rptr]) <= ind) rptr--;
					size_t eptr = execptr;
					int r;
					bool has_right = false;
					while (eptr < codestream.size() - 1) {
						// End if indent equals and not 'elif' or 'else'.
						//if () break;
						string s = codestream[++eptr];
						r = getIndent(s);
						vector<string> sp = split(s, ' ', 1);
						if (!sp.size()) continue;
						if (r == ind && sp[0] != "elif" && sp[0] != "else:") goto end101;
						if (r < ind) { // Multi-jump at once: Too far
							// Direct jumps
							if (jmptable.count(eptr - 1)) eptr = jmptable[eptr - 1];
							goto end101;
						}
					}
					if (r > ind) eptr++;	// Last line still not acceptable
				//	jmptable[rptr] = jmptable[eptr];	// Already end of code!
				//	goto end102;
					if (eptr == 0) {
						has_right = false;
					}
					else if (eptr >= codestream.size() - 1) {
						has_right = true;
					}
					else {
						string ep = codestream[eptr];
						getIndent(ep);
						auto gr = getIndentRaw(codestream[eptr]);
						has_right = gr <= ind;
						// However:
						if (beginWith(ep, "elif") || beginWith(ep, "else:")) if (gr == ind) has_right = false;	// No jumping here
					}
					if (jmptable.count(eptr)) eptr = jmptable[eptr];
					else if (has_right && jmptable.count(eptr - 1)) eptr = jmptable[eptr - 1];	//?
				end101: jmptable[rptr] = eptr;
				end102:;
				}
				else {
					// Go on elif / else
					size_t eptr = execptr + 1;
					if (eptr >= codestream.size() - 1) {
						eptr++;
					}
					while (eptr < codestream.size() - 1) {
						// End if indent equals and not 'elif' or 'else'.
						string s = codestream[++eptr];
						int r = getIndent(s);
						vector<string> sp = split(s, ' ', 1);
						if (!sp.size()) continue;
						if (r <= ind) break;
						// -- Dismatches still? --
						if (eptr >= codestream.size() - 1) {
							eptr++;
							break;
						}
					}
					bool has_right = false;
					string ep = "";
					if (eptr >= codestream.size()) {
						has_right = true;
					}
					else if (eptr <= 0) {
						has_right = false;	// How can you proceed jump on the first line?
					}
					else {
						ep = codestream[eptr];
						getIndent(ep);
						auto gr = getIndentRaw(codestream[eptr]);
						has_right = gr <= ind;
						// However:
						if (beginWith(ep, "elif") || beginWith(ep, "else:")) if (gr == ind) has_right = false;	// No jumping here
					}
					if (has_right && jmptable.count(eptr - 1)) execptr = jmptable[eptr - 1];
					else execptr = eptr;
					goto after_add_exp;
				}
			}
			else if (codexec[0] == "else:" || codexec[0] == "preset") {
				// Go on executing							^ Have been dealt with during the preRun segment
			}
			else if (codexec[0] == "run") {
				parameter_check(2);
				calculate(codexec[1], myenv);
			}
			else if (codexec[0] == "while") {
				parameter_check(2);
				if (codexec[1].length()) codexec[1].pop_back();
				if (calculate(codexec[1], myenv).boolean()) {
					// True, go on execution, and set jumper to here at the end.
					// must have a acceptable line.
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
					int r;
					while (execptr != codestream.size() - 1) {
						r = getIndentRaw(codestream[++execptr]);
						if (r == ind) goto after_add_exp; //break;
						if (r < ind) { // Multi-jump at once: Too far
							// Direct jumps
							if (jmptable.count(execptr - 1)) execptr = jmptable[execptr - 1];
							goto after_add_exp;
						}
					}
					if (r > ind) execptr++;		// Last line is not acceptable
					if (jmptable.count(execptr)) execptr = jmptable[execptr]; // Must not look for execution

					goto after_add_exp;

				}
			}
			else if (codexec[0] == "continue") {
				// Go back to previous (nearest) small-indent
				int addin = ind;
				while (execptr > 0) {
					execptr--;
					string s = codestream[execptr];
					int id = getIndent(s);
					//int addin = ind;
					vector<string> spl = split(s, ' ', 1);
					if (id < addin) {
						if (spl[0] == "while" || spl[0] == "for") {
							break;
						}
						else {
							addin = id;	// Mustn't in correct block. Must be its father.
						}
					}
				}
				goto after_add_exp;
			}
			else if (codexec[0] == "break") {
				// Find for looper
				size_t ep = execptr;
				int ide;	// The indent to search
				int addin = ind;
				while (ep > 0) {
					ep--;
					string s = codestream[ep];
					ide = getIndent(s);
					vector<string> spl = split(s, ' ', 1);
					if (ide < addin) {
						if (spl[0] == "while" || spl[0] == "for") {
							break;
						}
						else {
							addin = ide;	// Mustn't in correct block. Must be its father.
						}
					}

				}
				// ep is the position of the block.
				jmptable.revert_all(ep, true);
				while (true) {
					execptr++;
					if (execptr >= codestream.size()) break;
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
				noroll = true;
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
				if (rangeobj.size() >= 3) stepper = calculate(rangeobj[2], myenv);
				if (myenv[codexec2[0]].isNull) {
					current = intValue(calculate(rangeobj[0], myenv).numeric);
				}
				else {
					current = intValue(myenv[codexec2[0]].numeric + stepper.numeric);
				}
				myenv[codexec2[0]] = current;
				if (myenv[codexec2[0]].numeric == calculate(rangeobj[1], myenv).numeric) {
					// Jump where end-of-loop
					myenv.tree_clean(codexec2[0]);
					noroll = true; //  End of statements' life
					int r;
					while (execptr != codestream.size() - 1) {
						r = getIndentRaw(codestream[++execptr]);
						if (r == ind) goto after_add_exp; //break;
						if (r < ind) { // Multi-jump at once: Too far
							// Direct jumps
							if (jmptable.count(execptr - 1)) execptr = jmptable[execptr - 1];
							goto after_add_exp;
						}
						if (execptr == codestream.size() - 1) {
							// Still not match
							if (jmptable.count(execptr)) execptr = jmptable[execptr];
							else execptr++;
							goto after_add_exp;
						}
					}
					if (r <= ind) execptr--;			// Last line is not acceptable
					if (jmptable.count(execptr)) execptr = jmptable[execptr]; // Must not look for execution
					goto after_add_exp;
				}
				else {
					size_t eptr = execptr;
					while (eptr != codestream.size() - 1) {
						// End if indent equals.
						int r = getIndentRaw(codestream[++eptr]);
						if (r <= ind) {
							jmptable[eptr - 1] = execptr;
							goto afterset1;
						}
					}
					jmptable[eptr] = execptr;	// Should be changed in 'while' loop??
				afterset1:;
				}
			}
			else if (codexec[0] == "dump") {
				if (codexec.size() >= 2) {
					if (codexec[1] == "__errno__") {
						cout << errno << endl;
					}
					else {
						cout << myenv.serial(codexec[1]).str << endl;
					}

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
					int f = calculate(codexec2[1], myenv).numeric;
					if (files.count(f)) {
						fclose(files[f]);
						files.erase(f);
					}
					else {
						//curlout();
						//cout << "Incorrect file: " << f << endl;
						//endout();
						raise_ce("Incorrect file: " + to_string(f));
					}

				}
				else if (op == "write") {
					codexec3 = split(codexec2[1], ',', 1);
					int f = calculate(codexec3[0], myenv).numeric;
					if (files.count(f)) {
						fputs(calculate(codexec3[1], myenv).str.c_str(), files[f]);
					}
					else {
						raise_ce("Incorrect file: " + to_string(f));
					}

				}
				else {
					if (op == "open") {
						// file open [var]=[filename],[mode]
						codexec3 = split(codexec2[1], '=', 1);
						vector<string> codexec4 = split(codexec3[1], ',', 1);
						int n = 0;
						while (files.count(++n));
						if (codexec3[0].find(":") != string::npos) {
							codexec3[0] = curexp(codexec3[0], myenv);
						}
						myenv[codexec3[0]] = intValue(n);
						intValue ca = calculate(codexec4[0], myenv);
						string fn = ca.str;
						string op = calculate(codexec4[1], myenv).str;
						files[n] = fopen(fn.c_str(), op.c_str());
						if (files[n] == NULL || feof(files[n])) {
							//cout << "Cannot open file " << fn << " as " << n << ", errno: " << errno << endl;
							//raise_ce("Cannot open file " + fn +)
							raiseError(intValue(errno), myenv, fname, execptr + 1, 1, "Cannot open file " + fn + " as " + to_string(n));
						}
					}
					else if (op == "open_console") {
						// file open_console [var]=[console instruction string],[mode]
						codexec3 = split(codexec2[1], '=', 1);
						vector<string> codexec4 = split(codexec3[1], ',', 1);
						int n = 0;
						while (files.count(++n));
						if (codexec3[0].find(":") != string::npos) {
							codexec3[0] = curexp(codexec3[0], myenv);
						}
						myenv[codexec3[0]] = intValue(n);
						intValue ca = calculate(codexec4[0], myenv);
						string fn = ca.str;
						string op = calculate(codexec4[1], myenv).str;
						files[n] = popen(fn.c_str(), op.c_str());
						if (files[n] == NULL || feof(files[n])) {
							raiseError(intValue(errno), myenv, fname, execptr + 1, 1, "Cannot open command " + fn + " as a pipe for " + to_string(n));
						}
					}
					else if (op == "read") {
						// file read [store]=[var]
						codexec3 = split(codexec2[1], '=', 1);
						int fid = calculate(codexec3[1], myenv).numeric;
						bool rs = files.count(fid) ? feof(files[fid]) : 0;
						if (files.count(fid) && !rs) {
							string res = "";
							char c;
							while ((!feof(files[fid])) && (c = fgetc(files[fid])) != '\n') res += c;
							if (codexec3[0].find(":") != string::npos) {
								codexec3[0] = curexp(codexec3[0], myenv);
							}
							myenv[codexec3[0]] = intValue(res);
						}
						else {
							raise_ce("Incorrect file: " + to_string(fid));
						}
					}
					else if (op == "valid") {
						// file vaild [store]=[var]
						codexec3 = split(codexec2[1], '=', 1);
						if (codexec3[0].find(":") != string::npos) {
							codexec3[0] = curexp(codexec3[0], myenv);
						}
						int fid = calculate(codexec3[1], myenv).numeric;
						bool rs = files.count(fid) ? (!feof(files[fid])) : 0;
						if (files.count(fid) && rs) {
							myenv[codexec3[0]] = intValue(1);
						}
						else {
							myenv[codexec3[0]] = intValue(0);
						}
					}
					else if (op == "len") {
						// file len [store]=[var]
						codexec3 = split(codexec2[1], '=', 1);
						int fid = calculate(codexec3[1], myenv).numeric;
						bool rs = files.count(fid) ? feof(files[fid]) : 0;
						if (files.count(fid) && !rs) {
							myenv[codexec3[0]] = intValue(getLength(fid));
						}
						else {
							raise_ce("Incorrect file: " + to_string(fid));
						}
					}
					else if (op == "binary_read") {
						// file binary_read [store list]=[var]
						codexec3 = split(codexec2[1], '=', 1);
						int fid = calculate(codexec3[1], myenv).numeric;
						bool rs = files.count(fid) ? feof(files[fid]) : 0;
						if (files.count(fid) && !rs) {
							size_t len = getLength(fid);
							char *buf = new char[len + 2];
							fread(buf, sizeof(char), len, files[fid]);
							if (codexec3[0].find(":") != string::npos) {
								codexec3[0] = curexp(codexec3[0], myenv);
							}
							string &cn = codexec3[0];
							//myenv[cn + ".__type__"] = "list";
							generateClass(cn, "list", myenv, false);	// Accerlation
							myenv[cn + "._length"] = intValue(len);			// For property
							for (size_t i = 0; i < len; i++) {
								myenv[cn + "." + to_string(i)] = intValue(int(buf[i]));
							}
							delete[] buf;
							//... For list
						}
						else {
							raise_ce("Incorrect file: " + to_string(fid));
						}
					}
					else if (op == "binary_write") {
						// file binary_write [fid],[list data]
						//codexec3 = split(codexec2[1], '=', 1);
						vector<string> codexec4 = split(codexec2[1], ',', 1);
						int fid = calculate(codexec4[0], myenv).numeric;
						bool rs = files.count(fid) ? feof(files[fid]) : 0;
						if (files.count(fid) && !rs) {
							if (codexec4[1].find(":") != string::npos) {
								codexec4[1] = curexp(codexec4[1], myenv);
							}
							string &cn = codexec4[1];
							if (inh_map.is_same(myenv[cn + ".__type__"].str, "list")) {
								long64 clen = myenv[cn + "._length"].numeric;
								char *buf = new char[clen + 2];
								for (size_t i = 0; i < clen; i++) {
									buf[i] = char((myenv[cn + "." + to_string(i)].numeric));
								}
								fwrite(buf, sizeof(char), clen, files[fid]);
								delete[] buf;
							}
							else {
								raise_ce("Binary output requires list type");
							}
						}
						else {
							raise_ce("Incorrect file: " + to_string(fid));
						}
					}
				}

			}
			else if (codexec[0] == "mutex") {
				vector<string> codexec2 = split(codexec[1], ' ', 1), codexec3;
				parameter_check(2);
				parameter_check2(2, "mutex");
				if (codexec2[0] == "test") {
					// mutex test receiver=name
					codexec3 = split(codexec2[1], '=', 1);
					if (codexec3[0][0] == '$') {
						codexec3[0].erase(codexec3[0].begin());
						codexec3[0] = calculate(codexec3[0], myenv).str;
					}
					else if (codexec3[0].find(":") != string::npos) {
						codexec3[0] = curexp(codexec3[0], myenv);
					}
					myenv[codexec3[0]] = intValue(mutex_table.count(calculate(codexec3[1], myenv).str));
				}
				else {
					string mutex_name = calculate(codexec2[1], myenv).str;
					if (codexec2[0] == "make") {
						mutex_table[mutex_name];
					}
					else if (codexec2[0] == "wait") {
						mutex_table[mutex_name].lock();
					}
					else if (codexec2[0] == "release") {
						mutex_table[mutex_name].unlock();
					}
				}
			}
			else if (codexec[0] == "thread") {
				vector<string> codexec2 = split(codexec[1], ' ', 1), codexec3;
				parameter_check(2);
				parameter_check2(2, "thread");
				if (codexec2[0] == "test" || codexec2[0] == "new") {

					// thread test [receiver=]name
					codexec3 = split(codexec2[1], '=', 1);
					bool dispose_result = false;
					if (codexec3.size() < 2) {
						dispose_result = true;
						codexec3.push_back(codexec3[0]);
					}
					if (codexec3[0][0] == '$') {
						codexec3[0].erase(codexec3[0].begin());
						codexec3[0] = calculate(codexec3[0], myenv).str;
					}
					else if (codexec3[0].find(":") != string::npos) {
						codexec3[0] = curexp(codexec3[0], myenv);
					}
					if (codexec2[0] == "test") {
						if (dispose_result) {
							raiseError(null, myenv, fname, execptr + 1, 3, "Can't dispose result of 'thread test'");
						}
						else {
							int cid = calculate(codexec3[1], myenv).numeric;
							thread_status result = thread_status::unknown;
							if (thread_table.count(cid)) {
								thread &t = thread_table[cid];
								if (t.joinable()) {
									result = thread_status::joinable;
								}
								else {
									result = thread_status::not_joinable;
								}
							}
							else {
								result = thread_status::not_exist;
							}
							myenv[codexec3[0]] = intValue(result);
						}
						
					}
					else {
						parameter_check3(2, "thread");
						int n = 0;
						while (thread_table.count(++n));
						if (!dispose_result) myenv[codexec3[0]] = intValue(n);
						getValue(codexec3[1], myenv, false, n);

					}

				}
				else {
					// mutex join/detach var
					if (codexec2[0] == "join") {
						int cid = calculate(codexec2[1], myenv).numeric;
						if (thread_table.count(cid)) {
							auto &t = thread_table[cid];
							if (t.joinable()) t.join();
							else raise_ce("Cannot join current thread");
						}
					}
					else if (codexec2[0] == "detach") {
						int cid = calculate(codexec2[1], myenv).numeric;
						if (thread_table.count(cid)) {
							auto &t = thread_table[cid];
							t.detach();
						}
					}
				}
			}
			else if (codexec[0] == "import") {
				// Do nothing
			}
			else if (codexec[0] == "__dev__") {
				if (codexec.size() >= 2) {
					if (codexec[1] == "time_set") {
						SYSTEMTIME sys;
						GetLocalTime(&sys);
						myenv["this.year"] = intValue(sys.wYear);
						myenv["this.month"] = intValue(sys.wMonth);
						myenv["this.day"] = intValue(sys.wDay);
						myenv["this.hour"] = intValue(sys.wHour);
						myenv["this.minute"] = intValue(sys.wMinute);
						myenv["this.second"] = intValue(sys.wSecond);
						myenv["this.ms"] = intValue(sys.wMilliseconds);
						myenv["this.week"] = intValue(sys.wDayOfWeek);
					}
					else if (beginWith(codexec[1], "bad_property")) {
						vector<string> codexec2 = split(codexec[1], ' ', 1);
						raiseError(null, myenv, fname, execptr + 1, __LINE__, "Bad use of property " + codexec2[1] + " (this might be because the method is disallowed)");
						return null;	// For GET attempts
					}

				}
				else {
					specialout();
					cout << "Developer call" << endl;
					endout();
				}
			}
			else if (codexec[0] == "debugger") {
				if (in_debug) {
					debugcall();
				}
			}
			else if (codexec[0] == "call") {
				// Call internal calls
				// call ([var]=)[callname],[parameter]
				parameter_check(2);
				vector<string> codexec2 = split(codexec[1], ',', 1);
				intValue result;
				if (codexec2.size() <= 0) {
					raise_ce("Error: required parameter missing");
					goto add_exp;
				}
				string save_target = "";
				if (codexec2[0].find('=') != string::npos) {
					vector<string> spl = split(codexec2[0], '=', 1);
					save_target = spl[0];
					codexec2[0] = spl[1];
				}
				if ((!intcalls.count(codexec2[0]))) {
					raise_ce("Error: required system call does not exist");
					goto add_exp;
				}
				if (codexec2.size() == 1) {
					result = intcalls[codexec2[0]]("", myenv);
				}
				else if (codexec2.size() == 2) {
					result = intcalls[codexec2[0]](codexec2[1], myenv);
				}
				if (save_target.length()) {
					if (result.isObject) {
						myenv.deserial(save_target, result.serial_data);
					}
					else {
						myenv[save_target] = result;
					}
					
				}
			}
			else {
				// = run ...
				calculate(prerun, myenv);
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

	string env_name;	// Directory of current file.
	const set<char> to_trim = { ' ', '\n', '\r', -1 };

	intValue preRun(string code, map<string, intValue> required_global = {}, map<string, bcaller> required_callers = {}) {
		// Should prepare functions for it.
		string fname = "Runtime preproessor";
		
		myenv.push();
		// Preset constants
#pragma region Preset constants
		myenv.set_global("LF", intValue("\n"), true);
		myenv.set_global("TAB", intValue("\t"), true);
		myenv.set_global("BKSP", intValue("\b"), true);
		myenv.set_global("ALERT", intValue("\a"), true);
		myenv.set_global("RBACK", intValue("\r"), true);
		myenv.set_global("CLOCKS_PER_SEC", intValue(CLOCKS_PER_SEC), true);
		myenv.set_global("true", intValue(1), true);
		myenv.set_global("false", intValue(0), true);
		myenv.set_global("thread_state.unknown", intValue(thread_status::unknown), true);
		myenv.set_global("thread_state.not_exist", intValue(thread_status::not_exist), true);
		myenv.set_global("thread_state.joinable", intValue(thread_status::joinable), true);
		myenv.set_global("thread_state.not_joinable", intValue(thread_status::not_joinable), true);
		// Replacable:
		myenv.set_global("err.__type__", intValue("exception"));			// Error information
		myenv.set_global("__error_handler__", intValue("call set_color,14\nprint \"On \"+err.source+\", Line \"+err.line+LF+err.description+LF+err.value+LF\ncall set_color,7"));	// Preset error handler
		// End of replacable
		myenv.set_global("__file__", intValue(env_name), true);
		myenv.set_global("system_type", intValue(environ_type), true);
		// Insert more global variable
		for (auto &i : required_global) {
			myenv.set_global(i.first, i.second);
		}
#pragma endregion
#pragma region Preset calls
		intcalls["thread_yield"] = [](string args, varmap &env) -> intValue {
			this_thread::yield();
			return null;
		};
		intcalls["sleep"] = [this](string args, varmap &env) -> intValue {
			//Sleep(DWORD(calculate(args, env).numeric));
			this_thread::sleep_for(chrono::milliseconds((long long)calculate(args, env).numeric));
			return null;
		};
		intcalls["system"] = [this](string args, varmap &env) -> intValue {
			return intValue(system(calculate(args, env).str.c_str()));
		};
		intcalls["exit"] = [this](string args, varmap &env) -> intValue {
			exit(int(calculate(args, env).numeric));
			return null;
		};
		intcalls["set_color"] = [this](string args, varmap &env) -> intValue {
			setColor(DWORD(calculate(args, env).numeric));
			return null;
		};
		intcalls["dir"] = [this](string args, varmap &env) -> intValue {
			// dir target,origin,matcher[,prohibit recesuive]
			vector<string> codexec = parameterSplit(args);
			bool proh_rec = false;
			if (codexec.size() < 3) {
				raiseError(null, env, "Yield caller", 0, 6, "Bad grammar");
				return null;
			}
			if (codexec.size() >= 4) proh_rec = calculate(codexec[3], env).numeric;
			auto result = yieldDirectory(calculate(codexec[1], env).str, calculate(codexec[2], env).str, codexec[0] + '.');
			env.insert(result.begin(), result.end());
			env[codexec[0]] = null;
			env[codexec[0] + ".__type__"] = intValue("yield_result");
			return null;
			//res.isObject = true;
			//return res;
		};
		// I remind you that eval is dangerous!
		intcalls["eval"] = [this](string args, varmap &env) -> intValue {
			return run(calculate(args, env).str, env, "Internal eval()");
		};
		intcalls["cls"] = [this](string args, varmap &env) -> intValue {
			clearScreen();
			return null;
		};
		intcalls["dll"] = [this](string args, varmap &env) -> intValue {
			// call dll,dllname_string function
			// call dll,dllname_string function:name1=val1;name2=val2,...
			// name: normal name, or
			// $ format, or
			// a:b format. will be dealt with as set statements
			vector<string> dllargs = split(args, ':', 1);
			varmap new_env;
			new_env.push();
			if (dllargs.size() >= 2) {
				vector<string> dllpara = split(dllargs[1], ';');
				for (auto &i : dllpara) {
					vector<string> singles = split(i, '=', 1);
					// Deal with singles[0].
					if (singles[0][0] == '$') {
						singles[0] = calculate(singles[0].substr(1), env).str;
					}
					else {
						singles[0] = auto_curexp(singles[0], env);
					}
					new_env[singles[0]] = calculate(singles[1], env);
				}
			}
			vector<string> dllmains = split(dllargs[0], ' ', 1);
			if (dllmains.size() < 2) {
				raiseError(null, env, "DLL Caller", 0, 6, "Bad grammar");
				return null;
			}
			string dllname = calculate(dllmains[0], env).str;
			string funcname = dllmains[1];
			// Deal with function name:
			if (dllmains[1][0] == '$') funcname = calculate(dllmains[1].substr(1), env).str;
#if defined(_WIN32)
			HINSTANCE dllinst = LoadLibrary(dllname.c_str());
			if (dllinst == NULL) {
				raiseError(GetLastError(), env, "Dll caller", 0, 8 + (GetLastError() << 4), "Can't get DLL instance");
				return null;
			}
			blue_dcaller func = (blue_dcaller)GetProcAddress(dllinst, funcname.c_str());
			if (func == nullptr) {
				raiseError(errno, env, "Dll caller", 0, 7, "Can't get address of certain function");
				FreeLibrary(dllinst);
				return null;
			}
			//
			auto res = func(&new_env);
			FreeLibrary(dllinst);
			return res;
#elif defined(__linux__)
			void *dlib = dlopen(dllname.c_str(), RTLD_NOW);
			blue_dcaller func = dlsym(dlib, funcname.c_str());
			auto res = func(&new_env);
			dlclose(dlib);
			return res;
#else
			raiseError(null, env, "Dll caller", 0, 5, "Dynamic library calls are not supported on this platform");
			return null;
#endif
		};
		intcalls["__bnot"] = [this](string args, varmap &env) -> intValue {
			return ~ulong64(calculate(args, env).numeric);
		};
		// It is better to add more functions by intcalls, not set
#define math_extension(funame) intcalls["_maths_" #funame] = [this](string args, varmap &env) -> intValue { \
		return intValue(funame(calculate(args, env).numeric)); \
	}
#define system_caller(fname,funcref) intcalls[fname] = [this](string args, varmap &env) -> intValue { \
		int ret = intValue(funcref(calculate(args, env).str)).numeric; \
		if (ret) raiseError(ret, env, fname, 0, 9 + (errno << 4) + (ret << 6), string("System error in "#fname " errno: ") + to_string(errno) + " ret: " + to_string(ret)); \
		return null; \
	}
		math_extension(sin);
		math_extension(cos);
		math_extension(tan);
		math_extension(asin);
		math_extension(acos);
		math_extension(atan);
		math_extension(sqrt);
		system_caller("mkdir", makeDirectory);
		system_caller("md", makeDirectory);
		system_caller("rmdir", removeDirectory);
		system_caller("rd", removeDirectory);
		system_caller("rm", removesFile);
		system_caller("del", removesFile);
		intcalls["_trim"] = [this](string args, varmap &env) -> intValue {
			string s = calculate(args, env).str;
			while (s.length() && to_trim.count(s[s.length() - 1])) s.pop_back();
			size_t spl;
			for (spl = 0; spl < s.length(); spl++) if (!to_trim.count(s[spl])) break;
			return intValue(s.substr(spl));
		};
		for (auto &i : required_callers) {
			intcalls[i.first] = i.second;
		}
#pragma endregion
		// Get my directory
		size_t p = env_name.find_last_of('\\');
		string env_dir;	// Directly add file name.
		if (p <= 0) {
			env_dir = "";
		}
		else {
			env_dir = env_name.substr(0, p) + '\\';
		}
		include_sources.insert(include_sources.begin(), env_dir);	// search at first
		//bool bmain_fail = false;
		vector<string> codestream;
		// Initalize libraries right here
		FILE *f = fopen("bmain.blue", "r");
		if (f != NULL) {
			while (!feof(f)) {
				fgets(buf1, 65536, f);
				codestream.push_back(buf1);
			}
			fclose(f);
		}
		else if (!no_lib) {
			size_t execptr = 0;	// For ce-raising
			raise_ce("You don't have bmain.blue, which is required! To prevent this, copy bmain.blue under program directory or use --no-lib.");
		}

		// End

		vector<string> sc = split(code, '\n', -1, '\"', '\\', true);
		codestream.insert(codestream.end() - 1, sc.begin(), sc.end());
		string curclass = "";					// Will append '.'
		string curfun = "", cfname = "", cfargs = "";
		int fun_indent = max_indent;
		for (size_t i = 0; i < codestream.size(); i++) {
			size_t &execptr = i;	// For macros
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
					myenv.set_global(cfname, intValue(curfun));
					myenv.set_global(cfname + ".__type__", intValue("function"), true);
					myenv.set_global(cfname + ".__arg__", cfargs, true);
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
						myenv.set_global(codexec[1] + ".__type__", intValue("class"));
						myenv.declare_global(codexec[1]);
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
						else {
							// TODO: Add more scanner for BluePage or others' support
							bool suc_flag = false;
							for (auto &i : include_sources) {
								f = fopen((i + codexec2[1]).c_str(), "r");
								if (f != NULL) {
									while (!feof(f)) {
										fgets(buf1, 65536, f);
										codestream.push_back(buf1);
									}
									fclose(f);
									suc_flag = true;
									break;
								}
							}
							if (!suc_flag) {
								raise_ce(string("Bad import: ") + codexec2[1]);
							}
						}
					}
					else if (codexec[0] == "prerun") {
						parameter_check(2);
						string inst = codexec[1];
						if (codexec.size() >= 3) inst = inst + ' ' + codexec[2];
						calculate(inst, myenv);
					}
					else if (codexec[0] == "error_handler:") {
						fun_indent = 1;
						cfname = "__error_handler__";
					}
					break;
				case 1:
					if (codexec[0] == "init:") {
						fun_indent = 2;
						cfname = curclass + "__init__";
					}
					else if (codexec[0] == "inherits") {
						string &to_inherit = codexec[1];
						if (myenv[to_inherit + ".__no_inherit__"].str == "1") {
							curlout();
							cout << "Warning: No inheriting class " << to_inherit << endl;
							endout();
						}
						else {
							string curn = curclass.substr(0, curclass.length() - 1);
							string old_inherits = myenv[curn + ".__inherits__"].str;
							if (old_inherits == "null" || old_inherits == "") old_inherits = "";
							else old_inherits += ",";
							myenv.set_global(curn + ".__inherits__", old_inherits + to_inherit);
							// Run inheritance
							myenv.copy_inherit(to_inherit, curn);
							inh_map.unions(to_inherit, curn);
						}
					}
					else if (codexec[0] == "shared") {
						string &to_share = codexec[1];
						while (to_share.length() && to_share[to_share.length() - 1] == '\n') to_share.pop_back();
						if (to_share == "class") {
							myenv.set_global(curclass + "__shared__", intValue("1"));
						}
						else {
							string to_set = curclass + to_share;
							if (!myenv.count(to_set)) {
								myenv.declare_global(to_set);
							}
							myenv.set_global(to_set + ".__shared__", intValue("1"));
						}
					}
					else if (codexec[0] == "must_inherit") {
						myenv.set_global(curclass + "__must_inherit__", intValue("1"));
					}
					else if (codexec[0] == "no_inherit") {
						myenv.set_global(curclass + "__no_inherit__", intValue("1"));
					}
					else if (codexec[0] == "hidden") {
						parameter_check(2);
						string &ce = codexec[1];
						while (ce.length() && (ce[0] == '\n')) ce.erase(ce.begin());
						while (ce.length() && (ce[ce.length() - 1] == '\n')) ce.pop_back();
						myenv.set_global(curclass + ce + ".__hidden__", intValue("1"));
					}
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
				// property [get|set] ...: <function body>
				// For <setter> use <value> as parameter
				else if (codexec[0] == "property") {
					parameter_check(3);
					fun_indent = 1 + bool(curclass.length());
					if (codexec[2][codexec[2].length() - 1] == '\n') codexec[2].pop_back();
					if (codexec[2][codexec[2].length() - 1] == ':') codexec[2].pop_back(); // ':'
					//myenv[curclass + codexec[2] + ".__is_prop__"] = intValue("1");
					myenv.set_global(curclass + codexec[2] + ".__is_prop__", intValue("1"), true);
					if (codexec[1] == "get") {
						cfname = curclass + codexec[2];
						cfargs = "";
					}
					else if (codexec[1] == "set") {
						cfname = curclass + codexec[2] + ".__setter__";
						cfargs = "value";
					}
					else if (codexec[1] == "noget") {
						string tcf = curclass + codexec[2];
						myenv.set_global(tcf, intValue("__dev__ bad_property " + tcf));
						myenv.set_global(tcf + ".__type__", intValue("function"), true);
						myenv.set_global(tcf + ".__arg__", intValue(""), true);
					}
					else if (codexec[1] == "noset") {
						string tcf = curclass + codexec[2] + ".__setter__";
						myenv.set_global(tcf, intValue("__dev__ bad_property " + curclass + codexec[2]));
						myenv.set_global(tcf + ".__type__", intValue("function"), true);
						myenv.set_global(tcf + ".__arg__", intValue("value"), true);
					}
					else {
						raise_ce("Bad property");
					}
				}
				else if (codexec[0] == "preset") {
					// Instead of 'global', use 'preset' for preRun-segment global variables!
					bool constant = false;
					parameter_check(2);
					while (codexec[1][codexec[1].length() - 1] == '\n') codexec[1].pop_back();
					string data = codexec[1];
					if (codexec.size() >= 3) {
						while (codexec[2][codexec[2].length() - 1] == '\n') codexec[2].pop_back();
						data = data + ' ' + codexec[2];
					}
					vector<string> codexec2 = split(data, '=', 1);	// May be global a=const ...
					parameter_check2(2, "set");
					if (codexec2[0][0] == '$') {
						codexec2[0].erase(codexec2[0].begin());
						codexec2[0] = calculate(codexec2[0], myenv).str;
					}
					else if (codexec2[0].find(":") != string::npos) {
						codexec2[0] = curexp(codexec2[0], myenv);
					}
					codexec2[0] = curclass + codexec2[0];
					if (codexec2[0].find(".__const__") != string::npos || myenv[codexec2[0] + ".__const__"].str == "1") {
						raise_ce(string("Cannot set a value of constant: ") + codexec2[0] + " (this could be because of a redeclaration)");
					}
					else {
						static const string const_sign = "const ";	// set a=const ...
						if (beginWith(codexec2[1], const_sign)) {
							constant = true;
							codexec2[1] = codexec2[1].substr(const_sign.length());
						}
						char det;
						string external_op = "", &czero = codexec2[0];
						// Only 2 layers' detect, reversely
						if (czero.length() >= 2 && priority(det = czero[czero.length() - 1], true) > 0) {
							external_op = det;
							czero.pop_back();
							if (czero.length() >= 2 && priority(det = czero[czero.length() - 1], true) > 0) {
								external_op = det + external_op;
								czero.pop_back();
							}
						}
						// Should be like:
						// myenv[codexec2[0]] = primary_calculate(myenv[codexec2[0]], external_op, res, myenv);
						if (codexec2[0].find(".__const__") != string::npos || myenv[codexec2[0] + ".__const__"].str == "1") {
							raise_ce(string("Cannot set a value of constant: ") + codexec2[0]);
							return null;
						}
						auto res = calculate(codexec2[1], myenv);
						if (res.isObject) {
							if (external_op.length()) {
								raise_ce("Warning: using operators like +=, -=, *= for object is meaningless");
							}
							myenv.global_deserial(codexec2[0], res.serial_data);
							if (constant) {
								myenv.set_global(codexec2[0] + ".__const__", intValue("1"));
							}
						}
						else if (external_op.length()) {
							myenv.set_global(codexec2[0], primary_calculate(myenv.get_global(codexec2[0]), external_op, res, myenv), constant);
						}
						else {
							myenv.set_global(codexec2[0], res, constant);
						}
					}

				}

			}

		}
		if (cfname.length()) {
			myenv.set_global(cfname, curfun);
			myenv.set_global(cfname + ".__type__", intValue("function"), true);
			myenv.set_global(cfname + ".__arg__", cfargs, true);
		}

		//return null;
		// End

		intValue res = run(code, myenv, "Main function");

		// For debug propose
		//myenv.dump();
		return res;
	}

	interpreter(bool in_debug = false, bool no_lib = false, map<string, bcaller> intcalls = {}, vector<string> include_sources = {}) 
		: in_debug(in_debug), no_lib(no_lib), intcalls(intcalls), include_sources(include_sources) {

		}

	bool in_debug = false;	// Runner debug option.
	bool no_lib = false;
	vector<string> include_sources;
	map<string, bcaller> intcalls;
	varmap myenv;

	private:
		int __spec = 0;
		const int max_indent = 65536;

		// For mutex and thread support
		// mutex: mutex make/wait/release [name]
		map<string, mutex> mutex_table;
		// thread: thread new container=function; thread join container; thread detach container; thread status receiver=container
		map<int, thread> thread_table;

		bool np = false, spec_ovrd = false;
		
		map<int, FILE*> files;
	

};

