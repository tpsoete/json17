#include "json17.h"

#include <iostream>
#include <fstream>


using json = json17::json;

int main1()
{
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
	
	std::string res = j.dumps();
	printf("%s\n\n", res.c_str());

	json jp;
	jp = json::parse("[false,123.45e6,true,{\"2\":null}, -8]");
	std::cout << jp.dumps(1, '\t');
	for (;;) {
		std::cout << "Input json in one line:\n";
		std::getline(std::cin, res);
		try {
			jp.loads(res);
			std::cout << jp.dumps(2) << std::endl;
		}
		catch (const std::exception& e) {
			std::cout << e.what() << std::endl;
			break;
		}
	}
	return 0;
}

int main2()
{
	std::string str = R"(
{
	"123":"456\n\r",
	"this": [true, null, false, 127e25, -13, 7.e-34],
	"that": { "\u0033": "\ufffd\ufffd", "\ud852\uDF62": []},
	"what": [{}],
	"dcicxcl\bdsljfh": "null"
})";
	json j = json::parse(str);
	std::cout << j.dumps(2);
	std::ofstream ofs("out.json");
	if (ofs.is_open()) {
		ofs << j.dumps(2);
	}
	return 0;
}

int main()
{
	main2();
	main1();
	std::cout << "\n will read file from demo.json \n";
	system("pause");
	std::ifstream ifs("demo.json");
	if (!ifs.is_open()) {
		std::cout << "file not opened: demo.json\n";
		return 1;
	}
	json j;
	std::string str = std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
	j.loads(str);
	std::cout << j.dumps(4);
	return 0;
}