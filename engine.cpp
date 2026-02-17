#include <emscripten.h>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <cctype>

#include "tgen/src/tgen.h"

static std::vector<std::string> split_args_(const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;

    bool in_quotes = false;

    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];

        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }

        if (!in_quotes && std::isspace((unsigned char)c)) {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        }
        else {
            cur += c;
        }
    }

    if (!cur.empty())
        out.push_back(cur);

    return out;
}

void string_to_argv(const std::string& cmd, int& argc, char**& argv)
{
    auto tokens = split_args_(cmd);

    argc = (int)tokens.size();
    argv = new char*[argc + 1];

    for (int i = 0; i < argc; ++i)
    {
        argv[i] = new char[tokens[i].size() + 1];
        std::strcpy(argv[i], tokens[i].c_str());
    }

    argv[argc] = nullptr;
}

void free_argv(int argc, char** argv)
{
    for (int i = 0; i < argc; ++i)
        delete[] argv[i];

    delete[] argv;
}

static std::string captured;

/* ================= Runtime Object ================= */

struct Object {
	virtual ~Object() {}
	virtual std::shared_ptr<Object> call(
			const std::string& method,
			const std::vector<std::string>& args)=0;
	virtual std::string to_string(){ return ""; }
};

/* ================= Helpers ================= */

static int to_int(const std::string& s){
	return std::stoi(s);
}

static std::set<int> to_int_set(std::string s){
	std::set<int> result;
	// remove braces
	if(!s.empty() && s.front()=='{') s.erase(s.begin());
	if(!s.empty() && s.back()=='}')  s.pop_back();

	std::string cur;
	for(char c : s)
	{
		if(c==',')
		{
			if(!cur.empty()){
				result.insert(std::stoi(cur));
				cur.clear();
			}
		}
		else if(!isspace(c))
			cur+=c;
	}

	if(!cur.empty())
		result.insert(std::stoi(cur));

	return result;
}

static std::vector<int> to_int_vec(std::string s){
	std::vector<int> result;
	// remove braces
	if(!s.empty() && s.front()=='{') s.erase(s.begin());
	if(!s.empty() && s.back()=='}')  s.pop_back();

	std::string cur;
	for(char c : s)
	{
		if(c==',')
		{
			if(!cur.empty()){
				result.push_back(std::stoi(cur));
				cur.clear();
			}
		}
		else if(!isspace(c))
			cur+=c;
	}

	if(!cur.empty())
		result.push_back(std::stoi(cur));

	return result;
}

/* ================= sequence ================= */

struct SeqInstObj : Object {
	tgen::sequence<int>::instance inst;

	SeqInstObj(tgen::sequence<int>::instance v) : inst(std::move(v)) {}

	std::shared_ptr<Object> call(
			const std::string& m,
			const std::vector<std::string>& a) override {
		if(m=="reverse")
			return std::make_shared<SeqInstObj>(inst.reverse());
		if(m=="sort")
			return std::make_shared<SeqInstObj>(inst.sort());

		throw std::runtime_error("Unknown instance method: " + m);
	}

	std::string to_string() override {
		std::ostringstream ss;
		ss << inst;
		return ss.str();
	}
};

struct SeqObj : Object {
	tgen::sequence<int> seq;

	SeqObj(tgen::sequence<int> s) : seq(std::move(s)) {}

	std::shared_ptr<Object> call(
			const std::string& m,
			const std::vector<std::string>& a) override {
		if (m=="set")
			return std::make_shared<SeqObj>(seq.set(to_int(a[0]),to_int(a[1])));

		if (m=="equal")
			return std::make_shared<SeqObj>(seq.equal(to_int(a[0]),to_int(a[1])));

		if (m=="equal_range")
			return std::make_shared<SeqObj>(seq.equal_range(to_int(a[0]),to_int(a[1])));

		if (m=="distinct") {
			if (a.size() == 0)
				return std::make_shared<SeqObj>(seq.distinct());
			else
				return std::make_shared<SeqObj>(seq.distinct(to_int_set(a[0])));
		}

		if (m=="different")
			return std::make_shared<SeqObj>(seq.different(to_int(a[0]), to_int(a[1])));

		if (m=="gen")
			return std::make_shared<SeqInstObj>(seq.gen());

		throw std::runtime_error("Unknown sequence method: " + m);
	}

};

/* ================= permutation ================= */

struct PermInstObj : Object {
	tgen::permutation::instance inst;

	PermInstObj(tgen::permutation::instance v) : inst(std::move(v)) {}

	std::shared_ptr<Object> call(
			const std::string& m,
			const std::vector<std::string>& a) override {
		if(m=="reverse")
			return std::make_shared<PermInstObj>(inst.reverse());
		if(m=="sort")
			return std::make_shared<PermInstObj>(inst.sort());
		if(m=="inverse")
			return std::make_shared<PermInstObj>(inst.inverse());
		if(m=="add_1")
			return std::make_shared<PermInstObj>(inst.add_1());

		throw std::runtime_error("Unknown instance method: " + m);
	}

	std::string to_string() override {
		std::ostringstream ss;
		ss << inst;
		return ss.str();
	}

};

struct PermObj : Object {
	tgen::permutation perm;

	PermObj(tgen::permutation p):perm(std::move(p)){}

	std::shared_ptr<Object> call(
			const std::string& m,
			const std::vector<std::string>& a) override
	{
		if (m=="set")
			return std::make_shared<PermObj>(perm.set(to_int(a[0]), to_int(a[1])));
		if (m=="gen") {
			if (a.size() == 0) return std::make_shared<PermInstObj>(perm.gen());
			return std::make_shared<PermInstObj>(perm.gen(to_int_vec(a[0])));
		}
		throw std::runtime_error("Unknown permutation method: " + m);
	}

};

/* ================= Factory ================= */

static std::shared_ptr<Object> make_object(const std::string& name, const std::vector<std::string>& args) {
	if (name=="sequence<int>")
		return std::make_shared<SeqObj>(
				tgen::sequence<int>(to_int(args[0]), to_int(args[1]), to_int(args[2]))
				);

	if (name=="permutation")
		return std::make_shared<PermObj>(
				tgen::permutation(to_int(args[0]))
				);

	throw std::runtime_error("Unknown generator " + name);

}

/* ================= Parser ================= */

static std::vector<std::string> split_args(std::string s) {
	std::vector<std::string> r;
	std::string cur;
	int brace=0;
	for (char c : s) {
		if (c=='{' ) brace++;
		if (c=='}' ) brace--;
		if (c==',' && brace == 0) {
			r.push_back(cur);
			cur.clear();
		}
		else cur+=c;
	}
	if (!cur.empty()) r.push_back(cur);
	return r;
}

inline bool updated = false;
inline std::string tgen_args, last_code;

extern "C" EMSCRIPTEN_KEEPALIVE const char* run(const char* code) {

	if (updated and code == last_code) return captured.c_str();
	updated = true;
	last_code = code;

	int argc;
	char** argv;
	string_to_argv(tgen_args, argc, argv);
	tgen::register_gen(argc, argv);
	free_argv(argc, argv);

	captured.clear();
	std::string src(code);

	try {
		auto start = src.find("tgen::");
		if (start == std::string::npos) throw std::runtime_error("not a tgen:: method");
		auto end = src.size();

		std::string expr = src.substr(start, end-start+6);

		// constructor
		auto p = expr.find("(");
		std::string name = expr.substr(6, p-6);

		auto close = expr.find(")");
		auto args = split_args(expr.substr(p+1, close-p-1));

		auto obj = make_object(name, args);

		size_t pos = close+1;

		while (pos < expr.size()) {
			while (expr[pos] == '\n' or expr[pos] == ' ') pos++;
			if (expr[pos]!='.') break;
			pos++;

			auto m_end = expr.find("(", pos);
			std::string method = expr.substr(pos, m_end - pos);

			auto a_end = expr.find(")", m_end);
			auto margs = split_args(expr.substr(m_end+1, a_end-m_end-1));

			obj = obj->call(method,margs);
			pos = a_end+1;
			if (!obj) break;
		}

		captured += obj->to_string() + "\n";

	} catch (const std::exception& e) {
		captured += e.what();
	}

	return captured.c_str();
}

extern "C" EMSCRIPTEN_KEEPALIVE void set_input_string(const char* str) {
    std::string s(str);
    // store globally, or recompute algorithm
	tgen_args = s;
	updated = false;
}
