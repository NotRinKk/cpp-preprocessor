#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool Preprocess(const path& in_file, const path& out_file, const string& name, const vector<path>& include_directories) {
    ifstream read_file(in_file);
    if (!read_file.is_open()) {
        return false; 
    }

    ofstream write_file(out_file, ios::app);

    static regex relative_include_reg(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex dir_include_reg(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    smatch match;

    size_t line_count = 0u;
    string line;

    // Только если файл найден
    while (getline(read_file, line)) {
        ++line_count;
        // Если в строке содержится относительная #include-директива
        if (regex_search(line, match, relative_include_reg)) {

            path include = path((match[1]).str());
            path p = in_file.parent_path() / include;

            // Если файл не найден, поиск выполняется последовательно по всем элементам вектора
            if(!Preprocess(p, out_file, in_file.filename().string(), include_directories)){
                bool found;
                for (const auto& dir_entry : include_directories) {
                    found = Preprocess(dir_entry / include, out_file, in_file.string(), include_directories);
                    if(found) {
                        break;
                    }
                } 
                if (!found) {
                    cout << "unknown include file " << include.filename().string() << " at file " << name << " at line " << line_count << endl;
                    return false;  
                } 
            }
        } // Если в строке содержится "абсолютная" #include-директива
        else if (regex_search(line, match, dir_include_reg)) {
            path include = path((match[1]).str());
            bool found;
            // Поиск выполняется последовательно по всем элементам вектора
            for (const auto& dir_entry : include_directories) {
                found = Preprocess(dir_entry / include, out_file, in_file.string(), include_directories);
                 if(found) {
                    break;
                }
            } 
            if (!found) {
                cout << "unknown include file " << include.filename().string() << " at file " << name << " at line " << line_count << endl;  
                return false;
            } 
        } // Если строка не содержит #include-директив, то записываем содержимое в выходной файл
        else {
            write_file << line << endl;
        }
    }
    return true;
}


bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    // Если не удалось открыть начальный файл,
    ifstream read_file(in_file.string());
    if (!read_file) {
        return false;
    }

    ofstream  write_file(out_file, ios::app);
    if(!write_file.is_open()) {
        return false;
    }

    return Preprocess(in_file, out_file, in_file.string(), include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}