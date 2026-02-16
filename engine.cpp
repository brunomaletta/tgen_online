#include <emscripten.h>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <cctype>

#include "tgen/src/tgen.h"

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

static std::set<int> to_int_set(const std::string& s){
	std::set<int> st;
	std::stringstream ss(s);
	int x; char c;
	while(ss>>x){ st.insert(x); ss>>c; }
	return st;
}

/* ================= sequence ================= */

struct SeqObj : Object {
	tgen::sequence<int> seq;

	SeqObj(tgen::sequence<int> s):seq(std::move(s)) {}

	std::shared_ptr<Object> call(
			const std::string& m,
			const std::vector<std::string>& a) override
	{
		if(m=="equal_range")
			return std::make_shared<SeqObj>(seq.equal_range(to_int(a[0]),to_int(a[1])));

		if(m=="distinct")
			return std::make_shared<SeqObj>(seq.distinct(to_int_set(a[0])));

		if(m=="gen"){
			std::ostringstream ss;
			ss<<seq.gen();
			captured+=ss.str()+"\n";
			return nullptr;
		}

		throw std::runtime_error("Unknown sequence method: "+m);
	}

};

/* ================= permutation ================= */

struct PermObj : Object {
	tgen::permutation perm;

	PermObj(tgen::permutation p):perm(std::move(p)){}

	std::shared_ptr<Object> call(
			const std::string& m,
			const std::vector<std::string>& a) override
	{
		captured += "TEST";
		if(m=="gen"){
			std::ostringstream ss;
			ss<<perm.gen();
			captured+=ss.str()+"\n";
			return nullptr;
		}
		throw std::runtime_error("Unknown permutation method: "+m);
	}

};

/* ================= Factory ================= */

static std::shared_ptr<Object> make_object(const std::string& name,const std::vector<std::string>& args){
	if(name=="sequence<int>")
		return std::make_shared<SeqObj>(
				tgen::sequence<int>(to_int(args[0]),to_int(args[1]),to_int(args[2]))
				);

	if(name=="permutation")
		return std::make_shared<PermObj>(
				tgen::permutation(to_int(args[0]))
				);

	throw std::runtime_error("Unknown generator "+name);

}

/* ================= Parser ================= */

static std::vector<std::string> split_args(std::string s){
	std::vector<std::string> r;
	std::string cur;
	int brace=0;
	for(char c:s){
		if(c=='{' ) brace++;
		if(c=='}' ) brace--;
		if(c==',' && brace==0){ r.push_back(cur); cur.clear();}
		else cur+=c;
	}
	if(!cur.empty()) r.push_back(cur);
	return r;
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* run(const char* code) {
	captured.clear();
	std::string src(code);

	try{

		auto start=src.find("tgen::");
		auto end=src.rfind(".gen()");
		if(start==std::string::npos||end==std::string::npos)
			throw std::runtime_error("No generator expression found");

		std::string expr=src.substr(start,end-start+6);

		// constructor
		auto p=expr.find("(");
		std::string name=expr.substr(6,p-6);

		auto close=expr.find(")");
		auto args=split_args(expr.substr(p+1,close-p-1));

		auto obj=make_object(name,args);

		size_t pos=close+1;

		while(pos<expr.size()){
			while (expr[pos] == '\n' or expr[pos] == ' ') pos++;
			if(expr[pos]!='.') break;
			pos++;

			auto m_end=expr.find("(",pos);
			std::string method=expr.substr(pos,m_end-pos);

			captured += "method: " + method + "\n";

			auto a_end=expr.find(")",m_end);
			auto margs=split_args(expr.substr(m_end+1,a_end-m_end-1));

			obj=obj->call(method,margs);
			pos=a_end+1;
			if(!obj) break;
		}

	}catch(const std::exception& e){
		captured=e.what();
	}

	return captured.c_str();

}

