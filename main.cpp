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
#include <ctime>
#include <cmath>
using namespace std;

// To symbol if it's next statement, not next progress
bool np = false, spec_ovrd = false;
int __spec = 0;

// Open if not use bmain.blue
bool no_lib = false;

// Pre declare
class varmap;
struct intValue;
intValue run(string code, varmap &myenv, string fname);
intValue calculate(string expr, varmap &vm);
void raiseError(intValue raiseValue, varmap &myenv, string source_function = "Unknown source", size_t source_line = 0, double error_id = 0, string error_desc = "");
intValue getValue(string single_expr, varmap &vm, bool save_quote = false);

HANDLE stdouth;
DWORD precolor = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN, nowcolor = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN;
void setColor(DWORD color) {
	SetConsoleTextAttribute(stdouth, color);
	precolor = nowcolor;
	nowcolor = color;
}

inline void begindout() {
	setColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
}

inline void endout() {
	
	setColor(precolor);
}

inline void specialout() {
	setColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
}

inline void curlout() {
	setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
}

const int max_indent = 65536;

char buf0[255], buf01[255], buf1[65536];
bool in_debug = false;	// Runner debug option.
//set<size_t> breakpoints;
vector<string> watches;

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

string unformatting(string origin) {
	string ns = "\"";
	for (size_t i = 0; i < origin.length(); i++) {
		if (origin[i] == '"' || origin[i] == '\\') {
			ns += "\\";
			ns += origin[i];
		}
		else {
			ns += origin[i];
		}
	}
	return ns + "\"";
}

struct intValue {
	// To be considered:
	// When these values are set, 'isNull' should NOT be true anymore.

	// DO NOT WRITE THEM DIRECTLY!
	double								numeric;
	string								str;
	bool								isNull = false;
	bool								isNumeric = false;
	bool								isObject = false;

	intValue negative() {
		if (this->isNumeric) {
			return intValue(-this->numeric);
		}
		else if (!this->isNull) {
			return intValue(this->str);
		}
		else {
			return null;
		}
	}

	void set_numeric(double value) {
		this->numeric = value;
		this->str = to_string(value);
		this->isNull = false;
		this->isNumeric = true;
	}

	void set_str(string value) {
		this->str = value;
		this->isNull = false;
		this->isNumeric = true;
	}

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

} null, trash;	// trash: Return if incorrect varmap[] is called.

#define raise_ce(description) raiseError(null, myenv, fname, execptr, __LINE__, description)
#define raise_varmap_ce(description) raiseError(null, *this, "Runtime", 0, __LINE__, description)
#define raise_gv_ce(description) raiseError(null, vm, "Runtime", 0, __LINE__, description);

inline int countOf(string str, char charToFind) {
	int count = 0;
	for (auto &i : str) {
		if (i == charToFind) count++;
	}
	return count;
}

// All the varmaps show one global variable space.
/*
Special properties:

__arg__			For a function, showing its parameters (delimitered by space).
__init__		For a class definition, showing its initalizing function.
	For class,	[name].[function]
__type__		For a class object, showing its kind.
__inherits__	For a class object, showing its inherited class (split using ','.)
__hidden__		For a class definition, showing if this value will be hidden during get_member (if 'forceShow' is not given).

Extras:
__must_inherit__		For a class objct, 1 if it must be inherited.
	(A __must_inherit__ = 1 class can't have object, either.)
__no_inherit__			For a class object, 1 if it mustn't be inherited.
__shared__				For a class object, 1 if object can't create from the class (shared class).
	(This feature will be inherited!)
	For a function object, 1 if this function can't use "this" but can be called directly by class name.

In environment:
__error_handler__			(User define) processes error handler
__is_sharing__				(User define) symbol if is calling shared thing.
	(If __is_sharing__ = 1, call of "this" will fail.)

ALL THINGS ABOVE WILL BECOME A STRING

Therefore, the result of serial() should be considered!
Functions will become STRING as well!

The "null" inside should be seen as 'null' (which has .isNull = true).

*/
// Specify what will not be copied.
const set<string> nocopy = { ".__type__", ".__inherits__", ".__arg__", ".__must_inherit__", ".__no_inherit__" };
// Specify what will not be lookup.
const set<string> magics = { ".__type__", ".__inherits__", ".__arg__", ".__must_inherit__", ".__no_inherit__", ".__init__", ".__hidden__", ".__shared__" };
 class varmap {
	public:
		
		using value_type = intValue;
		using single_mapper = map<string, value_type>;

		typedef vector<single_mapper>::reverse_iterator			vit;
		typedef single_mapper::iterator							mit;
		
		struct referrer {

			// Not recommended to use the default constructor,
			// since it may crash!
			referrer(string og = "", varmap *src = nullptr) : origin_name(og), source(src) {

			}

			intValue& getValue(string dot_member = "") {
				string expand = "";
				if (dot_member.length()) expand = string(".") + dot_member;
				return this->source->operator[](origin_name + expand);
			}

			bool count(string dot_member = "") {
				string expand = "";
				if (dot_member.length()) expand = string(".") + dot_member;
				return this->source->count(origin_name + expand);
			}

			string origin_name;
			varmap *source;
		};

		varmap() {

		}
		void push() {
			vs.push_back(single_mapper());
		}
		void pop() {
			if (vs.size()) vs.pop_back();
		}
		bool count(string key) {
			if (!key.length()) return false;
			vector<string> spls = split(key, '.', 1);
			string dm = "";
			if (spls.size() >= 2) dm = spls[1];
			if (have_referrer(spls[0])) return ref[spls[0]].count(dm);
				for (vit i = vs.rbegin(); i != vs.rend(); i++) {
					if (i->count(key)) {
						return true;
					}
				}
				if (glob_vs.count(key)) return true;
				return false;
		}
		// Before call THIS check it
		void set_referrer(string here_name, referrer ref) {
			// As a template, replace others ...
			size_t last_pos = here_name.find_last_of('.');
			vector<string> spls = { here_name.substr(0, last_pos), "" };
			if (last_pos < string::npos) spls[1] = here_name.substr(last_pos + 1);
			string dm = "";
			if (spls.size() >= 2) dm = spls[1];
			if (have_referrer(spls[0]) && dm.length()) {
				// Set to its actual referrer
				auto &re = this->ref[spls[0]];
				re.source->set_referrer(re.origin_name + "." + spls[1], ref);
			}
			else {
				this->ref[here_name] = ref;
			}
		}
		void set_referrer(string here_name, string origin_name, varmap *origin) {
			if (here_name == "" || origin_name == "" || origin == nullptr) return;
			//ref[here_name] = referrer(origin_name, origin);
			set_referrer(here_name, referrer(origin_name, origin));
		}
		bool have_referrer(string here_name) {
			return ref.count(here_name);
		}
		void clean_referrer(string here_name) {
			if (have_referrer(here_name)) ref.erase(here_name);
		}
		// If return object serial, DON'T MODIFY IT !
		value_type& operator[](string key) {
#pragma region Debug Purpose
			//cout << "Require key: " << key << endl;
#pragma endregion
			// Shouldn't be LF in key.
			for (size_t i = 0; i < key.length(); i++) {
				if (key[i] == '\n') key.erase(key.begin() + i);
			}
			// Find where it is
			bool is_sharing = false;
			if (key != "__is_sharing__" && this->operator[]("__is_sharing__").str == "1") {
				is_sharing = true;
			}
			if (key == "this" || (key.substr(0, 5) == "this.")) {
				// Must be class
				if (is_sharing || (!have_referrer("this"))) {
					raise_varmap_ce("Error: attempt to call 'this' in a shared function or non-class function");
					return trash;
				}
			}

			// Search for referrer
			/*auto gd = get_dot(key);
			if (have_referrer(gd.first)) {
				return this->ref[gd.first].getValue(gd.second);
			}
			else if (have_referrer(key)) {
				return this->ref[key].getValue();
			}// Or for whole directly.
			*/
			// Check where the direction is OK.
			vector<size_t> cutters;
			size_t res = 0;
			while ((res = key.find('.', res)) != string::npos) {
				cutters.push_back(res);
				res++;		// Or deadloop here
			}
			for (int it = cutters.size() - 1; it >= 0; it--) {
				size_t &i = cutters[it];
				string predot = key.substr(0, i);
				string incdot = key.substr(i+1);	// '.' is NOT included !!
				if (have_referrer(predot)) {
					return this->ref[predot].getValue(incdot);
				}
			}

			if (have_referrer(key)) {
				return this->ref[key].getValue();
			}

			if (key.find('.') != string::npos) {
				// Must in same layer
				vector<string> la = split(key, '.', 1);
				for (vit i = vs.rbegin(); i != vs.rend(); i++) {
					if (i->count(la[0])) {
						if (!i->count(key)) return (*i)[key] = null;
						if (unserial.count((*i)[key + ".__type__"].str)) {
							return (*i)[key];
						}
						else {
							auto se = serial(key);
							intValue res = se;
							res.isObject = true;
							return ((*i))[key] = res;
						}
						
					}
				}
				if (glob_vs.count(la[0])) {
					if (!glob_vs.count(key)) {
						return glob_vs[key] = null;
					}
					if (unserial.count(glob_vs[key + ".__type__"].str)) {
						return glob_vs[key];
					}
					else {
						
						auto se = serial(key);
						intValue res = se;
						res.isObject = true;
						return glob_vs[key] = res;
					}
					
				}
			}
			
				for (vit i = vs.rbegin(); i != vs.rend(); i++) {
					if (i->count(key)) {
						if (unserial.count((*i)[key + ".__type__"].str)) {
							return ((*i))[key];
						}
						else {
							auto se = serial(key);
							intValue res = se;
							res.isObject = true;
							return ((*i))[key] = res;
						}
					}
				}
				
				if (glob_vs.count(key)) {
					if (unserial.count(glob_vs[key + ".__type__"].str)) {
						return glob_vs[key];
					}
					else {
						auto se = serial(key);
						intValue res = se;
						res.isObject = true;
						return glob_vs[key] = res;
					}
				}
				
				return vs[vs.size() - 1][key];
			
		}
		value_type serial(string name) {
			for (vit i = vs.rbegin(); i != vs.rend(); i++) {
				if (i->count(name)) {
					return serial_from(*i, name);
				}
			}
			if (glob_vs.count(name)) {
				return serial_from(glob_vs, name);
			}
			return mymagic;
		}
		vector<value_type> get_member(string name, bool force_show = false) {
			for (vit i = vs.rbegin(); i != vs.rend(); i++) {
				if (i->count(name)) {
					return get_member_from(*i, name, force_show);
				}
			}
			if (glob_vs.count(name)) {
				return get_member_from(glob_vs, name, force_show);
			}
			return vector<value_type>();
		}
		void deserial(string name, string serial) {
			if (!beginWith(serial, mymagic)) {
				return;
			}
			serial = serial.substr(mymagic.length());
			vector<string> lspl = split(serial, '\n', -1, '\"', '\\', true);
			for (auto &i : lspl) {
				vector<string> itemspl = split(i, '=', 1);
				if (itemspl.size() < 2) itemspl.push_back("null");
				(*this)[name + itemspl[0]] = getValue(itemspl[1], *this);
			}
			(*this)[name] = null;
		}
		void global_deserial(string name, string serial) {
			if (!beginWith(serial, mymagic)) {
				return;
			}
			serial = serial.substr(mymagic.length());
			vector<string> lspl = split(serial, '\n', -1, '\"', '\\', true);
			for (auto &i : lspl) {
				vector<string> itemspl = split(i, '=', 1);
				if (itemspl.size() < 2) itemspl.push_back("null");
				this->set_global(name + itemspl[0], getValue(itemspl[1], *this));
			}
			this->set_global(name, null);
		}
		void tree_clean(string name) {
			if (have_referrer(name)) {
				this->clean_referrer(name);
				return;
			}
				// Clean in my tree.
				for (auto i = vs.rbegin(); i != vs.rend(); i++) {
					if (i->count(name)) {
						i->erase(name);
						vector<string> to_delete;
						for (auto &j : (*i)) {
							if (beginWith(j.first, name + ".")) {
								// Delete!
								to_delete.push_back(j.first);
							}
						}
						for (auto &j : to_delete) {
							i->erase(j);
						}
						return;
					}
				}
		}
		void transform_referrer_from(string here_name, varmap &transform_from, string transform_from_referrer) {
			if (!transform_from.have_referrer(transform_from_referrer)) return;
			auto &refs = transform_from.ref[transform_from_referrer];
			this->set_referrer(here_name, refs.origin_name, refs.source);
		}
		static void set_global(string key, value_type value) {
			glob_vs[key] = value;
		}
		static void declare_global(string key) {
			set_global(key, null);
		}
		void declare(string key) {
			vs[vs.size() - 1][key] = null;
		}
		void set_this(varmap *source, string name) {
			set_referrer("this", name, source);
		}
		// This function is deprecated.
		void dump() {
			specialout();
			cout << "*** VARMAP DUMP ***" << endl;
			for (vit i = vs.rbegin(); i != vs.rend(); i++) {
				for (mit j = i->begin(); j != i->end(); j++) {
					cout << j->first << " = ";
					j->second.output();
					cout << endl;
				}
				cout << endl;
			}
			cout << "global:" << endl;
			for (mit i = glob_vs.begin(); i != glob_vs.end(); i++) {
				cout << i->first << " = ";
				i->second.output();
				cout << endl;
			}
			cout << "*** END OF DUMP ***" << endl;
			endout();
		}
		static void copy_inherit(string from, string dest) {
			while (from[from.length() - 1] == '\n') from.pop_back();
			while (dest[dest.length() - 1] == '\n') dest.pop_back();
			for (auto &i : glob_vs) {
				if (beginWith(i.first, from + ".")) {
					bool flag = false;
					for (auto tests : nocopy) {
						if (beginWith(i.first, from + tests)) flag = true;
					}
					if (flag) continue;
					vector<string> spl = split(i.first, '.', 1);
					glob_vs[dest + "." + spl[1]] = i.second;
					glob_vs[dest + "." + from + "@" + spl[1]] = i.second;
				}
			}
		}

		//string															this_name = "";
		//varmap															*this_source;
			// Where 'this' points. use '.'

		const string mymagic = "__object$\n";
private:
	vector<value_type> get_member_from(single_mapper &obj, string name, bool force_show = false) {
		vector<value_type> result;
		string mytype = (*this)[name + ".__type__"].str;
		if (unserial.count(mytype)) mytype = "";
		for (auto &i : obj) {
			// Only lookup for one
			size_t dpos = i.first.find_first_of('.');
			if (dpos >= i.first.length()) continue;
			string keyname = i.first.substr(dpos);
			if (countOf(i.first, '.') == 1) {
				bool isshown = true;
				if ((!force_show) && (mytype.length())) {
					for (auto &j : magics) {
						if (i.first.find(j) != string::npos) {
							isshown = false;
							break;
						}
					}
					if (isshown) {
						string hiddener = mytype + keyname + ".__hidden__";
						if ((*this)[hiddener].str == "1") isshown = false;
					}
				}
				if (force_show || isshown) {
					result.push_back(intValue(keyname.substr(1)));
				}
			}
		}
		return result;
	}
	// serial and serial_from does NOT support RAW REFERRER.
	intValue serial_from(single_mapper &obj, string name) {
		string tmp = mymagic;
		const static string sdot = ".";
		for (auto &j : obj) {
			if (beginWith(j.first, name + ".")) {
				//vector<string> spl = split(j.first, '.', 1);
				vector<string> spl = { "","" };
				size_t fl = j.first.find('.', j.first.find(name) + name.length());
				if (fl == string::npos) continue;
				spl[0] = j.first.substr(0, fl);
				if (spl[0] != name) continue;
				spl[1] = j.first.substr(fl + 1);
				tmp += string(".") + spl[1] + "=" + j.second.unformat() + "\n";
			}
		}
		// Deep copy in default!
		for (auto &j : this->ref) {
			if (beginWith(j.first, name + ".")) {
				vector<string> spl = { "","" };
				size_t fl = j.first.find('.', j.first.find(name) + name.length());
				if (fl == string::npos) continue;
				spl[0] = j.first.substr(0, fl);
				if (spl[0] != name) continue;
				spl[1] = j.first.substr(fl + 1);
				auto val = j.second.getValue();
				if (unserial.count(j.second.getValue("__type__").str)) {
					// Simple values:
					tmp += sdot + spl[1] + "=" + val.unformat() + "\n";
				}
				else {
					// Deserial it at once:
					string serial = val.str;
					if (!beginWith(serial, mymagic)) {
						continue;
					}
					serial = serial.substr(mymagic.length());
					vector<string> lspl = split(serial, '\n', -1, '\"', '\\', true);
					for (auto &i : lspl) {
						vector<string> itemspl = split(i, '=', 1);
						if (itemspl.size() < 2) itemspl.push_back("null");
						tmp += sdot + spl[1] + itemspl[0] + "=" + itemspl[1] + "\n";
					}
				}
				
			}
		}
		return intValue(tmp);
	}
	const set<string> unserial = { "", "function", "class", "null" };
	static pair<string, string> get_dot(string expr) {
		size_t last_pos = expr.find_last_of('.');
		pair<string, string> spls = { expr.substr(0, last_pos), "" };
		if (last_pos < string::npos) spls.second = expr.substr(last_pos + 1);
		string dm = "";
		return spls;
	}
	
	// 'this' is a special reference.
	map<string, referrer>													ref;
	vector<map<string, value_type> >										vs;
	// Save evalable thing, like "" for string
		static map<string, value_type>										glob_vs;
};

map<string, varmap::value_type> varmap::glob_vs;

void raiseError(intValue raiseValue, varmap &myenv, string source_function, size_t source_line, double error_id, string error_desc) {
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

// Ignorer can be quote-like char.
string formatting(string origin, char dinner = '\\', char ignorer = -1) {
	string ns = "";
	bool dmode = false, ign = false;
	for (size_t i = 0; i < origin.length(); i++) {
		if (origin[i] == ignorer && (!dmode)) {
			ign = !ign;
		}
		if (origin[i] == dinner && (!dmode) && (!ign)) {
			dmode = true;
		}
		else {
			ns += origin[i];
			dmode = false;
		}
	}
	return ns;
}

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

// If save_quote, formatting() will not process anything inside quote.
intValue getValue(string single_expr, varmap &vm, bool save_quote) {
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
				bool str = false, dmode = false;
				if (spl.size() >= 2) {	// Procees with arguments ...
					//arg = split(spl[1], ',');
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
					if (spl.size() >= 2) arg.push_back(spl[1]);
				}
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
							nvm.deserial(array_arg + dots + to_string(i), re.str);
						}
						else {
							nvm.deserial(argname[i], re.str);
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
				nvm[array_arg + dots + "length"] = intValue(arg.size() + external);
			}
			if (set_this.length()) nvm.set_this(&vm, set_this);
			if (set_no_this) {
				nvm["__is_sharing__"].set_str("1");
			}
			string s = vm[spl[0]].str;
			if (vm[spl[0]].isNull || s.length() == 0) {
				raise_gv_ce(string("Warning: Call of null function ") + spl[0]);
			}
			__spec++;
			auto r = run(s, nvm, spl[0]);
			__spec--;
			if (r.isNumeric && neg < 0) {
				r = intValue(-r.numeric);
			}
			return r;
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
int priority(char op) {
	switch (op) {
	case ')': case ':':	// Must make sure ':' has the most priority
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
	default:
		return -1;
	}
}

int priority(string op) {
	if (!op.length()) return -1;
	return priority(op[0]);
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

// Code must be checked
intValue calculate(string expr, varmap &vm) {
	if (expr.length() == 0) return null;
	stack<string> op;
	stack<intValue> val;
	string operand = "";
	bool cur_neg = false, qmode = false, dmode = false;
	int ignore = 0, op_pr;

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
				while (int(i)-t>0 && expr[i - t] == '(') t++;
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
					if (operand.length()) {
						if (expr[i] == ':') {
							val.push(intValue(operand));
						}
						else {
							auto_push();
						}
						cur_neg = false;
					}
					else if (!op.empty()) {	// Can't have something like &a& -> a&&
						string top_op = op.top();
						if (top_op.length()) {
							switch (top_op[0]) {
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
								if (expr[i] == '=' || expr[i] == top_op[0]) {
									op.pop();
									op.push({ top_op[0], expr[i] });
									goto end_of_pusher;
								}
								break;
							default:
								break;
							}
						}
					}
					auto_pop(my_pr);
					op.push(string({expr[i]}));
					operand = "";
				end_of_pusher:;
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
		auto_push();
		cur_neg = false;
	}
	auto_pop();
	return val.top();
}

#define parameter_check(req) do {if (codexec.size() < req) {raise_ce("Error: required parameter not given") ;return null;}} while (false)
#define parameter_check2(req,ext) do {if (codexec2.size() < req) {raise_ce(string("Error: required parameter not given in sub command ") + ext); return null;}} while (false)
#define parameter_check3(req,ext) do {if (codexec3.size() < req) {raise_ce(string("Error: required parameter not given in sub command ") + ext); return null;}} while (false)
#define dshell_check(req) do {if (spl.size() < req) {cout << "Bad command" << endl; goto dend;}} while (false)

map<int, FILE*> files;

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
			jmper[origin].push_back(jmper[origin][jmper[origin].size()-1]);
			return jmper[origin][jmper[origin].size() - 1];
		}
	}

	bool revert(size_t origin, size_t revert_from, bool omit = false) {
		bool flag = false;
		while (jmper[origin].size() && jmper[origin][jmper[origin].size()-1] == revert_from) {
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

typedef intValue(*bcaller)(string,varmap&);
map<string, bcaller> intcalls;

int random() {
	static int seed = time(NULL);
	srand(seed);
	return seed = rand();
}

inline bool haveContent(string s, char filter = '\t') {
	for (auto &i : s) if (i != filter) return true;
	return false;
}

size_t getLength(int fid) {
	size_t cur = ftell(files[fid]);
	fseek(files[fid], 0, SEEK_END);
	size_t res = ftell(files[fid]);
	fseek(files[fid], cur, SEEK_SET);
	return res;
}

// This 'run' will skip ALL class and function declarations.
// Provided environment should be pushed.
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
		else if (codexec[0] == "class" || codexec[0] == "function" || codexec[0] == "error_handler:") {
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
			cout << calculate(codexec[1], myenv).str;
		}
		else if (codexec[0] == "return") {
			parameter_check(2);
			return calculate(codexec[1], myenv);
		}
		else if (codexec[0] == "raise") {
			parameter_check(2);
			raiseError(calculate(codexec[1], myenv), myenv, fname, execptr + 1, -1, "User define error");
		}
		else if (codexec[0] == "set" || codexec[0] == "declare") {
			parameter_check(2);
			vector<string> codexec2 = split(codexec[1], '=', 1);
			parameter_check2(2,"set");
			if (codexec2[0][0]=='$') {
				codexec2[0].erase(codexec2[0].begin());
				codexec2[0] = calculate(codexec2[0], myenv).str;
			}
			else if (codexec2[0].find(":") != string::npos) {
				codexec2[0] = curexp(codexec2[0], myenv);
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
				myenv.deserial(codexec2[0], getValue(codexec3[1], myenv, true).str);
			}
			else if (beginWith(codexec2[1], "serial ")) {
				vector<string> codexec3 = split(codexec2[1], ' ', 1);
				parameter_check3(2, "Operator number");
				myenv[codexec2[0]] = myenv.serial(codexec3[1]);
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
						myenv.deserial(codexec2[0], res.str);
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
			else if (beginWith(codexec2[1], "__typeof")) {	// Not necessary any longer
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
			myenv[codexec2[0] + ".length"] = intValue(result.size());
}
#pragma endregion
			else {
				auto res = calculate(codexec2[1], myenv);
				if (res.isObject) {
					myenv.deserial(codexec2[0], res.str);
				}
				else {
					myenv[codexec2[0]] = res;
				}
			}
			
		}
		else if (codexec[0] == "global") {	// Todo: add 'new' , 'object', 'serial', ... After varmap is changed
		parameter_check(2);
		vector<string> codexec2 = split(codexec[1], '=', 1);
		parameter_check2(2, "set");
		if (codexec2[0][0] == '$') {
			codexec2[0].erase(codexec2[0].begin());
			codexec2[0] = calculate(codexec2[0], myenv).str;
		}
		else if (codexec2[0].find(":") != string::npos) {
			codexec2[0] = curexp(codexec2[0], myenv);
		}
		auto res = calculate(codexec2[1], myenv);
		if (res.isObject) {
			myenv.global_deserial(codexec2[0], res.str);
		}
		else {
			myenv.set_global(codexec2[0], res);
		}
		}
		else if (codexec[0] == "if" || codexec[0] == "elif") {
			parameter_check(2);
			// Certainly you have ':'
			if (codexec[1].length()) codexec[1].pop_back();
			if (calculate(codexec[1], myenv).boolean()) {
				// True, go on execution, and set jumper to the end of else before any else / elif
				size_t rptr = execptr;								// Where our code ends
				while (rptr < codestream.size()-1 && getIndentRaw(codestream[++rptr]) > ind);	// Go on until aligned else / elif
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
		else if (codexec[0] == "else:") {
			// Go on executing
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
				while (execptr != codestream.size()-1 ) {
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
			if (ide < addin ) {
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
				else if (op == "read") {
					// file read [store]=[var]
					codexec3 = split(codexec2[1], '=', 1);
					int fid = calculate(codexec3[1], myenv).numeric;
					bool rs = files.count(fid)?feof(files[fid]):0;
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
						myenv[cn + ".length"] = intValue(len);
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
							long64 clen = myenv[cn + ".length"].numeric;
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
		else if (codexec[0] == "import") {
			// Do nothing
		}
		else if (codexec[0] == "__dev__") {
			if (codexec.size() >= 2 && codexec[1] == "time_set") {
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
				myenv[save_target] = result;
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
const set<char> to_trim = {' ', '\n', '\r', -1};

intValue preRun(string code, map<string, intValue> required_global = {}, map<string, bcaller> required_callers = {}) {
	// Should prepare functions for it.
	string fname = "Runtime preproessor";
	varmap myenv;
	myenv.push();
	// Preset constants
#pragma region Preset constants
	myenv.set_global("LF", intValue("\n"));
	myenv.set_global("TAB", intValue("\t"));
	myenv.set_global("BKSP", intValue("\b"));
	myenv.set_global("ALERT", intValue("\a"));
	myenv.set_global("CLOCKS_PER_SEC", intValue(CLOCKS_PER_SEC));
	myenv.set_global("true", intValue(1));
	myenv.set_global("false", intValue(0));
	myenv.set_global("err.__type__", intValue("exception"));			// Error information
	myenv.set_global("__error_handler__", intValue("call set_color,14\nprint err.description+LF+err.value+LF\ncall set_color,7"));	// Preset error handler
	myenv.set_global("__file__", intValue(env_name));
	// Insert more global variable
	for (auto &i : required_global) {
		myenv.set_global(i.first, i.second);
	}
#pragma endregion
#pragma region Preset calls
	intcalls["sleep"] = [](string args, varmap &env) -> intValue {
		Sleep(DWORD(calculate(args, env).numeric));
		return null;
	};
	intcalls["system"] = [](string args, varmap &env) -> intValue {
		system(calculate(args, env).str.c_str());
		return null;
	};
	intcalls["exit"] = [](string args, varmap &env) -> intValue {
		exit(int(calculate(args, env).numeric));
		return null;
	};
	intcalls["set_color"] = [](string args, varmap &env) -> intValue {
		setColor(DWORD(calculate(args, env).numeric));
		return null;
	};
	// Remind you that eval is dangerous!
	intcalls["eval"] = [](string args, varmap &env) -> intValue {
		return run(calculate(args, env).str, env, "Internal eval()");
	};
	intcalls["__bnot"] = [](string args, varmap &env) -> intValue {
		return ~ulong64(calculate(args, env).numeric);
	};
	// It is better to add more functions by intcalls, not set
#define math_extension(funame) intcalls["_maths_" #funame] = [](string args, varmap &env) -> intValue { \
		return intValue(funame(calculate(args, env).numeric)); \
	}
	math_extension(sin);
	math_extension(cos);
	math_extension(tan);
	math_extension(asin);
	math_extension(acos);
	math_extension(atan);
	math_extension(sqrt);
	intcalls["_trim"] = [](string args, varmap &env) -> intValue {
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

	vector<string> sc = split(code, '\n', -1, '\"', '\\', true);
	codestream.insert(codestream.end()-1,sc.begin(),sc.end());
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
				myenv.set_global(cfname + ".__type__", intValue("function"));
				myenv.set_global(cfname + ".__arg__", cfargs);
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
						f = fopen((env_dir + codexec2[1]).c_str(), "r");
						if (f != NULL) {
							while (!feof(f)) {
								fgets(buf1, 65536, f);
								codestream.push_back(buf1);
							}
						}
					}
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
		}
		
	}
	if (cfname.length()) {
		myenv.set_global(cfname, curfun);
		myenv.set_global(cfname + ".__type__", intValue("function"));
		myenv.set_global(cfname + ".__arg__", cfargs);
	}
	
	//return null;
	// End

	intValue res = run(code, myenv, "Main function");

	// For debug propose
	//myenv.dump();
	return res;
}


int main(int argc, char* argv[]) {
	stdouth = GetStdHandle(STD_OUTPUT_HANDLE);

	// Test: Input code here:
#pragma region Compiler Test Option
#if _DEBUG
	string code = "", file = "test4.blue";
	in_debug = true;
	no_lib = false;

	if (code.length()) {
		specialout();
		cout << code;
		cout << endl << "-------" << endl;
		endout();
	}
#else
	// DO NOT CHANGE
	string code = "", file = "";
	in_debug = false;
	no_lib = false;
#endif
	string version_info = string("BlueBetter Interpreter\nVersion 1.16\nCompiled on ") + __DATE__ + " " + __TIME__;
#pragma endregion
	// End

	if (argc <= 1 && !file.length() && !code.length()) {
		cout << "Usage: " << argv[0] << " filename [options]";
		return 1;
	}
	
#pragma region Read Options

	if (!file.length() && !code.length()) {
		file = argv[1];
	}

	if (file == "--version") {
		// = true;
		cout << version_info << endl;
		return 0;
	}

	env_name = file;
	map<string, intValue> reqs = { {"FILE_NAME", intValue(file)} };
	map<string, bcaller> callers;	// Insert your requirements here

	for (int i = 2; i < argc; i++) {
		string opt = argv[i];
		if (opt == "--debug") {
			in_debug = true;
		}
		else if (beginWith(opt, "--const:")) {
			// String values only
			vector<string> spl = split(opt, ':', 1);
			if (spl.size() < 2) {
				curlout();
				cout << "Error: Bad format of --const option" << endl;
				endout();
				return 1;
			}
			vector<string> key_value = split(spl[1], '=', 1);
			if (key_value.size() < 2) {
				reqs[key_value[0]] = null;
			}
			else {
				reqs[key_value[0]] = intValue(key_value[1]);
			}


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
			if (spl[0] == "quit") {
				exit(0);
			}
		} while (command != "run");
		endout();
	}

	return preRun(code, reqs, callers).numeric;
}
