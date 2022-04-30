#include "json17.h"


int main()
{
	using json17::json;

	json j;
	j["first"] = json::object{ 
		{"1", json::array{123,"456",false,nullptr}},
		{"2", json::object{{"123", "456"}, {"877", nullptr}}} 
	};
	j["second"] = nullptr;
	j["third"] = json::array({ false,7e40 });
	j.get_object().emplace("fourth", json::object());
	auto& jarr = j["third"].get_array();
	jarr.push_back(9);
	jarr.push_back(json::object());

	printf("j[first].size = %zu\n", j["first"]["1"].get_array().size());
	printf("j[third][1] = %e\n", j["third"][1].get_number());

	printf("%s\n", j.dumps(2).c_str());
	
	std::string res;
	auto biter = std::back_inserter(res);
	j["first"].dump(std::back_inserter(res));
	res += '\n';
	j["second"].dump(res.begin());
	printf("%s\n\n", res.c_str());

	j.dump(std::cout, 4);
	std::cout << std::endl;
	j["third"].dump(std::cout, json17::dump_options(1, '\t'));

	json jp;
	jp = json::parse("123.45e6");
	jp.dump(std::cout);
	std::cout << "\n";
	jp.loads("false");
	jp.dump(std::cout);
	std::cout << "\n";
	jp.loads("null");
	jp.dump(std::cout);
	return 0;
}