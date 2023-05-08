#include "blue.h"
using namespace std;

int main(int argc, char* argv[]) {
	standardPreparation();

	interpreter ip;

	// Test: Input code here:
#pragma region Compiler Test Option
#if _DEBUG
	string code = "", file = "test3.blue";
	ip.in_debug = true;
	ip.no_lib = false;

	if (code.length()) {
		specialout();
		cout << code;
		cout << endl << "-------" << endl;
		endout();
	}
#else
	// DO NOT CHANGE
	string code = "", file = "";
	ip.in_debug = false;
	ip.no_lib = false;
#endif
	string version_info = string("BlueBetter Interpreter\nVersion 1.25\nCompiled on ") + __DATE__ + " " + __TIME__;
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

	ip.env_name = file;
	map<string, intValue> reqs = { {"FILE_NAME", intValue(file)} };
	map<string, interpreter::bcaller> callers;	// Insert your requirements here

	for (int i = 2; i < argc; i++) {
		string opt = argv[i];
		if (opt == "--debug") {
			ip.in_debug = true;
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
		else if (beginWith(opt, "--include-from:")) {
			vector<string> spl = split(opt, ':', 1);
			if (spl.size() < 2) {
				curlout();
				cout << "Error: Bad format of --include-from option" << endl;
				endout();
				return 1;
			}
			ip.include_sources.push_back(spl[1]);
		}
		else if (opt == "--no-lib") {
			ip.no_lib = true;
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

	if (ip.in_debug) {
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

	return ip.preRun(code, reqs, callers).numeric;
}
