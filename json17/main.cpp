#include "json17.h"

#include <fstream>


using json = json17::json_inplace;

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
	jp = json::parse("[false,123.45e6,true,{\"2\":null}, -8]");
	jp.dump(std::cout, 2);
	for (;;) {
		std::cout << "Input json in one line:\n";
		std::getline(std::cin, res);
		try {
			jp.loads(res);
			jp.dump(std::cout, 2);
			std::cout << std::endl;
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
	j.dump(std::cout, 2);
	j.dump(std::cout, json17::dump_options(1, '\t', true));
	std::ofstream ofs("out.json");
	if (ofs.is_open()) {
		j.dump(ofs, 2);
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
	j.load(ifs);
	std::cout << j.dumps(4);
	return 0;
}