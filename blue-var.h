#pragma once
#include "blue-lib.h"
#include "blue-platform-adaptor.h"

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

	using serial_type = map<string, intValue>;

	enum output_level {
		least,
		normal,
		most,
		recursive
	};

	static output_level current_output;

	// DO NOT WRITE THEM DIRECTLY!
	double								numeric;
	string								str;
	serial_type							serial_data;
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
			return intValue();
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
		this->isNumeric = false;
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
	intValue(serial_type serials) : serial_data(serials) {
		this->isObject = true;
	}

	// Usually use for debug proposes
	void output() {
		DWORD prec = nowcolor;
		if (isNull) {
			setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
			cout << "null";
		}
		else if (isNumeric) {
			setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			cout << numeric;
		}
		else if (isObject) {
			setColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			cout << "<" << serial_data["__type__"].str << " object at " << (this) << ">" << endl;
			if (current_output != least) {
				for (auto &i : serial_data) {
					if (current_output == normal) {
						if (i.first.find("__type__") != string::npos || i.first.find("__const__") != string::npos) continue;
					}
					setColor(FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
					cout << i.first;
					setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
					cout << ": ";
					if (i.second.isNull) {
						cout << "null" << endl;
						continue;
					}
					else if (i.second.isObject) {
						
						if (current_output == recursive) {
							i.second.output();
						}
						else {
							setColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
							cout << "<object>";
						}
						
					}
					else if (i.second.isNumeric) {
						setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
						cout << i.second.numeric;
					}
					else {
						setColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
						cout << i.second.str;
					}
					cout << endl;
				}
			}
		}
		else {
			setColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			cout << str;
		}
		setColor(prec);
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
intValue::output_level intValue::current_output = normal;	// This is suggested

thread_local intValue trash;	// trash: Return if incorrect varmap[] is called.

const set<string> nocopy = { ".__type__", ".__inherits__", ".__arg__", ".__must_inherit__", ".__no_inherit__" };
// Specify what will not be lookup.
const set<string> magics = { ".__type__", ".__inherits__", ".__arg__", ".__must_inherit__", ".__no_inherit__", ".__init__", ".__hidden__", ".__shared__", ".__const__", ".__is_prop__", ".__setter__" };
class varmap {
public:

	using serial_type = intValue::serial_type;

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

		void tree_clean(bool reserve_position = true) {
			// The referrer should be saved if the referrer refers to a referrer
			this->source->tree_clean(this->origin_name, reserve_position, true);
		}

		string origin_name;
		varmap *source;
	};
	varmap() {

	}
	varmap(const varmap &others) {
		copy_from(others);
	}
	void copy_from(const varmap &source) {
		this->vs = source.vs;
		this->ref = source.ref;
	}
	void push() {
		wait_for_perm();
		vs.push_back(single_mapper());
		release_perm();
	}
	void pop() {
		wait_for_perm();
		if (vs.size()) vs.pop_back();
		release_perm();
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
	void insert(mit begin, mit end) {
		this->vs.rbegin()->insert(begin, end);
	}
	// Before call THIS check it
	// return TRUE if success or FALSE if fail £¨Unused currently)
	bool set_referrer(string here_name, referrer ref) {
		wait_for_perm();
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
		release_perm();
		return true;
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
		//wait_for_perm();
		if (have_referrer(here_name)) ref.erase(here_name);
		//release_perm();	// No need at all!
	}
	// If return object serial, DON'T MODIFY IT !
	// TODO: Add lock for it?
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
		if (vs[0]["__is_sharing__"].str == "1") {
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
			string incdot = key.substr(i + 1);	// '.' is NOT included !!
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
		return intValue(serial_type());
	}
	value_type traditional_serial(string name) {
		for (vit i = vs.rbegin(); i != vs.rend(); i++) {
			if (i->count(name)) {
				return traditional_serial_from(*i, name);
			}
		}
		if (glob_vs.count(name)) {
			return traditional_serial_from(glob_vs, name);
		}
		return intValue(serial_type());
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
	void deserial(string name, serial_type serial) {
		wait_for_perm();
		for (auto &i : serial) {
			//vector<string> itemspl = split(i, '=', 1);
			//if (itemspl.size() < 2) itemspl.push_back("null");
			//(*this)[name + itemspl[0]] = getValue(itemspl[1], *this);
			(*this)[name + '.' + i.first] = i.second;
		}
		(*this)[name] = null;
		release_perm();
	}
	void traditional_deserial(string name, string serial) {
		wait_for_perm();
		if (!beginWith(serial, mymagic)) {
			release_perm();
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
		release_perm();
	}
	void traditional_global_deserial(string name, string serial) {
		wait_for_perm();
		if (!beginWith(serial, mymagic)) {
			release_perm();
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
		release_perm();
	}
	void global_deserial(string name, serial_type serial) {
		wait_for_perm();
		for (auto &i : serial) {
			this->set_global(name + '.' + i.first, i.second);
		}
		this->set_global(name, null);
		release_perm();
	}
	void tree_clean(string name, bool reserve_the_position = true, bool reserve_referrer = false) {
		wait_for_perm();
		if (have_referrer(name)) {
			// Can't reserve any position (referrer is global ...)
			// But sometimes (while setting, etc.), we need to reserve referrer,
			if (reserve_referrer) {
				this->ref[name].tree_clean(reserve_the_position);
			}
			else {
				this->clean_referrer(name);
			}
			release_perm();
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
				if (reserve_the_position) (*i)[name] = null;
				release_perm();
				return;
			}
		}
		release_perm();	// Do it in the end
	}
	void transform_referrer_from(string here_name, varmap &transform_from, string transform_from_referrer) {
		if (!transform_from.have_referrer(transform_from_referrer)) return;
		auto &refs = transform_from.ref[transform_from_referrer];
		this->set_referrer(here_name, refs.origin_name, refs.source);
	}
	// constant here is used for internal libraries.
	static void set_global(string key, value_type value, bool constant = false) {
		glob_vs[key] = value;
		if (constant) glob_vs[key + ".__const__"] = intValue("1");
	}
	static void declare_global(string key) {
		set_global(key, null);
	}
	static intValue& get_global(string key) {
		return glob_vs[key];
	}
	void declare(string key) {
		wait_for_perm();
		vs[vs.size() - 1][key] = null;
		release_perm();
	}
	void set_this(varmap *source, string name) {
		set_referrer("this", name, source);
	}
	// Generate a unused random name.
	string generate() {
		string name;
		do {
			name = this->mygenerate + to_string(random());
		} while (this->count(name));
		return name;
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
	static void copy_inherit(string from, string dest) {	// Not required, only use when single-thread preRun
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
	const string mygenerate = "__generate_";
	const set<string> unserial = { "", "function", "class", "null" };

protected:
	// This provide a smaller version of getValue, which doesn't require interpreter's environment:
	intValue getValue(string single_expr, varmap env, bool save_quote = false) {
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
		raise_varmap_ce("Unknown value in internal getValue() -- please check if object string contains runtime code");
		return intValue();
	}

private:

	mutex thread_protect;
	// Only use if modify it
	void wait_for_perm() {
		thread_protect.lock();
	}
	void release_perm() {
		thread_protect.unlock();
	}

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
	intValue serial_from(single_mapper &obj, string name) {
		// TODO: Avoid looping-serialer
		static thread_local unsigned entries = 0u;
		serial_type tmp;
		for (auto &j : obj) {
			if (beginWith(j.first, name + ".")) {
				//vector<string> spl = split(j.first, '.', 1);
				vector<string> spl = { "","" };
				size_t fl = j.first.find('.', j.first.find(name) + name.length());
				if (fl == string::npos) continue;
				spl[0] = j.first.substr(0, fl);
				if (spl[0] != name) continue;
				spl[1] = j.first.substr(fl + 1);
				//tmp += string(".") + spl[1] + "=" + j.second.unformat() + "\n";
				tmp[spl[1]] = j.second;
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
					//tmp += sdot + spl[1] + "=" + val.unformat() + "\n";
					tmp[spl[1]] = val;
				}
				else {
					// Deserial it at once:
					auto sub_serial = val.serial_data;
					for (auto &i : sub_serial) {
						tmp[spl[1] + '.' + i.first] = i.second;
					}
					tmp[spl[1]] = null;
				}

			}
		}
		return intValue(tmp);
	}
	intValue traditional_serial_from(single_mapper &obj, string name) {
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
